# Agent-Based Economy Simulation — v1 Design

**Date:** 2026-08-27
**Status:** Approved for implementation planning

---

## 1. What this is

A long-running agent-based simulation of a modern economy, built in C++23.
Individual firms and households follow simple adaptive rules; macroeconomic
behaviour — unemployment cycles, price levels, wealth distribution — emerges
from their interaction rather than being imposed.

This document specifies **v1 only**: a faithful implementation of Lengnick's
baseline model (JEBO 2013), plus the structural decisions that let it grow
into the larger system described in Section 8.

### Why this scope

The project's stated ambition is much larger: regions, supply chains,
multiple labour types, services and information goods, credit, and eventually
a playable layer. That ambition is not abandoned — it is *ordered*.

A previous attempt at this project failed by sprawl. That failure is a known
hazard of the method, not a personal one. Lengnick names it directly:

> "ACE modelers are thus tempted to over-increase the level of complexity...
> the available ACE macro models are often so complex that it is unclear
> which macro pattern is a result of what micro property."

A DSGE modeller is disciplined by having to solve equations. Nobody imposes
that constraint on an agent-based modeller. **This document is the
substitute for that constraint.** v1 is deliberately the smallest model that
produces checkable macro behaviour, so that every later extension can be
added one at a time against a base that is known to work.

### The intellectual frame

Two ideas from the literature anchor the eventual full model, and are
recorded here so later extensions have a rationale rather than being
accreted features:

- **Coase's theory of the firm.** Firms exist because internal coordination
  is sometimes cheaper than market transactions. This determines firm size
  and generates make-or-buy decisions — which is how supply chains will be
  produced endogenously rather than authored.
- **Non-rivalry (Romer).** Intangible assets — software, designs, IP — are
  produced once at fixed cost and used at any scale. This is the source of
  increasing returns and industrial concentration.

Neither appears in v1.

---

## 2. Core design decisions

These four are cheap to adopt now and expensive to retrofit. They apply to
all future versions, not just v1.

### 2.1 Indices, never pointers

Agents live in flat contiguous arrays and reference each other by index
(`uint32_t`), never by pointer or reference.

*Rationale:* pointers between agents break serialization, break population
resizing, and cause cache misses on every access. At the agent counts this
project targets (100k+), memory layout dominates runtime. Indices cost
nothing and keep serialization, resizing, and parallelism available.

### 2.2 Determinism via a single seeded RNG

One seeded RNG, threaded explicitly through every call site that needs
randomness. The same seed produces a byte-identical run, **including the
random order in which agents act**.

*Rationale:* this is non-negotiable for an ABM. Without it, bugs do not
reproduce and scenario comparisons cannot distinguish a real effect from RNG
noise. No hidden global RNG, no `rand()`, no thread-local state.

### 2.3 A `Recorder` seam as the only output path

The simulation core is headless. It knows nothing about rendering, plotting,
or files. It emits records through one `Recorder` interface.

- v1 implementation: CSV writer.
- Later: binary format (planned, not built in v1).
- Later: terminal renderer, then custom graphics.

*Rationale:* a one-way dependency that never reverses. Any frontend is a
separate consumer of output. This makes early throwaway tooling (Python /
matplotlib for validation plots) genuinely disposable — deleting it costs
nothing because nothing depends on it.

### 2.4 Plain structs, no agent polymorphism

Firms and households are plain structs. No inheritance, no virtual
functions.

*Rationale:* future variation (labour types, output classes, firm sectors)
arrives as **fields and enums**, not subclasses. Polymorphic agent
hierarchies are a well-known ABM trap for both performance and complexity.

---

## 3. Module structure

Five modules. Every deferred extension in Section 8 attaches to exactly one
of them; an extension that fits none is a signal to re-examine it.

| Module | Responsibility |
|---|---|
| `world` | Owns the agent arrays and global state. The single source of truth. |
| `agents` | Firm and household structs and their decision rules. |
| `network` | The household↔firm connection graph and rewiring. |
| `scheduler` | The day/month tick order. |
| `recorder` | Output interface and CSV implementation. |

The core must remain runnable headless with no frontend present.

---

## 4. Agents

### Firm

| Field | Meaning |
|---|---|
| `liquidity` | Money held |
| `inventory` | Produced goods in stock, ready to sell |
| `price` | Sale price of its good |
| `wage` | Monthly wage offered |
| `employees` | Indices of employed households |
| `demand_last_month` | Units demanded last month (drives all decisions) |
| `had_unfilled_vacancy` | Whether last month's vacancy went unfilled |
| `months_fully_staffed` | Consecutive months with no open position |

