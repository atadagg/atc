# economy — agent-based macroeconomic simulation

C++23 implementation of Lengnick's baseline ABM (JEBO 2013, "Agent-based
macroeconomics: A baseline model"). 1000 households, 100 firms, no central
market-clearing. Macro behaviour emerges from local interaction.

Long-horizon project. v1 is deliberately the smallest model that produces
checkable macro behaviour; extensions are ordered, not optional.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # Release is the default
./build/economy_tests                              # 205 cases
./build/economy --seed 1 --burn-in 1000 --months 6000 --out output/run.csv
python3 scripts/validate.py output/run.csv         # stdlib only, no deps

./build/economy --scenario scenarios/stimulus.scenario --out output/s.csv
```

A **scenario** is a save file and a reproducible experiment: `(seed +
intervention log)`, a few lines of text rather than a state dump. `--scenario`
loads one; an explicit `--seed` overrides the file's. `scenarios/stimulus.scenario`
is the worked example — one line creates money and the CLI then reports the two
numbers separately:

```
books  start=100000 end=100000 drift=0.000000002  (must be flat)
supply start=100000 end=150000 issued=50000.000000
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
`model_agents`, `model_dist`, `model_time`, `model_steering`), separate from
how it is drawn.
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

**Steering is implemented** (`panel_steering.cpp` + `model_steering`). Sliders
for all 21 behavioural parameters, a button that issues money to every
household, the audit trail of what has been done, and a scenario save.

The rule that makes it safe: a slider **schedules** rather than applies. The
edit is appended to `App::log` for the month about to open, and takes effect
there, before any agent acts. So the slider keeps reading the value the
simulation is actually using until the month turns — which looks like a bug and
is not, and the panel says so on screen. This is what keeps invariant 2 intact:
the run is (seed + log), so scrubbing back through a change and forward again
replays it identically.

Committing happens on `IsItemDeactivatedAfterEdit`, not per frame — a drag
would otherwise append hundreds of interventions for one gesture.

`App` holds no graphics types and lives in `viewer_core`, so the steering flow
is tested without a window (`tests/test_viewer_app.cpp`): scheduled-not-applied,
scrub-back-through-a-change, issuance expanding the supply while the books stay
flat. The panels themselves remain untested; they only draw.

Loading a scenario is CLI-only (`--scenario`) — the viewer can save but not
load, because `Sim` takes its log at construction and reloading means
restarting the run.

## Architecture

| File | Responsibility |
|---|---|
| `params.hpp` | Structural `constexpr` (layout, initial conditions, run config) vs. the behavioural `Params` struct that lives on `World`. Respect that boundary. |
| `agents.hpp` | `Firm` and `Household` plain structs, `NO_FIRM`/`NO_HOUSEHOLD` sentinel |
| `world.{hpp,cpp}` | Owns the agent arrays and the issuer accounts; `money_in_circulation()` (a measurement) and `books_balance()` (the assertion) |
| `ledger.{hpp,cpp}` | `transfer` — the only code permitted to write a balance, and the only place solvency is enforced; `Kind`/`may_issue`, `Journal`, `Entry`, `Reason` |
| `rng.{hpp,cpp}` | Deterministic seeded RNG, hand-rolled conversions |
| `network.{hpp,cpp}` | Household↔firm links (employment, shop connections) |
| `firm_decisions.{hpp,cpp}` | Monthly: wage, hire/fire, price |
| `household_decisions.{hpp,cpp}` | Monthly: shop search, job search, consumption budget |
| `daily.{hpp,cpp}` | Daily: shopping, then production |
| `month_close.{hpp,cpp}` | Wages, profits, reservation wages, deferred fires, demand rollover |
| `intervention.{hpp,cpp}` | The intervention log: `Params` table (name, accessors, valid range), `Intervention`, `Log`, `apply` |
| `scenario.{hpp,cpp}` | `Scenario` = seed + log; stream read/write. Streams, not paths — the core opens no files. |
| `scheduler.{hpp,cpp}` | The day/month tick; applies due interventions at month open |
| `recorder.{hpp,cpp}` | `Metrics` + `Recorder` interface + `CsvRecorder`. Records demand-side series too: `demand`, `planned_consumption`, `inventory`, `household_liquidity`. Also `CsvJournal`, the ledger's file sink. |
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

