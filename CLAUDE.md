# economy — agent-based macroeconomic simulation

C++23 implementation of Lengnick's baseline ABM (JEBO 2013, "Agent-based
macroeconomics: A baseline model"). 1000 households, 100 firms, no central
market-clearing. Macro behaviour emerges from local interaction.

Long-horizon project. v1 is deliberately the smallest model that produces
checkable macro behaviour; extensions are ordered, not optional.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # Release is the default
./build/economy_tests                              # 144 cases
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

(This file once claimed matplotlib "cannot be installed here." That was
wrong — it conflated "no pip module" with "unavailable". Verify environment
claims before repeating them.)

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
and "the wealth gap does not close" read as late ≥ half of early. The 0.01 was
drawn when a run measured 0.0285 — but that run is now known to have been
inflated by the rewiring bug (see Status). The honest reading today is that
the threshold is unvalidated and the current 0.0039 fails it by a wide
enough margin that the exact line does not matter.

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

### Viewer

```bash
cmake -S . -B build && cmake --build build -j   # builds economy_viewer if SDL3 is present
./build/economy_viewer
```

A live SDL3 + ImGui front-end (ImGui vendored at `third_party/imgui/`,
`-w` on that target only — it does not have to satisfy our warning settings).
The viewer owns the `World` and ticks it in its own frame loop, one frame at
a time, rather than driving `main.cpp`'s CLI path. Scrubbing backward is
exact re-simulation from the nearest retained snapshot (every 50 months),
never interpolation — the same determinism (invariant 2) that makes a run
reproducible from a seed is what makes a scrubbed-to month byte-identical to
having run straight there. Testable viewer logic — `Sim`'s snapshot/seek,
`RingRecorder`, histogram binning — lives in `viewer_core`, separate from the
ImGui panels, and is covered by `tests/test_viewer_*.cpp`.

What the viewer *shows* is decided in a view-model layer (`viewer/cell.hpp`,
`model_agents`, `model_dist`, `model_time`), separate from how it is drawn.
Panels render `Cell`s and never reach into `Firm`/`Household` themselves:
filters, sorting, the column schema, the detail panel's link graph and the
scrub bounds are all display-independent and tested without a window. A
`Cell` carries a raw number for every kind — that number is the sort key —
and never a pre-formatted string, so a future skin (ASCII, 2D, 3D) reuses
the decisions and reimplements only the drawing. `Missing` is a cell kind,
not a sentinel: an unemployed household's employer must never reach a skin
as `0xFFFFFFFF`, which would print as 4294967295 and sort above every real
employer. Line grouping and label wording stay in the panel — those are
display concerns. See
`docs/superpowers/specs/2026-08-30-viewer-view-model-design.md`.

Steering (live parameter mutation from the UI) is deliberately not
implemented: `params.hpp`
is `constexpr`, and mutating simulation parameters mid-run needs its own
answer for determinism (an intervention log, most likely) before it can be
built without quietly breaking invariant 2.

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
- **The two shop-rewiring paths draw differently, on purpose.** Price
  comparison draws the challenger weighted by firm size
  (`random_unconnected_firm_by_size` — visibility), because an explicit price
  test then decides the switch. The rationed path draws unweighted
  (`random_unconnected_firm`), because it applies no price test and a
  weighting there is unopposed. Do not unify them — see Status.
- Firms do not optimize, forecast, or maximize profit. Simplicity is the
  design.

## Status

v1 complete and merged. 144 tests, 227,856 assertions.

**Criterion 1 fails; 2, 3 and 4 pass** (seed 1, burn-in 1000, 6000 months;
confirmed on seeds 2/3/4). This is a *correction* of an earlier "all four
pass" — see "Resolved: the size-weighted draw on the rationed path" below.
The earlier pass was produced by a network bug, not by the economics.

| Criterion | With the rewiring bug | Corrected |
|---|---|---|
| Endogenous unemployment fluctuation | "PASS" — sd 0.0285, max 12.6% | **FAIL** — mean 0.33%, sd 0.0039, max 2.8% |
| Phillips curve | −0.445 | **−0.404** |
| Beveridge curve | −0.842 | **−0.622** |
| Wealth dispersion (Gini) | 0.102, rising | **0.0141**, early 0.0168 → late 0.0122 |