**Inventory is the master signal** — it drives both hiring and pricing.
Firms do not optimize, form expectations, or maximize profit.

### Household

| Field | Meaning |
|---|---|
| `liquidity` | Money held |
| `reservation_wage` | Minimum acceptable wage claim |
| `employer` | Firm index, or sentinel if unemployed |
| `shops` | Fixed-size array of 7 firm indices |
| `demand_unsatisfied` | Per-shop shortfall last month, for rewiring |

Each household inelastically supplies exactly one unit of labour.

### Knowledge is local

Households know only the prices of their 7 connected firms and the wage of
their own employer. Firms know nothing about competitors. **There is no
auctioneer and no market-clearing step.** Every transaction is one named
buyer and one named seller. The law of one price does not hold, and should
not be expected to.

---

## 5. The tick

Time is indexed by **days**, with **21 days to a month**. Goods are bought
daily; labour is contracted monthly. This mismatch is deliberate — it is a
primary source of coordination failure.

### 5.1 Month opens

1. **Wage setting.** Raise `wage` if a vacancy went unfilled last month.
   Lower it if fully staffed for `γ` consecutive months. Adjust
   multiplicatively by `(1 ± υ)`, `υ ~ U[0, δ]`.

2. **Hiring / firing.** Compute inventory bands from last month's demand:
   `i_low = φ_lo · demand_last_month`, `i_high = φ_hi · demand_last_month`
   (`0 < φ_lo < φ_hi`).
   - `inventory < i_low` → open one vacancy.
   - `inventory > i_high` → fire one randomly chosen employee.
   - Otherwise → no change.

   At most one hire and one fire per firm per month. Firing takes effect
   with a one-month lag (job-protection analogue).

3. **Price setting.** Only reconsidered when inventory is outside the same
   bands. Bounded relative to marginal cost:
   `p_min = ϕ_lo · marginal_cost`, `p_max = ϕ_hi · marginal_cost`
   (`1 < ϕ_lo < ϕ_hi`). Adjust by `(1 ± η)`, `η ~ U[0, ϑ]`, and apply the
   new price only with probability `θ < 1` (Calvo-style stickiness).

4. **Household rewiring.**
   - With probability `P_price`: compare one random connected shop against
     one random unconnected firm, the latter drawn with probability
     proportional to firm size (employee count — a proxy for visibility).
     Switch if the new price is at least `ξ` percent lower.
   - With probability `P_quant`: if demand went unsatisfied last month, drop
     one such shop — chosen with probability proportional to the shortfall —
     and replace it with a random firm.

5. **Job search.** Search intensity varies by status:
   - Unemployed: visit up to `β` random firms. Accept an open position if
     its wage exceeds current income.
   - Employed but underpaid (`wage < reservation_wage`): visit one random
     firm with probability 1.
   - Employed and satisfied: visit one random firm with probability `π < 1`.

   In all cases a position is accepted only if it pays more than the
   current one.

6. **Consumption budget.** Planned real monthly consumption:

   ```
   c = min( (m / P)^α , m / P )      0 < α < 1
   ```

   where `P` is the average price across the household's 7 connected firms.
   The `min` enforces the budget constraint. Saving is purely
   precautionary; there is no interest rate.

### 5.2 Each day (×21)

1. **Shopping.** Households act in random order. Each visits one randomly
   chosen connected firm and attempts to buy `c / 21`.
   - Limited by household liquidity → buy `m / p`.
   - Limited by firm inventory → buy what remains. Inventory never goes
     negative.
   - Unmet demand is retried at other connected firms, stopping after `n`
     attempts or once 95% of planned demand is met. Any remainder is lost.

2. **Production.** Each firm produces linearly in labour:
   `output = λ · employee_count`, added to inventory.

### 5.3 Month closes

1. Pay wages: firm liquidity falls by `wage × employees`; each employee's
   liquidity rises by `wage`.
2. Retain a buffer proportional to the wage bill: `buffer = χ · wage ×
   employees`.
3. Distribute all remaining liquidity to households as profit, **in
   proportion to each household's current liquidity**.

Step 3 is the model's inequality engine: wealth begets wealth. It is
retained deliberately.

---

## 6. Success criteria

v1 is correct when the following emerge with **no exogenous shocks applied**:

1. **Endogenous unemployment fluctuation.** Unemployment rises and falls on
   its own. Nothing in the model schedules this.
2. **A Phillips curve.** Negative relationship between unemployment and
   wage inflation in the recorded output.
3. **A Beveridge curve.** Negative relationship between unemployment and
   vacancies.