1. **Every event balances.** Every movement of money has a source and a sink
   and nets to exactly zero. True **by construction**: `ledger::transfer` is
   the only code that writes `liquidity`, and it does one subtraction and one
   addition of the same `double`. Do not reintroduce a paired mutation
   anywhere — a new module that moves money calls `transfer` or it is wrong.
   **This invariant has no exceptions**, and none may be added: money is never
   created by a movement, only by issuance, which is itself a balanced
   movement out of an issuer account. If a feature seems to need an exception,
   it is missing an issuer, not entitled to one.

   *Corollary, true only while the model has no issuers* — which Lengnick v1
   is: the sum of all balances is constant. Drift is 0 over the 7000-month
   reference run (spread 3e-9, which is `total_money()`'s own summation error,
   not movement). **This is a property of the model, not of the engine.** Do
   not write it into the engine, and do not assume it survives the bank
   extension. See `docs/superpowers/specs/2026-08-31-ledger-and-journal-design.md`
   Part 2 for the issuer design and the solvency enforcement that goes with it.

   Enforced, not merely documented: `transfer` rejects a movement that would
   push a **non-issuer** below zero (`ledger::Overdraft`). A completed run is
   therefore *proof* that no account went negative, rather than a run that was
   not checked. Issuer accounts (`World::issuers`) may go negative freely —
   that negative number is the money they have issued, and v1 has none.

   If the books ever fail to balance, it is the highest-priority bug; do not
   loosen a tolerance to hide it.
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

   3 and 4 are **correctness properties, not optimizations** — do not trade
   them for convenience thinking the cost is only speed. They are what keeps
   agents trivially copyable, which is what makes `SimSnapshot` a plain
   assignment and therefore what makes the viewer's backward scrub
   byte-exact (see the comment on `SimSnapshot` in `viewer/sim.hpp`). Cache
   locality is a bonus that starts mattering at the 100k target; at 1k
   agents it is irrelevant. A pointer or a virtual on `Firm` costs replay,
   scrubbing and save-games, not milliseconds.
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

v1 complete and merged. 205 tests, 437,351 assertions.

**Criterion 1 fails; 2, 3 and 4 pass** — now confirmed across **20 seeds**, not
inferred from one; see "Criteria across 20 seeds" below, which also corrects
the direction of criterion 4's trend. The table below is seed 1 and is kept
because it is what the rewiring-bug comparison was measured on.

This is a *correction* of an earlier "all four pass" — see "Resolved: the
size-weighted draw on the rationed path" below. The earlier pass was produced
by a network bug, not by the economics.

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

### Done: the `transfer` chokepoint (step 1 of the ledger spec)

`ledger::transfer(World&, Account from, Account to, double amount)` is now the
only code that writes `liquidity`. `Account` is a tagged index
(`{Kind::Firm|Household, uint32_t}`, invariant 3). The three paired-mutation
sites are migrated: `daily::shop`, `month_close::pay_wages`,
`month_close::distribute_profits`. Overdraft is deliberately *not* policed in
`transfer` — that is a caller policy, and credit will introduce accounts that
legitimately go negative.

### Done: journaling (step 2 of the ledger spec)

`transfer` takes an optional `Reason` and `ledger::Journal*`. A non-null
journal receives one `Entry` per movement — `{month, day, seq, from, to,
amount, reason}` — *after* the movement succeeds, so a rejected transfer
records nothing. `seq` is monotonic over the whole run and gives every entry
a total order. `day` is `ledger::kMonthClose` (−1) for wages and profits,
which move outside any day.

The clock is stamped by the scheduler (`Journal::set_time`), not by
`transfer` — `transfer` does not know what day it is, and should not.
`Scheduler::set_journal` attaches and detaches between months.

