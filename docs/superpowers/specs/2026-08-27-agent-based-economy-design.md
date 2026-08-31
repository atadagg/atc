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
   - **Unaffordable wage bill.** If `wage × employees` exceeds the firm's
     liquidity, the firm does not go bankrupt. Instead — Lengnick 2013
     §2.4 — `wage` is permanently cut to `liquidity / employees` (the most
     the firm can pay), and the bill is recomputed at that reduced wage
     before liquidity is debited. This is a real behavioural rule, not a
     bookkeeping clamp: the cut `wage` value persists into next month, so it
     also feeds `firm_decisions::marginal_cost` (and hence next month's
     price-band floor/cap), next month's `adjust_wage` raise/cut decision,
     and any employee's reservation wage via
     `month_close::update_reservation_wages`. It is what keeps the firm
     count constant with no entry or exit.
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

---

## 11. v1 results (2026-08-27, re-measured after the zero-wage fix)

Measured with `./build/atc --seed 1 --burn-in 1000 --months 6000`
(1000 households, 100 firms, unmodified Table 1 calibration, no exogenous
shocks) and `scripts/validate.py`. Money conservation: drift `0.0`, spread
`2.0e-9` over the whole run — the master invariant holds throughout.
Confirmed on seeds 2, 3 and 4; determinism re-verified byte-for-byte.

**All four criteria now pass.** The numbers below supersede every earlier
version of this section.

| # | Criterion | Before | After | Verdict |
|---|---|---|---|---|
| 1 | Endogenous unemployment fluctuation | mean 0.80%, sd 0.44pp, max 2.7%; autocorrelation ≈0 by lag 12 | mean 1.73%, sd 2.85pp, max 12.6%; autocorrelation −0.36 still at lag 36 | **PASS** |
| 2 | Phillips curve | −0.111 | **−0.445** | PASS |
| 3 | Beveridge curve | −0.339 | **−0.842** | PASS |
| 4 | Wealth dispersion (Gini) | mean 0.0265, falling 0.032 → 0.022 | mean 0.102, rising 0.067 → 0.106 | PASS |

### 11.1 Root cause of the criterion-1 failure

**67 of 100 firms held a wage of exactly `0.0`, permanently.** The chain:

1. `month_close::distribute_profits` strips every firm to
   `buffer = χ × wage_bill` (χ = 0.1), so a firm meets payday holding 10% of
   its own payroll.
2. A firm that sells poorly triggers Lengnick's no-bankruptcy rule (§2.4) in
   `month_close::pay_wages`, which set `wage = liquidity / headcount` — a
   ~90% cut. Repeated monthly, this is geometric decay to zero.
3. **Zero is an absorbing state.** Every wage-recovery channel in
   `firm_decisions::adjust_wage` is multiplicative (`wage *= (1 + μ)`), so
   `0` stays `0` forever. `bill = 0 × headcount = 0` also means the
   emergency branch never fires again — the state is sealed.
4. `marginal_cost = wage / (λ·21) = 0` collapses the price band to `[0, 0]`.
   Since the raise gate requires `price < cap`, those firms could never
   raise a price again. This is the "frozen price level" observed earlier.
5. A firm with zero labour cost cannot be outcompeted and never downsizes.
   Two thirds of the market was inert — which is precisely why the labour
   market could not move. Zombie firms also had `buffer = 0.1 × 0 = 0`, so
   they distributed *all* revenue as profit each month, inflating household
   liquidity.

Measured over 6000 months, the wage process ran 324,962 raises (mean
+0.94%), 5,524 tenure cuts (mean −0.95%) and 2,754 emergency cuts — of
which **67 landed exactly on zero**, one per zombie firm.

### 11.2 The fix