Criterion 4 still passes as coded (non-zero, does not collapse to half of
early) but the dispersion is an order of magnitude smaller and trending
down; do not read it as a strong result.

Criterion 1 is therefore **open**, and back to roughly where it was before
the zero-wage fix. The conjecture below — that v1 cannot represent
deficient demand, and needs the credit/leakage extension — is now the
leading explanation rather than one of two.

### Resolved: the size-weighted draw on the rationed path

**66 of 100 firms held no employees at all**, 54 of them pinned at exactly
`kMinWage`, with prices ~0.08 against the survivors' ~1.02 — the cheapest
firms in the economy could not attract a customer. 922 of 1000 households
were employed, concentrated 13–29 per firm in the ~34 survivors, against
Lengnick's ~10.

This was the zero-wage zombie state relocated, not resolved: `kMinWage`
removed the arithmetic absorbing point at 0 and the firms simply piled up
on the floor instead.

Root cause: `search_reliable_shop` drew its replacement with
`random_unconnected_firm_by_size` (weight `1 + employees`). Spec §4 gives
the size weighting to the **price-comparison** path only (`P_price`, where
it stands for visibility and an actual price test decides the switch); the
rationed path (`P_quant`) is specified as "replace it with a random firm".
Size-weighted, that path is unopposed — no price is consulted at all — so a
household turned away by a large firm was routed to another large firm ~30x
as often, and a firm that had lost its staff could never regain the
customers that would justify rehiring. Being small was self-perpetuating.

The loop, traced on firm 0: no customers → demand ~30 units/month → hire one
worker → that worker makes 63 units → inventory exceeds the upper band →
fire → wage cut at the next unaffordable payroll → drift back to the floor.

**Fix:** `network::random_unconnected_firm` (unweighted), used by
`search_reliable_shop`. Firms at the floor 54 → **0**, firms employing
66 → **90**, employed households 922 → **998**.