`Journal` is **not** part of `World`, and therefore not part of
`SimSnapshot`: a journal is an output, and outputs go through a sink the core
does not own (invariant 5). `VectorJournal` (in-memory, for tests) lives in
`ledger`; `CsvJournal` (the I/O half) lives in `recorder`.

Journaling off leaves the run **byte-identical** — verified over the full
6000-month reference run.

```bash
./build/economy --seed 1 --burn-in 50 --months 2 \
    --out output/run.csv --journal output/journal.csv --journal-months 1
```

Journaling defaults to a **one-month window** and never covers the burn-in,
for a reason worth knowing before you widen it: a single month of the
reference population produced 122,174 entries, and **100,000 of them were
profit distributions**. `distribute_profits` pays every firm's profit to every
household, so it is O(firms x households) per month — 100 x 1000 — against
~21,000 purchases and ~1,000 wage payments. Journaling 6000 months means
~700M rows. Journal a window.

That ratio is itself a finding: almost all recorded money movement in this
economy is the ownership proxy, not trade.

### Done: issuers and enforced solvency (Part 2 of the ledger spec)

The engine no longer assumes a fixed money stock.

- `Kind` gains `Issuer`; `may_issue(Kind)` is the single `constexpr` answer to
  "which accounts may create money". Solvency is a property of the *kind*, not
  a per-account flag, so the answer is one function rather than an audit.
- `World::issuers` is a bare `std::vector<double>` — accounts with no agent
  behind them. Empty in v1. A bank extension gives them behaviour; the
  invariant needs only the balances.
- `transfer` rejects a movement that would overdraw a non-issuer
  (`ledger::Overdraft`). A rejected transfer moves nothing and journals
  nothing.
- `total_money()` split in two, and the distinction is the point:
  `money_in_circulation()` (non-issuers — a **measurement**, it moves when
  money is issued) and `books_balance()` (everything — an **assertion**, it
  never moves). Identical today. The CSV column keeps the name `total_money`
  because `scripts/` read it.

**The distribution-tail fix, which had to come first.** Instrumentation
measured **16,953** sub-zero balances per reference run — 5,838 via wages,
11,115 via profits, worst −1.435e-12, none on households. Naive enforcement
would have rejected all of them. Both loops now pay the **last** recipient the
exact remaining balance rather than a separately computed share, so a firm
lands on exactly `0.0` or exactly its buffer. This also removes the
machine-epsilon residue introduced with the chokepoint.

Why profits were the larger half: an empty firm has a zero wage bill, hence a
zero buffer, hence distributes its *entire* balance — precisely the case where
N computed shares overshoot the pot. ~16% of firms are empty in a given month.

Results (seed 1, 6000 post-burn-in months), against the pre-Part-2 baseline:

| | before | after |
|---|---|---|
| unemployment mean / sd | 0.0033 / 0.0040 | 0.0037 / 0.0046 |
| Phillips | −0.444 | −0.459 |
| Beveridge | −0.624 | −0.645 |
| Gini mean | 0.0142 | 0.0151 |
| books spread | 3e-9 | 3e-9 |
| wall clock | 14.31 s avg | **13.67 s avg** |

Same rounding-path story as the chokepoint commit: the trajectory moved, the
economics did not. **Enforcement cost nothing measurable** — the run is if
anything marginally faster, well inside the spec's 2% budget, despite a branch
on all ~855M transfers. Determinism re-verified.

### Done: the intervention log (Part 4 of the ledger spec)

**A run is now (seed + intervention log), not seed alone.** That is what lets a
parameter change mid-run without breaking invariant 2, and it is the blocker
that kept viewer steering unimplemented.

- `intervention::Log` is append-only, kept sorted by `(month, seq)`. `seq` is
  monotonic across the whole log and is load-bearing: two changes in the same
  month need a defined order, or nondeterminism returns at a finer grain than
  the one the log exists to remove.
- `Scheduler::run_month` applies everything due **at month open, before any
  agent acts**. Applying mid-month would make the outcome depend on where in
  the tick the change landed.