4. **Wealth dispersion.** Starting from an equal endowment, the household
   liquidity distribution spreads and stays spread.

Additionally:

5. **Determinism.** Identical seed → byte-identical output across runs.
6. **Burn-in independence.** Results after the burn-in period do not depend
   on the randomly seeded initial network. If they do, that is a bug.

Criteria 1–4 are the reason to trust every later extension. They are not
academic formality — they are the only available evidence that the engine
works rather than merely runs.

---

## 7. Explicitly out of scope for v1

Each is deferred with a reason, not forgotten:

| Excluded | Reason |
|---|---|
| Firm-to-firm trade | Requires firms to buy inputs; a much larger firm model. This *is* the supply chain — the first major extension. |
| Multiple goods | v1 has one homogeneous consumption good. |
| Regions, transport cost | Needs multiple goods first to be meaningful. |
| Capital goods, investment | The K+S growth mechanism; needs a second sector. |
| Credit, banks, central bank | Requires a working real economy underneath. |
| Government, taxation, transfers | Policy layer; needs a stable base to measure against. |
| Labour types | Requires production complexity that v1 lacks. |
| Services / information goods | Requires multiple goods and regions. |
| Growth, population change | Fixed populations keep v1 interpretable. |
| Player interaction, magic setting | Application layer, far downstream. |
| Custom graphics | Frontend, behind the `Recorder` seam. |

---

## 8. Extension map

Recorded so future work has a route rather than an impulse. Rough dependency
order; each attaches at a named module.

1. **Firm-to-firm trade** → `network`, `agents`. Makes the graph
   non-bipartite. Supply chains emerge from make-or-buy decisions
   (Coase). The single largest step.
2. **Multiple goods** → `world`, `agents`. Prerequisite for most of what
   follows.
3. **Regions + transport cost** → `world`, `network`. Transport cost is the
   discriminator that makes output classes behave differently and drives
   regional specialization. `std::mdspan` is the natural fit for a
   region × good price matrix.
4. **Output classes** (goods / services / information goods) → `agents`.
   Distinguished by rivalry and storability: goods are rival and storable,
   services rival and non-storable (infinite transport cost), information
   goods non-rival with near-zero marginal cost.
5. **Labour types** (production / intangible-investment / coordination /
   market-making) → `agents`. Each has a distinct scaling law; the
   white-collar share should emerge rather than be set.
6. **Firm-initiated connections** → `network`. Firms reaching out to
   households instead of only receiving them. This is the precise home of
   sales and marketing — the "star mechanic" of the earlier game concept.
7. **Credit and net worth** → `agents`, new `bank` module. Bankruptcy
   cascades (CATS) produce realistic firm-size power laws.
8. **Binary recorder, terminal renderer, custom graphics** → `recorder` and
   separate frontend. Independent of everything above.

---

## 9. Technical decisions

| Decision | Choice | Note |
|---|---|---|
| Language | C++23 | `std::mdspan` for future region×good matrices; `std::print`/`std::format` for the terminal renderer. |
| Compiler | GCC 16.2.1 | Verified present. |
| Build | CMake 4.4.2 | Verified present. |
| Output | CSV via `Recorder` | Binary format deferred; the seam makes the swap local. |
| Validation plots | Python / matplotlib | **Throwaway.** Not installed yet. Nothing depends on it. |
| Initial scale | 1000 households, 100 firms | Lengnick's figures, for comparability. Architecture targets 100k+. |

---

## 10. Open items

- Parameter values (`γ, δ, φ_lo, φ_hi, ϕ_lo, ϕ_hi, ϑ, θ, α, β, π, ξ, χ, λ,
  n, P_price, P_quant`) to be taken from Lengnick's published table during
  implementation, then held fixed until the success criteria are met.
- Burn-in length: to be determined empirically; start at 500 months and
  confirm initial-condition independence.

---

## References

- Lengnick, M. (2013). *Agent-based macroeconomics: A baseline model.*
  Journal of Economic Behavior & Organization 86, 102–120. — **the v1 model**
- Dosi, G., Fagiolo, G., Roventini, A. (2010). *Schumpeter meeting Keynes.*
  JEDC 34(9). — capital goods, R&D, endogenous growth
- Delli Gatti, D. et al. *Business fluctuations in a credit-network economy.*
  — credit, bankruptcy cascades, firm-size power laws
- Coase, R. (1937). *The Nature of the Firm.* — coordination cost, firm boundaries
- Romer, P. (1990). *Endogenous Technological Change.* — non-rivalry, increasing returns