Post-fix the firm population churns instead of stratifying, measured over
6000 post-burn-in months: **no firm is empty in more than 90% of months, no
firm is never empty, and all 100 reach 10+ staff at some point**. A firm is
empty 16% of months on average and recovers. At month 7219, 1000 of 1000
households are employed with 42 vacancies open — the empty firms are
mid-turnover, not stranded. Staff still bunch (~14 per staffed firm against
Lengnick's ~10) because one worker produces 63 units/month and hiring is
therefore chunky; that is the model's arithmetic, not a defect.

**And it was propping up the criteria.** The cycling and the wealth
dispersion that criteria 1 and 4 were measuring were substantially two
thirds of the firms dying and half-reviving on a loop. Removing it makes
the economy healthy and the criteria worse — see the table above. Correct
beats passing.

Lesson, alongside the averaging-artifact one below: **a criterion that
starts passing after a fix is not thereby validated.** Check what mechanism
is producing the number.

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
| `planned_consumption` (units households intend to buy) | 62,972 | **0.5%** |
| `demand` (units firms are asked for) | 69,932 | **5.9%** |
| monthly capacity | 62,794 | 0.4% |
| `household_liquidity` | 95,081 | 0.2% |

Household demand is **stable** and sits essentially at capacity. The volatile
`demand` series is not want — it is the retry/telescoping mechanism counting
one underlying want at several firms, so it rises when goods are scarce
(corr with `inventory` = −0.91). Do not read `demand` as consumer appetite.

Inventory is non-zero in 6000/6000 months (once pinned at zero), which is
what lets the price-cut and layoff branches fire at all.

The cycle, by cross-correlation against unemployment over ±24 months
(negative lag = leads): `planned_consumption` −0.76 at −24mo, `inventory`
+0.77 at −3mo, `vacancies` −0.63 at +1mo, `mean_wage` −0.65 at +15mo.
Consumption softens, inventory piles up, firms fire, unemployment peaks.

Search the full ±24 window: an earlier table searched only ±9 and reported
local extrema as peaks. `planned_consumption`'s extremum sits at the window
edge (−24), so its true lag is outside ±24 — trust the sign, treat the lag
as approximate.

### Why unemployment averages 0.33%, not Lengnick's 5%

Not a defect — an accounting identity, re-measured after the rewiring fix
(seed 1, 6000 post-burn-in months):

| Quantity | Measured |
|---|---|
| separations | 2.63 /month |
| mean unemployment spell | 1.24 months |
| ⇒ households unemployed | 3.3 = **0.33%** |
| firms holding an open vacancy | 29.4 of 100 |

Spells are short because with 29.4% of firms hiring, β=5 search finds a
vacancy ~83% of the time.

**Tested and rejected** (test, don't reason — see the averaging-artifact
lesson above). Both were measured *before* the rewiring fix, against a 1.7%
mean; neither depended on firm concentration, so the conclusions should
hold, but they have not been re-run:
- Demand telescoping inflating inventory targets → record only the first
  visit of the day, removing all 25.7% duplication: **1.73% → 1.73%**.
- Search too effective → sweep β = 5,3,2,1: **1.62% → 2.10%**. Nearly flat.

**Actual cause: v1 has no demand leakage.** Closed circular flow, fixed money
stock, no credit/investment/government ⇒ aggregate demand ≡ aggregate output
in steady state. The price level floats until real demand equals capacity:
planned consumption 62,972 vs full-employment output ~63,000, i.e. the
economy wants about as many workers as it has. Unemployment is **frictional
by construction**; deficient demand is not representable in v1.

**Peak capped at 2.8% by the layoff throttle** (12.6% with the rewiring
bug). One fire per firm per month (faithful — spec §5.1), plus the
one-month lag. How many workers an over-stocked firm *would* shed if
unthrottled has not been re-measured since the fix; the 4.61 quoted here
before is a pre-fix number and was removed rather than carried forward.

Conjecture: mean and range are the same question (unemployment is floored at
0), and the likely route to both is a demand leakage — the credit/bank
extension. Untested.

## Next work, in dependency order

From spec §8. Criterion 1 is **open** again since the rewiring fix (see
Status), and the leading explanation is that v1 cannot represent deficient
demand at all. That makes item 7 — credit, the first real demand leakage —
the one most likely to move it, though the ordering below is by dependency,
not by priority.

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
   a separate frontend. Independent of everything above. The viewer's
   view-model layer is the seam a second frontend plugs into: it should
   reimplement drawing only, never re-derive filters, schemas or the link
   graph.

## Documents

- `docs/superpowers/specs/2026-08-27-agent-based-economy-design.md` — the
  design and the binding scope decisions. §7 is what is deliberately out of
  scope and why; §8 is the extension map; §11 is the measured v1 results.
- `docs/superpowers/plans/2026-08-27-lengnick-baseline-v1.md` — the
  implementation plan, including Lengnick's full Table 1 calibration.
- `docs/superpowers/specs/2026-08-28-economy-viewer-design.md` and
  `docs/superpowers/plans/2026-08-28-economy-viewer.md` — the live SDL3 +
  ImGui front-end: snapshot/seek, the recorder ring, the panels.
- `docs/superpowers/specs/2026-08-30-viewer-view-model-design.md` and
  `docs/superpowers/plans/2026-08-30-viewer-view-model.md` — the viewer's
  view-model layer: what it separates and why a second skin reuses it.
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
- `draw_cell`'s `col == 0` special case (identity column is selectable) is a
  schema fact living in the panel. Reordering a column schema would silently
  move row selection. A `bool identity` on `ColumnSpec` is the fix, deferred
  as speculative until a second skin exists.
- `panel_agents.cpp` looks fields up by label string; a typo between
  `model_agents.cpp` and the panel would print `0` rather than fail to
  compile. `constexpr` shared label constants would make it a build error.
- The scrub bar's call site (`scrub_bounds(app.max_month)`) is not itself
  unit-tested — panels need a window. The rule it encodes is tested.