- Three actions: `SetParam`, `Issue` (issuer → one household),
  `IssueToAllHouseholds`. An injection is an ordinary balanced transfer with
  `Reason::Issuance`, so it appears in the journal like anything else and needs
  **no exception to invariant 1**.
- `Sim` takes an optional log and passes it to each throwaway `Scheduler`,
  along with the month — `Sim` builds a fresh `Scheduler` per step, so without
  `set_month` the log and the journal would both see month 0 forever. That was
  a latent journal-timestamp bug under the viewer; it is fixed.

**Parameters are addressed by a table, not a parallel enum.**
`intervention::param_specs()` is one `constexpr` array of
`{name, set, get}`, and a parameter id is an index into it. There is
deliberately no second list of ids to drift out of sync — a drifted entry would
silently steer the *wrong knob* rather than fail to compile. Names are the
stable identity for a future save file; the index is the compact in-memory
form. Integer parameters convert once, inside the table row.

**Every `World` now has one issuer account**, balance zero, standing for what
is outside the modelled economy. v1 never touches it, so the money supply stays
constant. It exists so that money creation needs no new machinery when a bank
arrives, and so the injection path is testable today.

The scrub test is the one that matters: set two parameters at month 60, run to
80, scrub back to 40 (parameters revert — they travel in the snapshot), scrub
forward to 80 again, and the state is **byte-identical** to the first pass.

No interventions leaves the reference run byte-identical, at 13.3 s. (Discard
the first timed run after a rebuild — it reads ~18 s cold.)

### Done: scenarios as save files

`Scenario` is `(seed + intervention log)` and `scenario::read`/`write` move it
through a **stream, never a path** — the core opens no files (invariant 5), so
`main` and the viewer own the handle and the parser is testable without disk.

The format is one record per line, `#` comments allowed, and it stays readable
a year later because parameters are addressed by **name**:

```
economy-scenario 1
seed 1
3000 issue_all 0 50
```

Parsing is deliberately strict — unknown parameter, unknown action, wrong
arity, bad number, unknown version all throw rather than being skipped. A
scenario silently half-read is a run that is not the run anyone asked for, and
this is a reproducibility tool.

**`seq` is renumbered on read.** The file is written in `(month, seq)` order and
re-read in that order, so intra-month order — the only kind that decides
anything — survives, while absolute `seq` values need not match. There is a test
pinning exactly this.

Measured end to end with `scenarios/stimulus.scenario`: circulation steps
100,000 → 150,000 at the scheduled month, the books stay flat to 3e-9, and
unemployment drops from ~1.9% to ~0% over the following two months. Without a
scenario the reference run is byte-identical.

### Done: viewer steering, and parameter ranges

Each `ParamSpec` now carries the range of values that **mean** anything — a
probability above 1 is nonsense, not merely unusual — and `Log::set_param`
rejects anything outside it. The bound belongs with the parameter rather than
with whichever UI is drawing a slider: a scenario file is hand-written, and
that is where a typo lands.

The steering panel is the first consumer of the whole substrate. See the Viewer
section for the scheduled-not-applied rule and why the slider appears to snap
back.

### Resolved: the negative distribution remainder (seeds 5 and 7)

This was listed as a deferred item reading *"could in principle round
marginally negative... did not occur once in 7000 months. Left unclamped
deliberately."* **It occurs.** Seeds 5 and 7 abort mid-run:

```
./build/economy --seed 5 --burn-in 337 --months 1 --out /dev/null
terminate called after throwing an instance of 'std::invalid_argument'
  what():  ledger::transfer: amount must be finite and non-negative
```

The tail fix absorbed overshoot in one direction only. 999 proportional shares
can sum to marginally *more* than the pot, leaving the firm a hair **below** its
buffer and the last remainder negative (measured: −1.12e-12 against a pot of
562). A household whose entitlement is exactly zero — no liquidity of its own —
is maximally exposed, because it has no share of its own to absorb the error
accumulated across the other 999.