`pay_wages` still pays out everything the firm holds — that is the paper's
rule and what conserves money — but the *posted* wage is now bounded below
by `kMaxEmergencyWageCut` (50% maximum cut per month) and `kMinWage`. The
unpaid remainder is arrears, not a wage the firm can never climb back from.
Both constants are invented for this implementation and live in the
invented-constants section of `params.hpp`; neither is a Table 1 value, and
no published constant was touched. They have **not** been sensitivity-swept.

### 11.3 Correction to the earlier diagnosis

The previously recorded diagnosis — that `kInitialPrice = 1.0` sat at roughly
double its own price cap and that this was a calibration problem — was an
**averaging artifact**. "Mean wage 16.3" was 67 zeros averaged with 33
healthy firms at ~53; "price 2.3× its cap" was mostly zombie firms whose cap
is zero by construction. The proposed remedy was also directionally wrong:
lowering `kInitialPrice` raises real balances and worsens excess demand.
Setting `kInitialWage = 58` (tested) moved the criteria by nothing, because
initial conditions do not survive the 1000-month burn-in.

Likewise, the chronic vacancy glut (~58 of 100 firms) and the excess of
aggregate demand over productive capacity were **symptoms**, not causes:
both follow from zombie firms distributing 100% of revenue as profit.

Methodological lesson, recorded because it cost two diagnostic cycles:
**check per-firm distributions before trusting an aggregate.** A mean over a
bimodal population describes neither mode.

### 11.4 Demand dynamics (measured)

`Metrics` now records `demand`, `planned_consumption`, `inventory` and
`household_liquidity`. Post-burn-in, over 6000 months:

| Series | Mean | Coefficient of variation |
|---|---|---|
| `planned_consumption` (units households intend to buy) | 63,335 | **4.3%** |
| `demand` (units firms are asked for) | 81,720 | **26.6%** |
| `inventory` | 21,091 | 90.7% |
| `household_liquidity` | 94,790 | 0.4% |
| monthly capacity (λ × employed × 21) | 61,908 | 2.9% |

Two things follow. First, **household demand is stable and very close to
productive capacity** (63.3k planned vs 61.9k capacity, CV 4.3%) — the
demand side is not the volatile part of this economy. Second, the gap
between `planned_consumption` and `demand` is search friction, not want:
a household turned away restocks its visit elsewhere, and *each* firm
records the full amount wanted, so a single underlying want is counted
several times. `demand`'s 26.6% CV is therefore largely a **stockout
amplifier** — it rises when goods are scarce and falls when they are
plentiful (corr(`demand`, `inventory`) = −0.77). This is the mechanism
described in §5.2 working as designed, and it is why `demand/capacity` reads
1.32 while `planned_consumption/capacity` reads ≈1.02.

Critically, `inventory` is now non-zero in **6000 of 6000 months** (it was
structurally pinned at zero before the fix). Firms can hold stock, which is
the precondition for the price-cut and layoff branches to fire at all.

### 11.5 The cycle

Cross-correlation against unemployment (negative lag = series leads):

| Series | Peak correlation | At lag |
|---|---|---|
| `planned_consumption` | −0.79 | −12 months |
| `inventory` | +0.90 | −5 months |
| `vacancies` | −0.85 | +1 month |
| `mean_wage` | +0.62 | −17 months |

Searched over ±24 months. An earlier version of this table searched only ±9
and therefore reported local extrema as peaks; the `mean_wage` row is the one
that materially changes. The strongest wage relation is **positive and
leading** — wages are high about 17 months *before* unemployment peaks, which
reads as the boom bidding wages up until the squeeze arrives. The
Phillips-type negative relation is real but secondary, appearing as −0.52
around +9 months. Note the cycle is roughly 55 months long, so a −17 lead and
a +38 lag are the same alignment; treat the sign as informative and the exact
lag as approximate.

This is a coherent business cycle: consumption softens, inventory accumulates
about seven months later, firms respond by cutting vacancies and firing,
unemployment peaks, and wages fall afterwards. Falling wages and prices then
raise real balances (M/p), demand recovers, inventory drains, and hiring
resumes. The Phillips relation (criterion 2) shows up as the negative wage
correlation at positive lags rather than as a contemporaneous one.

