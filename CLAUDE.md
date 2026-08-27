# economy — agent-based macroeconomic simulation

C++23 implementation of Lengnick's baseline ABM (JEBO 2013, "Agent-based
macroeconomics: A baseline model"). 1000 households, 100 firms, no central
market-clearing. Macro behaviour emerges from local interaction.

Long-horizon project. v1 is deliberately the smallest model that produces
checkable macro behaviour; extensions are ordered, not optional.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # Release is the default
./build/economy_tests                              # 95 cases
./build/economy --seed 1 --burn-in 1000 --months 6000 --out output/run.csv
python3 scripts/validate.py output/run.csv         # stdlib only, no deps
```

`scripts/plot.py` needs matplotlib + pandas. There is no system `pip` module,
but that is not a blocker — `venv` + `ensurepip` are present, so a local
environment needs no sudo and touches nothing system-wide:

```bash
python3 -m venv .venv                                   # .venv/ is gitignored
.venv/bin/python -m pip install matplotlib pandas
.venv/bin/python scripts/plot.py output/run.csv         # *.png + index.html
```

(An earlier version of this file claimed matplotlib "cannot be installed
here." That was wrong — it conflated "no pip module" with "unavailable".
`extra/python-matplotlib` is also in the Arch repos.)

`scripts/plot.py` writes four figures: `criteria.png` (the four success
criteria), `cycle.png` (the four cycle series on one time axis),
`crosscorr.png` (lead-lag against unemployment) and `demand.png` (planned
consumption vs recorded demand vs capacity). The cycle and demand figures are
windowed on the most volatile 240 months — at 6000 months the cycles overplot.

`scripts/report.py` writes `output/index.html` — the review page, deliberately
written for a non-economist. It leads with a one-sentence verdict, then states
each of the five checks as a plain claim about the simulated world ("when jobs
are scarce, pay stops rising"), with the academic name and the measured number
demoted to a footnote under it. The figures follow, each with a caption saying
what to look for and decoding the jargon printed inside the image. A raw-numbers
table and a glossary sit at the bottom. Copying `output/` copies the whole
review, page and figures together.

Keep that register when editing it. The prose lives in the `CHECKS`, `FIGURES`
and `GLOSSARY` tables at the top of the file; the per-run wording and the
thresholds are in `check_rows`. `headline` special-cases a money-conservation
failure — it says stop reading, because invariant 1 outranks every economic
result on the page.

Two thresholds there are judgement calls, not spec: unemployment spread ≥ 0.01
(the line between the failing 0.0047 and passing 0.0285 runs recorded below),
and "the wealth gap does not close" read as late ≥ half of early.

Stdlib-only, so it runs without the venv; `plot.py` calls it at the end of its
own run, and a figure that has not been drawn is named on the page rather than
shown as a broken image. Household and firm counts are read out of
`src/params.hpp` so they cannot drift.

```bash
python3 scripts/report.py output/run.csv    # page alone, no venv needed
xdg-open output/index.html                  # read it
```

`scripts/validate.py` remains the stdlib-only path that needs no environment
at all, and prints every number the success criteria need. Its `load` and
`compute` are the single source of truth for the criteria math — `report.py`
imports them, so the page and the terminal cannot drift. Add a criterion there,
not in two places.

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
| `recorder.{hpp,cpp}` | `Metrics` + `Recorder` interface + `CsvRecorder`. Records demand-side series too: `demand`, `planned_consumption`, `inventory`, `household_liquidity`. |
| `main.cpp` | CLI. The only file permitted console I/O. |

Analysis lives in `scripts/`: `validate.py` (criteria math, stdlib),
`report.py` (HTML review page, stdlib), `plot.py` (figures, matplotlib).

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
   is constant. Drift is 0 over the 7000-month reference run (spread 2e-9). If this breaks, it
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
  no entry/exit. Do not add bankruptcy. The cut is *bounded* — see
  `kMaxEmergencyWageCut`/`kMinWage` and the Status section; an unbounded cut
  reaches zero, and zero is absorbing.
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

v1 complete and merged. 95 tests, 196,090 assertions.

**All four success criteria pass** (seed 1, burn-in 1000, 6000 months;
confirmed on seeds 2/3/4).

| Criterion | Before fix | After fix |
|---|---|---|
| Endogenous unemployment fluctuation | FAIL — sd 0.0047, max 3.3% | **PASS** — mean 1.7%, sd 0.0285, max 12.6% |
| Phillips curve | −0.111 | **−0.445** |
| Beveridge curve | −0.339 | **−0.842** |
| Wealth dispersion (Gini) | 0.0265 | **0.102**, rising, no collapse |

Unemployment autocorrelation now persists to lag 36 (−0.36) instead of
dying by lag 12.

### Resolved: the zero-wage absorbing state

Criterion 1 failed because **67 of 100 firms held a wage of exactly 0.0**,
permanently. The chain:

1. `distribute_profits` strips every firm to `buffer = 0.1 x wage_bill`, so a
   firm enters payday holding 10% of its payroll.
2. A firm that sells poorly hits the no-bankruptcy rule in `pay_wages` and
   cut its wage to `liquidity / headcount` — a ~90% cut, repeated monthly,
   geometric to zero.
3. **Zero is absorbing.** Every recovery channel is multiplicative
   (`f.wage *= (1.0 + mu)`), so `0` stays `0` forever. And `bill = 0` means
   the emergency branch never fires again either.
4. `marginal_cost == 0` collapses the price band to `[0, 0]`, so those firms
   could never raise a price again — the "frozen price" attractor.
5. Zombie firms have zero labour cost, so they cannot be outcompeted and
   never downsize. Two-thirds of the market was inert, which is why the
   labour market could not move. They also paid out all revenue as profit
   (buffer = 0.1 x 0 = 0), pumping household liquidity — the "excess demand"
   previously blamed as root cause was a *symptom* of this.

**Fix:** `pay_wages` still pays out everything the firm holds (that is the
paper's rule and what conserves money), but the *posted* wage is now bounded
below by `kMaxEmergencyWageCut` (50% max cut per month) and `kMinWage`. The
shortfall is arrears, not a wage the firm can never climb back from.

Note for future diagnosis: the earlier reading — that `kInitialPrice = 1.0`
sat above its cap and was the root cause — was an **averaging artifact**.
"Mean wage 16.3" was 67 zeros mixed with 33 healthy firms at ~53, and the
"price 2.3x its cap" was mostly zombies whose cap is zero. Initial conditions
do not survive the 1000-month burn-in; changing `kInitialWage` to 58 moved
the criteria by nothing. **Check per-firm distributions before trusting an
aggregate.**

### Demand: stable at the household, volatile at the firm

Measured post-burn-in over 6000 months:

| Series | Mean | CV |
|---|---|---|
| `planned_consumption` (units households intend to buy) | 63,335 | **4.3%** |
| `demand` (units firms are asked for) | 81,720 | **26.6%** |
| monthly capacity | 61,908 | 2.9% |
| `household_liquidity` | 94,790 | 0.4% |

Household demand is **stable** and sits just above capacity. The volatile
`demand` series is not want — it is the retry/telescoping mechanism counting
one underlying want at several firms, so it rises when goods are scarce
(corr with `inventory` = −0.77). Do not read `demand` as consumer appetite.

Inventory is now non-zero in 6000/6000 months (previously pinned at zero),
which is what lets the price-cut and layoff branches fire at all.

The cycle, by cross-correlation against unemployment over ±24 months
(negative lag = leads): `planned_consumption` −0.79 at −12mo, `inventory`
+0.90 at −5mo, `vacancies` −0.85 at +1mo, `mean_wage` **+0.62 at −17mo**.
Consumption softens, inventory piles up, firms fire, unemployment peaks.

Search the full ±24 window: an earlier table searched only ±9 and reported
local extrema as peaks. `mean_wage`'s strongest relation is positive and
*leading* (the boom bids wages up before the squeeze); the Phillips-type
negative appears as −0.52 at +9mo. The cycle is ~55 months, so −17 and +38
alias — trust the sign, treat the lag as approximate.

### Why unemployment averages 1.7%, not Lengnick's 5%

Not a defect — an accounting identity. 33.2 fires/month × a ~0.5-month spell
= 17 households = 1.7%. Spells are short because 45.6 of 100 firms hold an
open vacancy, so β=5 search finds one 95.2% of the time.

**Tested and rejected** (test, don't reason — see the averaging-artifact
lesson above):
- Demand telescoping inflating inventory targets → record only the first
  visit of the day, removing all 25.7% duplication: **1.73% → 1.73%**.
- Search too effective → sweep β = 5,3,2,1: **1.62% → 2.10%**. Nearly flat.

**Actual cause: v1 has no demand leakage.** Closed circular flow, fixed money
stock, no credit/investment/government ⇒ aggregate demand ≡ aggregate output
in steady state. The price level floats until real demand equals capacity:
planned consumption 63,335 vs full-employment output 63,000, i.e. the economy
wants ~1005 workers and has 1000. So 70 firms/month want to hire vs 7 wanting
to fire. Unemployment is **frictional by construction**; deficient demand is
not representable in v1.

**Peak capped at 12.6% by the layoff throttle.** One fire per firm per month
(faithful — spec §5.1), but a firing firm wants to shed **4.61 workers** on
average, 47.7% of the time more than one. Plus the one-month lag.

Conjecture: mean and range are the same question (unemployment is floored at
0), and the likely route to both is a demand leakage — the credit/bank
extension. Untested.

## Next work, in dependency order

From spec §8. Criterion 1 is resolved, so the labour market now moves and
extensions can be evaluated against it.

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