**Fix:** every payment is capped by what the firm can actually part with
(`capped_payment`), not just the last one. In the ordinary case the cap never
binds — the reference run is byte-identical. It also closes a second latent
failure that had not been hit: for a firm whose retained buffer is zero,
an overshoot would overdraw the account outright and trip `Overdraft`.

The loudness was kept where it earns its place. `check_landed_on_target`
asserts after each distribution that the firm is sitting on its target balance,
so a crumb is absorbed while a real discrepancy in the shares still throws.
That is the distinction the original "loud beats silent" reasoning missed:
there are two magnitudes, and only one of them is a bug.

**The lesson, and it is the recurring one.** "Did not occur once in 7000
months" was seven thousand months of *one seed*. Seeds 1–4 are all clean, so
even a spot check would have missed it; it took a 20-seed ensemble to surface.
**One seed is not a sample** — the same shape as the averaging artifact and
the rewiring bug above.

### Criteria across 20 seeds

Single-seed numbers had been carrying more weight than they could bear. The
ensemble (`scripts/ensemble.py --seeds 20`, 6000 post-burn-in months each):

| Quantity | mean | sd | min | max |
|---|---|---|---|---|
| unemployment mean | 0.0034 | 0.0003 | 0.0025 | 0.0038 |
| unemployment sd | 0.0041 | 0.0004 | 0.0030 | 0.0047 |
| unemployment max | 0.0295 | 0.0035 | 0.0240 | 0.0370 |
| Phillips | −0.4379 | 0.0332 | −0.4928 | −0.3326 |
| Beveridge | −0.6202 | 0.0176 | −0.6453 | −0.5718 |
| Gini mean | 0.0145 | 0.0007 | 0.0123 | 0.0156 |
| books spread | 2.26e-9 | 7e-10 | 1.11e-9 | 4.00e-9 |

Criteria 2 and 3 hold on **20/20** seeds. Criterion 1 fails on all of them —
the low mean and narrow range are structural, not a seed artifact, which
strengthens the demand-leakage explanation.

**Correction to criterion 4.** Seed 1 shows the wealth gap *narrowing* (early
0.0168 → late 0.0121) and this file said so. Across 20 seeds it **widens**:
early 0.0137 → late 0.0152, and late ≥ half of early on 20/20. The falling
trend was a one-seed artifact. Criterion 4 passes more convincingly than
previously recorded.

### Measured performance baseline

Seed 1, burn-in 1000, 6000 measurement months, Release, this machine:

| Quantity | Measured |
|---|---|
| Wall clock | **14.3 s** (14.14 / 14.48 / 14.32) |
| Money movements per month | 122,174 |
| ⇒ transfers per reference run | ~855 million |
| Of which profit distribution | ~82% |

Anything added to `transfer` runs ~855 million times per reference run, so
cost is a requirement rather than an afterthought — the budget is in the
spec's Part 5. The 82% figure is a share of *movements*, not a profiled share
of runtime; do not optimize on it without profiling first.

### Done: structural vs. behavioural parameters (step 3 of the ledger spec)

`params.hpp` is now two piles, and the boundary is load-bearing:

| Pile | Form | Examples |
|---|---|---|
| Structural | `constexpr` in `namespace params` | `kNumHouseholds`, `kNumFirms`, `kShopsPerHousehold`, `kDaysPerMonth` |
| Initial conditions | `constexpr` in `namespace params` | `kInitialWage`, `kInitialPrice`, … |
| Run configuration | `constexpr` in `namespace params` | `kBurnInMonths`, `kMeasurementMonths` |
| Behavioural | `struct Params`, one per `World` | every rate, threshold and probability the model consults while running |

The test for the boundary: **could a UI slider move it mid-run?** Structural
constants fail it because they define memory layout — `kShopsPerHousehold` is
the size of the array inside every `Household`, and making it runtime state
would end trivially-copyable agents and with them the byte-exact backward
scrub (invariants 3 and 4). Initial conditions fail it for a different
reason: they are consumed once at construction, several as default member
initializers in `agents.hpp`, so "change it at month 300" is not a meaningful
operation.