Criteria 2 and 3 strengthened substantially (−0.111 → −0.445 and −0.339 →
−0.842). Unlike the previous fix wave, this is not attributable to noise
correlation: unemployment's range widened from 0–2.7% to 0–12.6%, so the
relationships are now measured over an informative span.

### 11.6 Why the mean is 1.7% and the peak 12.6%

Lengnick reports unemployment roughly 0-20% with a mean near 5%. This run
gives 0-12.6%, mean 1.73%. The gap decomposes; it is not a mystery.

**The mean is an accounting identity.** Unemployment is a stock fed by a
flow, and the two measured quantities multiply out to the observed level:

| Quantity | Measured |
|---|---|
| Fires per month (instrumented `network::fire`) | 33.2 |
| Firms holding an open vacancy | 45.6 of 100 |
| P(unemployed finds one in β=5 visits) = 1−(1−0.456)^5 | 95.2% |
| Implied mean spell | ≈0.5 months |
| **Implied unemployment** | 33.2 × 0.5 = 17 households = **1.7%** (observed 1.73%) |

**Two plausible explanations were tested and rejected.** Both were tested
rather than reasoned about, per §11.3's lesson:

| Hypothesis | Test | Result |
|---|---|---|
| Telescoped demand inflates inventory targets, so firms over-hire | record demand only on the day's first shop visit, removing all 25.7% duplication | mean 1.73% → 1.73%, **no effect** |
| Job search too effective at β=5 | sweep β = 5, 3, 2, 1 | mean 1.62% → 2.10%, **nearly flat** |

**The actual reason: v1 has no demand leakage.** It is a closed circular
flow with a fixed money stock, no credit, no investment and no government.
In steady state all income is spent and all spending is income, so aggregate
demand is identically aggregate output — there is no channel by which demand
can sit below capacity for a sustained period. The price level is the free
variable and floats until real demand equals full-employment output:

- planned consumption: **63,335** units/month
- full-employment output: **63,000** units/month (1000 × 3 × 21)
- implied labour requirement: **~1,005 workers** against a workforce of 1,000

Hence 70 firm-months per month want to hire against 7 wanting to fire — a
10:1 ratio. Unemployment in v1 is **frictional by construction**: the residue
of firms and workers not finding each other within a month, never deficient
demand. A 5% mean requires demand to fall below capacity and stay there, and
nothing in v1 can produce that.

**The peak is capped by the layoff throttle.** A firm may fire at most one
worker per month (§5.1, faithful to the paper). Instrumented at the decision
point:

| When a firm wants to… | Mean workers it wants to move | Share needing >1 |
|---|---|---|
| fire | 4.61 | 47.7% |
| hire | 2.53 | 37.0% |

Nearly half of all firing decisions are throttled, the typical one about
fourfold. With the one-month firing lag on top, a contraction that "wants"
to shed 300 workers bleeds them out over many months while hiring elsewhere
refills the pool. Note this constraint is **faithful** — Lengnick operates
under it too — so it explains this run's ceiling, not the gap to his numbers.

**Conjecture, untested.** Because unemployment is floored at zero, a higher
mean and a wider range are the same question: a more volatile series has a
higher mean. The most likely route to both is a demand leakage — credit,
saving not immediately respent, or investment — letting aggregate demand
fall below capacity. That is extension 7 (the bank module) in §8. This is a
hypothesis about where to look, not a finding.

### 11.7 Still open

- `kMaxEmergencyWageCut` and `kMinWage` are unswept invented constants.
- Matching Lengnick's amplitude, per §11.6 — most likely needs a demand
  leakage, i.e. the credit/bank extension.
- The deferred items listed in `CLAUDE.md` are unchanged.
