# economy — agent-based macroeconomic simulation

C++23 implementation of Lengnick's baseline ABM (JEBO 2013, "Agent-based
macroeconomics: A baseline model"). 1000 households, 100 firms, no central
market-clearing. Macro behaviour emerges from local interaction.

Long-horizon project. v1 is deliberately the smallest model that produces
checkable macro behaviour; extensions are ordered, not optional.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # Release is the default
./build/economy_tests                              # 93 cases
./build/economy --seed 1 --burn-in 1000 --months 6000 --out output/run.csv
python3 scripts/validate.py output/run.csv         # stdlib only, no deps
```

`scripts/plot.py` needs matplotlib + pandas, which are NOT installed and
cannot be pip-installed here (no pip module; Arch blocks it under PEP 668).
Use `validate.py` — it prints every number the success criteria need.

## Architecture

| File | Responsibility |
|---|---|
| `params.hpp` | All constants. Split into published calibration vs. invented initial conditions — respect that boundary. |
| `agents.hpp` | `Firm` and `Household` plain structs, `NO_FIRM`/`NO_HOUSEHOLD` sentinel |
| `world.{hpp,cpp}` | Owns the agent arrays; `total_money()` |
| `rng.{hpp,cpp}` | Deterministic seeded RNG, hand-rolled conversions |
| `network.{hpp,cpp}` | Household↔firm links (employment, shop connections) |
| `firm_decisions.{hpp,cpp}` | Monthly: wage, hire/fire, price |
| `household_decisions.{hpp,cpp}` | Monthly: shop search, job search, consumption budget |
| `daily.{hpp,cpp}` | Daily: shopping, then production |
| `month_close.{hpp,cpp}` | Wages, profits, reservation wages, deferred fires, demand rollover |
| `scheduler.{hpp,cpp}` | The day/month tick |
| `recorder.{hpp,cpp}` | `Metrics` + `Recorder` interface + `CsvRecorder` |
| `main.cpp` | CLI. The only file permitted console I/O. |

### The tick

```
firm_decisions::run_monthly       wage, hire/fire marks, price
household_decisions::run_monthly  shop search, job search, consumption budget
settle_vacancy_bookkeeping        a still-open vacancy went unfilled
21 x daily::run_day               shopping (random shop order), then production
month_close::run                  wages, profits, reservation wages, fires, rollover
```

Ordering is load-bearing and subtle:
- `settle_vacancy_bookkeeping` runs *after* job search — that is when a
  vacancy is either taken or left standing.
- `pending_fire` is marked at month open and applied at month close, so a
  worker produces for the whole month. That is the one-month firing lag.
- `adjust_wage` runs before `adjust_workforce`, so it reads last month's
  `vacancy_went_unfilled` before this month's reset.
- `unsatisfied` is consumed by shop rewiring, then cleared, then
  re-accumulated during the 21 days.

## Invariants — do not break these

1. **Money conservation.** `sum(household.liquidity) + sum(firm.liquidity)`
   is constant. Currently drifts 3e-9 over 7000 months. If this breaks, it
   is the highest-priority bug; do not loosen a tolerance to hide it.
   Likeliest culprits, in order: `daily::shop`, `month_close::pay_wages`,
   `month_close::distribute_profits`.
2. **Determinism.** Same seed → byte-identical run, including randomized
   agent order. All randomness flows through an explicitly passed `Rng&`.
   No `rand()`, no globals, no `std::random_device` outside seeding.
   `Rng::uniform`/`index`/`shuffle` are hand-rolled *on purpose* — the
   standard distributions are not portable across stdlib versions, which
   would silently invalidate recorded baselines. Do not "simplify" them
   back to `std::uniform_int_distribution` / `std::shuffle`.
3. **Indices, never pointers,** between agents. Sentinel `0xFFFFFFFFu`.
4. **No virtuals on agent types.** `Recorder` is the sole exception.
   Variation arrives as fields and enums, never subclasses.
5. **Core does I/O only through `Recorder`.** No `std::cout` in `src/`
   outside `main.cpp`.
6. Build warning-clean under `-Wall -Wextra -Wpedantic`.

## Model rules that look like bugs but are not

- Firms **cut wages rather than go bankrupt** when the wage bill is
  unaffordable (Lengnick §2.4). This is what keeps firm count constant with
  no entry/exit. Do not add bankruptcy.
- **Profits are distributed proportional to household liquidity** — the
  wealthy receive more. Deliberate stock-ownership proxy; it is the model's
  inequality engine. Do not equalize it.
- **Reservation wage never falls** when an employer cuts pay; the household
  searches harder instead. It falls 10% only while unemployed.
- **Price is gated, not clamped**: raised only while below the cap, lowered
  only while above the floor. Clamping instead turns a raise into a large
  cut when price sits above cap.
- **Demand is recorded as *wanted*, not bought**, and telescopes across
  retry visits within a day. Each firm records the demand it personally
  faced; no firm reads the sum. Recording attempted demand is what makes a
  stocked-out firm expand.
- Firms do not optimize, forecast, or maximize profit. Simplicity is the
  design.

## Status

v1 complete and merged. 93 tests, 196,083 assertions.

**Three of four success criteria pass. Criterion 1 fails — this is the open
question.**

| Criterion | Result |
|---|---|
| Endogenous unemployment fluctuation | **FAIL** — mean 0.80%, max 2.7%, autocorrelation gone by lag 12 |
| Phillips curve | PASS (weak) — −0.111 |
| Beveridge curve | PASS — −0.339 |
| Wealth dispersion | PASS — Gini 0.0265, no collapse |

### The open issue

Aggregate demand (~72–88k units/month) structurally exceeds productive
capacity (~62.5k = 1000 workers × 3/day × 21 days), so firms can never
build inventory. 58 of 100 hold an open vacancy at all times and any
unemployed household finds work almost immediately. Unemployment is pinned
by construction.

Diagnosed as **calibration, not a code defect**. The suspects are the three
initial conditions in `params.hpp` that Lengnick does not specify and that
were invented for this implementation. They are mutually inconsistent with
the model's own price band: at `kInitialWage = 30`, marginal cost is 0.476
and the price band is [0.488, 0.548], but `kInitialPrice = 1.0` — roughly
double its own cap. The price gate then forbids all increases while above
cap, so prices only decay, dragging wages and the cap down with them.

Cheapest experiment: set `kInitialPrice` to ~0.50 and re-run (8 seconds).
Next lever is `kInitialHouseholdLiquidity` — total money supply is what sets
demand against a fixed productive capacity.

Already ruled out: the shopping-retry bug fix moved mean vacancies 57.4 →
58.1, i.e. not the cause.

**Do not tune published calibration constants to make a criterion pass.**
That is the failure mode the spec exists to prevent. The initial conditions
are fair game — they were invented, and `params.hpp` marks which is which.

## Next work, in dependency order

From spec §8. Resolve criterion 1 first — extensions built on a model whose
labour market cannot move will be very hard to evaluate.

1. **Firm-to-firm trade** → `network`, `agents`. Makes the graph
   non-bipartite. This *is* the supply chain; it emerges from make-or-buy
   decisions (Coase) rather than being authored. Largest single step.
2. **Multiple goods** → `world`, `agents`. Prerequisite for most of the rest.
3. **Regions + transport cost** → `world`, `network`. Transport cost is what
   makes output classes behave differently and drives specialization.
   `std::mdspan` suits a region × good matrix.
4. **Output classes** (goods / services / information goods) → `agents`.
   Distinguished by rivalry and storability.
5. **Labour types** (production / intangible-investment / coordination /
   market-making) → `agents`. White-collar share should emerge, not be set.
6. **Firm-initiated connections** → `network`. Firms reaching out to
   households. This is where sales/marketing lives.
7. **Credit and net worth** → new `bank` module. Bankruptcy cascades (CATS)
   produce realistic firm-size power laws.
8. **Binary recorder, terminal renderer, custom graphics** → `recorder` plus
   a separate frontend. Independent of everything above.

## Documents

- `docs/superpowers/specs/2026-08-27-agent-based-economy-design.md` — the
  design and the binding scope decisions. §7 is what is deliberately out of
  scope and why; §8 is the extension map; §11 is the measured v1 results.
- `docs/superpowers/plans/2026-08-27-lengnick-baseline-v1.md` — the
  implementation plan, including Lengnick's full Table 1 calibration.
- `DESIGN.md` — the originating concept: a magic-capitalism tycoon/grand-
  strategy game. This simulation is the economic substrate that design
  silently assumed. Retained deliberately.

## Known deferred items

Real but judged not worth fixing yet. Reconsider when touching that code.

- `network` has no bounds asserts on its index parameters.
- `daily.cpp`'s cost clamp does not reduce `bought`, so at machine-epsilon
  scale a firm can ship marginally more than paid for. Conservation
  unaffected.
- `household_decisions::run_monthly` has no direct test.
- `month_close`'s `total_household_liquidity <= 0` branch is unreachable by
  invariant and untested.
- Per-day `vector + iota + shuffle` allocates ~147k times per reference run.
  Irrelevant at 1k agents; matters at the 100k target.
- `month_close::run` takes no `Rng&`. Credit extensions will need one.