`Params` lives on `World`, which is why it costs almost no plumbing —
everything already takes `World&` — and why it is **already inside
`SimSnapshot`**: the snapshot copies the whole `World`. Restoring month 400
therefore restores the parameters as of month 400, which is spec detail #1,
satisfied structurally rather than by remembering to. `Params` must stay a
plain aggregate for the same reason `SimSnapshot` must stay a plain
assignment.

Three functions that took only a `Firm` now take a `const Params&` as well:
`marginal_cost`, `inventory_band`, `adjust_wage`, `adjust_price`.

One behavioural parameter needed a guard. `max_shop_visits_per_day` (the
paper's *n*) is a loop bound over a fixed-size array of shop slots, so
`daily::shop` clamps it to `kShopsPerHousehold`: a raised *n* cannot visit
shops the household does not have. At the calibrated 7 the clamp is a no-op.

The 6000-month reference run is **byte-identical** across this change.

**The reference baseline moved, and this is why.** The spec's acceptance test
was byte-identical output. It is not byte-identical, for a reason that is
arithmetic and not behavioural: paying 10 workers used to be one debit of the
aggregate `bill` followed by ten credits; true double-entry is ten paired
debit/credits. Those differ in the last bits of a `double`, and the system is
chaotic, so 6000 months amplifies ~3e-9 at month 0 into a visibly different
CSV. Evidence it is a rounding path and not an economics change (seed 1,
6000 post-burn-in months):

| | before | after |
|---|---|---|
| unemployment mean / sd | 0.0033 / 0.0039 | 0.0033 / 0.0040 |
| Phillips | −0.404 | −0.444 |
| Beveridge | −0.622 | −0.624 |
| Gini mean (early → late) | 0.0141 (0.0168 → 0.0122) | 0.0142 (0.0168 → 0.0144) |
| money spread | 2e-9 | 3e-9 |

Determinism (invariant 2) re-verified: same seed, byte-identical repeat runs.

Consequence to remember: `pay_wages` can leave a firm holding a
machine-epsilon residue rather than exactly zero, because `headcount` debits
of `per_worker` do not sum to exactly `bill`. That is a rounding remainder,
not unpaid wages. Do not "fix" it by restoring an aggregate debit — that
reintroduces the convention the chokepoint removes.

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

## Next work

### 0. Substrate, before the extensions

**`transfer` chokepoint and the intervention log.** Not economics — the two
pieces that make everything after them cheaper, and both get harder once more
modules move money. See
`docs/superpowers/specs/2026-08-31-ledger-and-journal-design.md`.

Parts 1–4 of the spec are **done** (see Status): the `transfer` chokepoint, journaling,
the `constexpr`/`Params` split, issuers with enforced solvency, and the
intervention log. The substrate is bank-ready: an extension that creates money
needs an issuer account and nothing else from that document. What remains:

- ~~**Viewer steering UI**~~ — **done**, see the Viewer section.
- **Loading a scenario in the viewer.** `Sim` takes its log at construction,
  so this means a `Sim::reset(seed, log)` and clearing the snapshot history.
  Small, but it is a new entry point into the run and deserves its own tests.
- ~~**Log serialization**~~ — **done**: `scenario.{hpp,cpp}`, `--scenario`.

### Extensions, in dependency order

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
- `docs/superpowers/specs/2026-08-31-ledger-and-journal-design.md` — **the
  financial substrate**: the `transfer` chokepoint and the journal (both
  built), the balanced-events invariant with issuer accounts and enforced
  solvency (designed), and the intervention log (designed). Part 0 separates
  what belongs to the engine from what belongs to Lengnick v1; Part 5 is the
  measured performance budget. Opens in plain language — read that section
  first if the accounting vocabulary is unfamiliar.
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
- `App::log` is appended to from the panel while `Sim` holds a `const Log*`,
  and `Log::due` hands out a span into the log's storage. Safe today because
  appends happen between steps on one thread; it would not survive stepping the
  simulation off the UI thread.
