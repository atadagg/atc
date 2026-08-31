# atc_viewer — a live inspection and steering front-end

Status: design approved, not yet implemented.
Supersedes nothing. `scripts/report.py` and `scripts/validate.py` keep their
jobs; see "Relationship to the existing analysis scripts".

## 1. Why

The current front-end is `scripts/report.py`, which renders a static HTML page
from a finished run's CSV. That page is a *report*: it answers "did this run
pass the four criteria" after the fact, for a reader who is not an economist.
It is good at that and is not being replaced.

It cannot answer the questions that actually drive the work:

- *Which* firms are failing, and since when? (The zero-wage diagnosis needed
  per-firm state; the CSV holds only means.)
- What does the wage distribution look like, rather than its mean? Two
  diagnoses in this project's history went wrong on aggregates — see the
  "averaging artifact" note in CLAUDE.md. The tool should make the
  distribution the default view and the mean the derived one.
- What does this economy *look like* while it runs?

The loop today is edit → rebuild → simulate 7000 months → re-read a CSV in
another language. By the time anything is drawn, the `World` no longer exists.

This is §8 item 8 of the design spec ("binary recorder, terminal renderer,
custom graphics"), narrowed: no binary recorder, no terminal renderer, and
the graphics are an inspection instrument rather than a presentation surface.

## 2. Scope

In scope, in build order:

- **Phase B — the agent table.** Every firm and household, sortable and
  filterable; select one to see its full field state and a recent-history
  sparkline.
- **Phase A — distributions.** Histograms over agents for the fields that
  aggregates hide.
- **Phase C — time and scrubbing.** Time-series charts, and a scrub bar that
  puts phases B and A at any month of the run, showing real state.

Out of scope for this document:

- **Steering** (live parameter mutation). Every value in `src/params.hpp` is
  `inline constexpr`; making them mutable is a core refactor threading a
  runtime `Params` through every decision module, and it collides with that
  file's "published calibration — do not tune" boundary. It also needs its own
  answer for reproducibility. It is the intended next phase and the design
  below must not preclude it, but it is specified separately.
- **Watching** in the sense of a spatial rendering of the agent network.
  Also intended, also later; phase B's data access is its prerequisite.

## 3. Architecture

A second executable, `atc_viewer`, linking the existing `atc_core`
unchanged. The CLI (`economy`), the tests, and both Python scripts are
untouched by this work.

### 3.1 The viewer owns the World and ticks it in the frame loop

Measured: the reference run is 7000 months in 9.7s — 1.4 ms/month. A 60 Hz
frame is 16 ms. Advancing one month per frame costs 9% of a frame's budget.

So the viewer holds `World`, `Rng` and `Scheduler` directly and calls the
scheduler from its own loop. There is no simulation thread, no snapshot type
copied per frame, and no serialization boundary. The UI reads the live agent
arrays. Speed is "months advanced per frame", with zero meaning paused.

Rejected alternatives:

- *Worker thread with double-buffered snapshots.* Buys throughput that is
  already 10x surplus, at the cost of a copy per frame, a snapshot type to
  keep in sync with `agents.hpp`, and a threading bug surface. The one case
  that wants a background run — a long batch — is what the CLI already does.
- *Replaying a recorded binary log.* Instant scrubbing and clean decoupling,
  but the result is inert: a log cannot be steered, and it forces an up-front
  decision about which fields are worth recording. That is precisely the
  CSV's current failure mode, in a new format.

### 3.2 Invariants

- **Invariant 5 (core does I/O only through `Recorder`) holds.** The viewer
  is not in `src/`; it lives in `viewer/`. Nothing in `src/` gains a print.
  The time series reaches the charts through a `Recorder` implementation
  (`RingRecorder`, §4.3), not through a new core hook.
- **Invariant 2 (determinism) holds** as long as the viewer only reads
  `World`. The UI must never mutate agent state. Steering, when it arrives,
  restores determinism by logging interventions and replaying them; that is
  why it is not smuggled in here.
- **No new virtuals on agent types** (invariant 4): the viewer reads plain
  struct fields.

### 3.3 Directory layout

```
viewer/
  main.cpp          window, frame loop, tick pacing
  app.{hpp,cpp}     viewer state: World, Rng, Scheduler, RingRecorder, selection
  panel_agents.cpp  phase B
  panel_dist.cpp    phase A
  panel_time.cpp    phase C
third_party/imgui/  vendored, beside doctest.h
third_party/implot/ vendored (see §6 risk)
```

`atc_core` gains nothing. `CMakeLists.txt` gains an `atc_viewer`
target, guarded by an `ATC_BUILD_VIEWER` option defaulting to ON when
SDL3 is found, so a machine without SDL3 still builds and tests the core.

## 4. Components

### 4.1 Phase B — the agent table

Two tables, firms and households, using ImGui's sortable table API.

Firm columns: index, wage, price, inventory, headcount, liquidity, months
since last wage change, vacancy open, demand last month.
Household columns: index, employer, liquidity, reservation wage, consumption
budget, shop connections, unsatisfied.

Filtering is a predicate over the row, entered as a few fixed toggles rather
than a query language: "wage at floor", "vacancy open", "unemployed",
"inventory zero". These are named after the pathologies already known to
matter; more get added when a new pathology is found, which is the honest
way this list grows.

Selecting a row opens a detail panel: every field of that agent, plus its
network links as clickable indices, so a household's employer navigates to
the firm.

The detail panel's sparklines read the ring buffer (§4.3), so on first build
they cover the recent window only. Full history arrives with phase C.

### 4.2 Phase A — distributions

Histograms over agents, drawn for the currently displayed month: firm wage,
price, inventory, headcount, liquidity; household liquidity and reservation
wage. Each histogram prints its mean and its count-at-extremes underneath —
the mean is shown as a derived annotation on the distribution, never as the
primary number. Bin count is adjustable; the axis does not auto-rescale per
frame, because a jumping axis makes a moving distribution unreadable.

Clicking a bin filters the phase B table to the agents in it. That is the
path from "there is a spike at zero" to "these 67 firms", in one click.

### 4.3 Phase C — time and scrubbing

**Time series.** `RingRecorder : Recorder` keeps the last N months of
`Metrics` in a fixed ring buffer and is handed to the `Scheduler` exactly as
`CsvRecorder` is. Charts plot the same series the CSV holds; no new metrics
are introduced by this document.

**Per-agent recent history.** A separate fixed ring of 512 months x 1100
agents x 4 fields (~18 MB) feeds §4.1's sparklines. 512 is a budget, not a
model constant, and lives in the viewer.

**Scrubbing to an arbitrary month.** Exact, via re-simulation, not
interpolation. The viewer snapshots `World` every 50 months (~100 KB per
snapshot, ~12 MB for a 6000-month run). Seeking to month M restores the
nearest snapshot at or before M and re-runs the scheduler forward at most 49
months — ~70 ms worst case. Determinism (invariant 2) is what makes this
byte-exact: the restored-and-replayed state equals the original state.

Snapshotting requires `World` and `Rng` to be copyable. `World` owns plain
struct arrays and `Rng` holds its seeded state, so this is a value copy today;
the design adds no interface to the core to support it. If a future extension
adds a non-copyable member, that is the point to revisit — not before.

Seeking backward past the earliest retained snapshot is not supported;
snapshots are kept for the whole run, so in practice this does not arise.

## 5. Relationship to the existing analysis scripts

- `scripts/validate.py` remains the single source of truth for the four
  success criteria. The viewer does **not** reimplement the criteria math.
  If a criterion number is ever shown in the viewer it must be computed the
  same way, and the honest first version simply does not show them.
- `scripts/report.py` remains the shareable static artifact — a directory you
  copy and hand to someone. The viewer cannot do that and should not try.
- `scripts/plot.py` becomes largely redundant once phase C exists, but is not
  deleted by this work.

The CSV path is unchanged: `economy --out run.csv` still produces exactly the
file the scripts read.

## 6. Dependencies and risks

- **SDL3 3.4.14** and **GL** are present system-wide (also SDL2 and GLFW3, if
  SDL3 proves awkward). Linked via `pkg-config`.
- **Dear ImGui** v1.92.9b-docking, vendored into `third_party/imgui/`
  following the existing `third_party/doctest.h` precedent. Docking is wanted:
  the three panels plus a detail panel need to be rearrangeable.
- **ImPlot** for the phase C charts. *Risk:* ImPlot's last tagged release
  predates ImGui 1.92's font/API changes and may not compile against it.
  Mitigation, in order: use ImPlot master; if that fails, pin ImGui to the
  latest version ImPlot supports; if that fails, phase C ships with
  `ImGui::PlotLines`, which is uglier and has no axis control but is in-tree
  and sufficient for the series involved. This risk is confined to phase C
  and does not block B or A.
- **Warning-clean core.** `-Wall -Wextra -Wpedantic` applies project-wide and
  vendored ImGui will not survive `-Wpedantic`. The vendored sources get their
  warnings suppressed at the target level; the viewer's own code does not.

## 7. Testing

The frame loop and the panels are not unit-tested; that is deliberate, and
the reason the core stays free of viewer code.

What *is* tested, in `tests/`, because it is logic rather than pixels:

- **Snapshot round-trip.** Simulate to month 200; snapshot; simulate to 300
  recording metrics; restore the snapshot; re-simulate to 300. The metrics
  must be byte-identical. This is invariant 2 stated as a test, and it is the
  one that makes scrubbing trustworthy.
- **`RingRecorder`** — wraps correctly, retains the last N, reports its
  window honestly when fewer than N months have run.
- **Histogram binning** — bin assignment at boundaries, the empty range, and
  the all-identical-values case (the zero-wage spike is exactly this).

## 8. Open questions

None blocking. Deferred by choice: steering (§2), spatial rendering of the
network (§2), and whether the viewer should ever display the success criteria
(§5).
