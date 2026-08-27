# economy_viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A live desktop viewer for the Lengnick ABM that shows every agent's
state, the distribution over agents, and the time series — with an exact scrub
bar over the run.

**Architecture:** A second executable, `economy_viewer`, that owns the `World`
and ticks it in its own frame loop (a month is 1.4 ms; a frame is 16 ms, so no
simulation thread is needed). All testable viewer logic — the sim wrapper,
the recorder, the histogram binning — lives in a `viewer_core` library with no
graphics dependency, linked into both the viewer and the existing test binary.
Only the panels and the frame loop touch SDL/ImGui. `src/` is not modified.

**Tech Stack:** C++23, CMake, SDL3 (system, `pkg-config sdl3`), Dear ImGui
v1.92.9b-docking (vendored), ImPlot (vendored, phase C only), doctest (already
vendored).

**Spec:** `docs/superpowers/specs/2026-08-28-economy-viewer-design.md`

## Global Constraints

- C++23, `-Wall -Wextra -Wpedantic`, warning-clean. Vendored third-party
  sources get warnings suppressed at the target level; viewer code does not.
- Nothing in `src/` is modified by this plan. No `std::cout` added to `src/`.
- The viewer reads `World`; it never mutates agent state. (Determinism,
  invariant 2.)
- Cross-agent references are indices with sentinel `0xFFFFFFFFu`
  (`NO_FIRM` / `NO_HOUSEHOLD`).
- The viewer does not reimplement the four success criteria;
  `scripts/validate.py` stays the single source of truth.
- `ECONOMY_BUILD_VIEWER` defaults ON only when SDL3 is found, so a machine
  without SDL3 still builds `economy` and `economy_tests`.
- Existing commands must keep working unchanged at every task boundary:
  `cmake -S . -B build && cmake --build build -j`, `./build/economy_tests`,
  `./build/economy --seed 1 --burn-in 1000 --months 6000 --out output/run.csv`.

## File Structure

| File | Responsibility |
|---|---|
| `viewer/sim.{hpp,cpp}` | `Sim`: owns `World` + `Rng` + month counter; `step()`, `snapshot()`, `restore()`, `seek()`. No graphics. |
| `viewer/ring_recorder.{hpp,cpp}` | `RingRecorder : Recorder` — last N `Metrics` in a fixed ring. No graphics. |
| `viewer/histogram.{hpp,cpp}` | `bin()` over a span of doubles. No graphics. |
| `viewer/agent_history.{hpp,cpp}` | Fixed ring of per-agent fields for sparklines. No graphics. |
| `viewer/app.{hpp,cpp}` | Viewer state: a `Sim`, the recorders, selection, filters, playback speed. No graphics. |
| `viewer/main.cpp` | SDL3 window, ImGui frame loop, tick pacing, panel dispatch. |
| `viewer/panel_agents.cpp` | Phase B: firm/household tables + detail panel. |
| `viewer/panel_dist.cpp` | Phase A: histograms, bin-click filtering. |
| `viewer/panel_time.cpp` | Phase C: time series + scrub bar. |
| `tests/test_viewer_sim.cpp` | Snapshot round-trip / seek exactness. |
| `tests/test_viewer_ring.cpp` | `RingRecorder`. |
| `tests/test_viewer_histogram.cpp` | Binning edge cases. |
| `third_party/imgui/`, `third_party/implot/` | Vendored, beside `doctest.h`. |

`viewer_core` (CMake library) = `sim`, `ring_recorder`, `histogram`,
`agent_history`, `app`. It links `economy_core` and nothing else.

---

### Task 1: `Sim` — own the world, step it, snapshot and seek it

This is first because it is the load-bearing claim of the design: scrubbing is
exact re-simulation, not interpolation. If this test does not pass, the rest of
the plan is not worth building.

Note for the implementer: `Scheduler` holds only a month counter beyond its
`World&`/`Rng&` references (`src/scheduler.hpp`), so `Sim` keeps its own month
counter and constructs a throwaway `Scheduler` per step. That avoids storing an
object with reference members inside a type we need to restore into.

**Files:**
- Create: `viewer/sim.hpp`, `viewer/sim.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_viewer_sim.cpp`

**Interfaces:**
- Consumes: `World`, `Rng`, `Scheduler`, `compute_metrics` from `economy_core`.
- Produces:
  - `struct SimSnapshot { World world; Rng rng; int month; };`
  - `class Sim` with `explicit Sim(uint64_t seed);`
    `void step();` `void step(int months);`
    `int month() const;`
    `const World& world() const;`
    `SimSnapshot snapshot() const;`
    `void restore(const SimSnapshot& s);`
    `void seek(int target_month);`
    `Sim` is non-copyable.

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_sim.cpp`:

```cpp
#include <vector>

#include "doctest.h"
#include "recorder.hpp"
#include "sim.hpp"
#include "world.hpp"

namespace {

// The metrics a fresh sim produces for months (from, to].
std::vector<Metrics> run_to(Sim& s, int to) {
    std::vector<Metrics> out;
    while (s.month() < to) {
        s.step();
        out.push_back(compute_metrics(s.world(), s.month()));
    }
    return out;
}

void require_identical(const std::vector<Metrics>& a, const std::vector<Metrics>& b) {
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].month == b[i].month);
        // Byte-exact, not approximate: determinism is the whole claim.
        CHECK(a[i].unemployment_rate == b[i].unemployment_rate);
        CHECK(a[i].mean_wage == b[i].mean_wage);
        CHECK(a[i].mean_price == b[i].mean_price);
        CHECK(a[i].gini == b[i].gini);
        CHECK(a[i].total_money == b[i].total_money);
    }
}

}  // namespace

TEST_CASE("stepping advances the month counter") {
    Sim s(1);
    CHECK(s.month() == 0);
    s.step();
    CHECK(s.month() == 1);
    s.step(5);
    CHECK(s.month() == 6);
}

TEST_CASE("restoring a snapshot reproduces the future exactly") {
    Sim s(1);
    s.step(200);
    const SimSnapshot snap = s.snapshot();
    CHECK(snap.month == 200);

    const std::vector<Metrics> first = run_to(s, 300);

    s.restore(snap);
    CHECK(s.month() == 200);
    const std::vector<Metrics> second = run_to(s, 300);

    require_identical(first, second);
}

TEST_CASE("seeking backward lands on the same state as running forward once") {
    Sim reference(1);
    reference.step(275);
    const Metrics expected = compute_metrics(reference.world(), 275);

    Sim s(1);
    s.step(400);
    s.seek(275);
    CHECK(s.month() == 275);
    const Metrics got = compute_metrics(s.world(), 275);

    CHECK(got.unemployment_rate == expected.unemployment_rate);
    CHECK(got.mean_wage == expected.mean_wage);
    CHECK(got.gini == expected.gini);
}

TEST_CASE("seeking forward is the same as stepping forward") {
    Sim s(1);
    s.seek(50);
    CHECK(s.month() == 50);
    const Metrics seeked = compute_metrics(s.world(), 50);

    Sim t(1);
    t.step(50);
    CHECK(compute_metrics(t.world(), 50).mean_wage == seeked.mean_wage);
}

TEST_CASE("money is conserved across a seek") {
    Sim s(1);
    const double start = s.world().total_money();
    s.step(300);
    s.seek(100);
    CHECK(s.world().total_money() == doctest::Approx(start).epsilon(1e-12));
}
```

- [ ] **Step 2: Wire the test into the build, then run it to verify it fails**

In `CMakeLists.txt`, add the library and the new test sources. Insert after the
`economy_core` target and before `enable_testing()`:

```cmake
add_library(viewer_core
    viewer/sim.cpp
)
target_link_libraries(viewer_core PUBLIC economy_core)
target_include_directories(viewer_core PUBLIC viewer)
```

Add to the `economy_tests` source list:

```cmake
    tests/test_viewer_sim.cpp
```

and add `viewer_core` to its `target_link_libraries`:

```cmake
target_link_libraries(economy_tests PRIVATE economy_core viewer_core)
```

Run: `cmake -S . -B build && cmake --build build -j`
Expected: FAIL — `viewer/sim.cpp` does not exist / `sim.hpp` not found.

- [ ] **Step 3: Write the implementation**

Create `viewer/sim.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

#include "rng.hpp"
#include "world.hpp"

// A restorable point in a run. Copyable by value: World owns plain-struct
// vectors and Rng owns its engine state, so this is a deep copy today. If a
// future extension gives either a non-copyable member, this is the line that
// stops compiling -- fix it here rather than adding an interface to the core.
struct SimSnapshot {
    World world;
    Rng rng;
    int month = 0;
};

// Owns a run and can move it to any month exactly.
//
// Scheduler holds no state beyond its own month counter (see
// src/scheduler.hpp), so Sim keeps the authoritative counter and builds a
// throwaway Scheduler per step. That keeps Sim free of members holding
// references into itself, which is what makes restore() a plain assignment.
class Sim {
public:
    explicit Sim(uint64_t seed);

    Sim(const Sim&) = delete;
    Sim& operator=(const Sim&) = delete;

    void step();
    void step(int months);

    int month() const { return month_; }
    const World& world() const { return world_; }
    uint64_t seed() const { return seed_; }

    SimSnapshot snapshot() const;
    void restore(const SimSnapshot& s);

    // Moves to target_month exactly. Forward: steps. Backward: restores the
    // newest retained snapshot at or before the target, then re-simulates
    // forward. Determinism (invariant 2) is what makes this byte-exact.
    // Negative targets are clamped to 0.
    void seek(int target_month);

    // Months between retained snapshots. A snapshot is ~100 KB, so a
    // 6000-month run retains ~12 MB and never re-simulates more than
    // kSnapshotInterval - 1 months (~70 ms).
    static constexpr int kSnapshotInterval = 50;

private:
    void maybe_retain_snapshot();

    uint64_t seed_;
    Rng rng_;
    World world_;
    int month_ = 0;
    std::vector<SimSnapshot> snapshots_;  // ascending by month, month 0 first
};
```

Create `viewer/sim.cpp`:

```cpp
#include "sim.hpp"

#include <algorithm>

#include "params.hpp"
#include "scheduler.hpp"

Sim::Sim(uint64_t seed)
    : seed_(seed), rng_(seed), world_(params::kNumHouseholds, params::kNumFirms, rng_) {
    snapshots_.push_back(SimSnapshot{world_, rng_, 0});
}

void Sim::step() {
    Scheduler(world_, rng_).run_month();
    ++month_;
    maybe_retain_snapshot();
}

void Sim::step(int months) {
    for (int i = 0; i < months; ++i) step();
}

void Sim::maybe_retain_snapshot() {
    if (month_ % kSnapshotInterval != 0) return;
    if (!snapshots_.empty() && snapshots_.back().month >= month_) return;
    snapshots_.push_back(SimSnapshot{world_, rng_, month_});
}

SimSnapshot Sim::snapshot() const { return SimSnapshot{world_, rng_, month_}; }

void Sim::restore(const SimSnapshot& s) {
    world_ = s.world;
    rng_ = s.rng;
    month_ = s.month;
}

void Sim::seek(int target_month) {
    const int target = std::max(0, target_month);
    if (target < month_) {
        // Newest retained snapshot at or before the target. snapshots_ is
        // ascending and always holds month 0, so this always finds one.
        auto it = std::upper_bound(
            snapshots_.begin(), snapshots_.end(), target,
            [](int m, const SimSnapshot& s) { return m < s.month; });
        restore(*(it - 1));
    }
    while (month_ < target) step();
}
```

`World` has no default constructor and `Rng` has no default constructor, so
`SimSnapshot` is only ever created from existing values — that is intended.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS, including all pre-existing cases (95 before this task).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt viewer/sim.hpp viewer/sim.cpp tests/test_viewer_sim.cpp
git commit -m "feat(viewer): add Sim with exact snapshot/seek over a run"
```

---

### Task 2: `RingRecorder` — the last N months of metrics

**Files:**
- Create: `viewer/ring_recorder.hpp`, `viewer/ring_recorder.cpp`
- Modify: `CMakeLists.txt` (add sources)
- Test: `tests/test_viewer_ring.cpp`

**Interfaces:**
- Consumes: `Recorder`, `Metrics` from `src/recorder.hpp`.
- Produces: `class RingRecorder : public Recorder` with
  `explicit RingRecorder(size_t capacity);`
  `void record(const Metrics& m) override;`
  `size_t size() const;` `size_t capacity() const;`
  `const Metrics& at(size_t i) const;`  // 0 = oldest retained
  `void clear();`

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_ring.cpp`:

```cpp
#include "doctest.h"
#include "ring_recorder.hpp"

namespace {

Metrics month(int m) {
    Metrics x;
    x.month = m;
    x.mean_wage = static_cast<double>(m);
    return x;
}

}  // namespace

TEST_CASE("an empty ring reports its window honestly") {
    RingRecorder r(4);
    CHECK(r.size() == 0);
    CHECK(r.capacity() == 4);
}

TEST_CASE("a partially filled ring returns what it has, oldest first") {
    RingRecorder r(4);
    r.record(month(1));
    r.record(month(2));
    CHECK(r.size() == 2);
    CHECK(r.at(0).month == 1);
    CHECK(r.at(1).month == 2);
}

TEST_CASE("a full ring keeps the last N, oldest first") {
    RingRecorder r(3);
    for (int m = 1; m <= 5; ++m) r.record(month(m));
    CHECK(r.size() == 3);
    CHECK(r.at(0).month == 3);
    CHECK(r.at(1).month == 4);
    CHECK(r.at(2).month == 5);
    CHECK(r.at(2).mean_wage == 5.0);
}

TEST_CASE("clear empties the ring") {
    RingRecorder r(3);
    r.record(month(1));
    r.clear();
    CHECK(r.size() == 0);
}

TEST_CASE("capacity of zero is rejected") {
    CHECK_THROWS_AS(RingRecorder(0), std::invalid_argument);
}
```

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_viewer_ring.cpp` to the `economy_tests` sources and
`viewer/ring_recorder.cpp` to the `viewer_core` sources in `CMakeLists.txt`.

Run: `cmake -S . -B build && cmake --build build -j`
Expected: FAIL — `ring_recorder.hpp` not found.

- [ ] **Step 3: Write the implementation**

Create `viewer/ring_recorder.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <vector>

#include "recorder.hpp"

// Keeps the last `capacity` Metrics in a fixed-size ring. The viewer's
// time-series source; handed to the simulation exactly as CsvRecorder is,
// so the core still does I/O only through Recorder (invariant 5).
class RingRecorder : public Recorder {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit RingRecorder(size_t capacity);

    void record(const Metrics& m) override;

    size_t size() const { return size_; }
    size_t capacity() const { return buf_.size(); }

    // i == 0 is the oldest retained month. Throws std::out_of_range if
    // i >= size().
    const Metrics& at(size_t i) const;

    void clear();

private:
    std::vector<Metrics> buf_;
    size_t next_ = 0;   // where the next record lands
    size_t size_ = 0;   // how many slots are live
};
```

Create `viewer/ring_recorder.cpp`:

```cpp
#include "ring_recorder.hpp"

#include <stdexcept>

RingRecorder::RingRecorder(size_t capacity) : buf_(capacity) {
    if (capacity == 0) throw std::invalid_argument("RingRecorder capacity must be > 0");
}

void RingRecorder::record(const Metrics& m) {
    buf_[next_] = m;
    next_ = (next_ + 1) % buf_.size();
    if (size_ < buf_.size()) ++size_;
}

const Metrics& RingRecorder::at(size_t i) const {
    if (i >= size_) throw std::out_of_range("RingRecorder::at index out of range");
    // Once full, the oldest live slot is the one next_ is about to overwrite.
    const size_t oldest = (size_ == buf_.size()) ? next_ : 0;
    return buf_[(oldest + i) % buf_.size()];
}

void RingRecorder::clear() {
    next_ = 0;
    size_ = 0;
}
```

Note: the constructor sizes `buf_` before the capacity check, so the throw
happens with an empty vector — harmless, and it keeps the member-init list
simple. `Metrics` is an aggregate with defaults, so `std::vector<Metrics>(n)`
value-initializes cleanly.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt viewer/ring_recorder.hpp viewer/ring_recorder.cpp tests/test_viewer_ring.cpp
git commit -m "feat(viewer): add RingRecorder for the live time series"
```

---

### Task 3: Histogram binning

Separate from the panel that draws it, because this is where the
zero-wage-spike case lives and it is worth a test.

**Files:**
- Create: `viewer/histogram.hpp`, `viewer/histogram.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_viewer_histogram.cpp`

**Interfaces:**
- Produces:
  - `struct Histogram { double lo; double hi; std::vector<int> counts;
    std::vector<std::vector<uint32_t>> members; double mean; };`
    `members[b]` holds the indices of the values that fell in bin `b`, which is
    what makes clicking a bin able to filter the agent table.
  - `Histogram bin(const std::vector<double>& values, int bins);`

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_histogram.cpp`:

```cpp
#include <vector>

#include "doctest.h"
#include "histogram.hpp"

TEST_CASE("values spread across bins land where expected") {
    const std::vector<double> v{0.0, 1.0, 2.0, 3.0};
    const Histogram h = bin(v, 4);
    CHECK(h.lo == 0.0);
    CHECK(h.hi == 3.0);
    REQUIRE(h.counts.size() == 4);
    CHECK(h.counts[0] == 1);
    CHECK(h.counts[3] == 1);
    CHECK(h.mean == doctest::Approx(1.5));
}

TEST_CASE("the maximum value lands in the last bin, not past it") {
    const std::vector<double> v{0.0, 10.0};
    const Histogram h = bin(v, 5);
    CHECK(h.counts.back() == 1);
    REQUIRE(h.members.back().size() == 1);
    CHECK(h.members.back()[0] == 1u);
}

TEST_CASE("all-identical values collapse into one bin") {
    // This is the zero-wage spike: 67 firms at exactly 0.0.
    const std::vector<double> v(67, 0.0);
    const Histogram h = bin(v, 10);
    CHECK(h.lo == 0.0);
    CHECK(h.hi == 0.0);
    CHECK(h.counts[0] == 67);
    CHECK(h.members[0].size() == 67);
    CHECK(h.mean == 0.0);
}

TEST_CASE("members record the original indices") {
    const std::vector<double> v{5.0, 0.0, 5.0};
    const Histogram h = bin(v, 2);
    REQUIRE(h.members.front().size() == 1);
    CHECK(h.members.front()[0] == 1u);
    REQUIRE(h.members.back().size() == 2);
    CHECK(h.members.back()[0] == 0u);
    CHECK(h.members.back()[1] == 2u);
}

TEST_CASE("an empty input produces empty bins, not a crash") {
    const Histogram h = bin({}, 4);
    CHECK(h.counts.size() == 4);
    CHECK(h.counts[0] == 0);
    CHECK(h.mean == 0.0);
}

TEST_CASE("a bin count below one is rejected") {
    CHECK_THROWS_AS(bin({1.0}, 0), std::invalid_argument);
}
```

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_viewer_histogram.cpp` to `economy_tests` and
`viewer/histogram.cpp` to `viewer_core` in `CMakeLists.txt`.

Run: `cmake -S . -B build && cmake --build build -j`
Expected: FAIL — `histogram.hpp` not found.

- [ ] **Step 3: Write the implementation**

Create `viewer/histogram.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

// A distribution over agents, plus the agent indices in each bin so that
// clicking a bin can filter the agent table.
struct Histogram {
    double lo = 0.0;
    double hi = 0.0;
    std::vector<int> counts;
    std::vector<std::vector<uint32_t>> members;
    double mean = 0.0;
};

// Bins `values` into `bins` equal-width buckets over [min, max]. The maximum
// value lands in the last bin. If every value is identical (the degenerate
// range), everything lands in bin 0.
// Throws std::invalid_argument if bins < 1.
Histogram bin(const std::vector<double>& values, int bins);
```

Create `viewer/histogram.cpp`:

```cpp
#include "histogram.hpp"

#include <algorithm>
#include <stdexcept>

Histogram bin(const std::vector<double>& values, int bins) {
    if (bins < 1) throw std::invalid_argument("bin() needs at least one bin");

    Histogram h;
    h.counts.assign(static_cast<size_t>(bins), 0);
    h.members.assign(static_cast<size_t>(bins), {});
    if (values.empty()) return h;

    const auto [lo_it, hi_it] = std::minmax_element(values.begin(), values.end());
    h.lo = *lo_it;
    h.hi = *hi_it;

    double sum = 0.0;
    for (double v : values) sum += v;
    h.mean = sum / static_cast<double>(values.size());

    const double width = h.hi - h.lo;
    for (uint32_t i = 0; i < values.size(); ++i) {
        size_t b = 0;
        if (width > 0.0) {
            const double t = (values[i] - h.lo) / width;
            b = static_cast<size_t>(t * static_cast<double>(bins));
            // t == 1.0 for the maximum: clamp it into the last bin rather
            // than one past the end.
            b = std::min(b, static_cast<size_t>(bins) - 1);
        }
        ++h.counts[b];
        h.members[b].push_back(i);
    }
    return h;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt viewer/histogram.hpp viewer/histogram.cpp tests/test_viewer_histogram.cpp
git commit -m "feat(viewer): add histogram binning with per-bin membership"
```

---

### Task 4: Window, ImGui, and a running economy on screen

The first task with no unit test — the gate is that it builds warning-clean,
opens, and runs. Everything testable was deliberately pushed into tasks 1-3.

**Files:**
- Create: `viewer/main.cpp`, `viewer/app.hpp`, `viewer/app.cpp`
- Create (vendored): `third_party/imgui/`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Sim` (task 1), `RingRecorder` (task 2).
- Produces: `struct App` with
  `explicit App(uint64_t seed);`
  `Sim sim; RingRecorder metrics{4096};`
  `int months_per_frame = 1;` `bool paused = true;`
  `uint32_t selected_firm = NO_FIRM;` `uint32_t selected_household = NO_HOUSEHOLD;`
  `void advance();`  // steps `months_per_frame` months if not paused, recording each
  and the free functions each later panel task implements:
  `void draw_agents_panel(App&);` `void draw_dist_panel(App&);`
  `void draw_time_panel(App&);`

- [ ] **Step 1: Vendor Dear ImGui**

```bash
git clone --depth 1 --branch v1.92.9b-docking https://github.com/ocornut/imgui /tmp/imgui-src
mkdir -p third_party/imgui/backends
cp /tmp/imgui-src/*.h /tmp/imgui-src/*.cpp third_party/imgui/
cp /tmp/imgui-src/backends/imgui_impl_sdl3.* /tmp/imgui-src/backends/imgui_impl_sdlrenderer3.* third_party/imgui/backends/
rm -f third_party/imgui/imgui_demo.cpp
rm -rf /tmp/imgui-src
```

The SDL renderer backend is used rather than OpenGL: it needs no GL loader,
and this UI draws no 3D.

- [ ] **Step 2: Add the viewer target to `CMakeLists.txt`**

Append to `CMakeLists.txt`:

```cmake
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

option(ECONOMY_BUILD_VIEWER "Build the interactive viewer" ${SDL3_FOUND})

if(ECONOMY_BUILD_VIEWER)
  if(NOT SDL3_FOUND)
    message(FATAL_ERROR "ECONOMY_BUILD_VIEWER is ON but SDL3 was not found")
  endif()

  add_library(imgui STATIC
      third_party/imgui/imgui.cpp
      third_party/imgui/imgui_draw.cpp
      third_party/imgui/imgui_tables.cpp
      third_party/imgui/imgui_widgets.cpp
      third_party/imgui/backends/imgui_impl_sdl3.cpp
      third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui SYSTEM PUBLIC third_party/imgui third_party/imgui/backends)
  target_link_libraries(imgui PUBLIC PkgConfig::SDL3)
  # Vendored third-party code does not have to satisfy our warning settings.
  target_compile_options(imgui PRIVATE -w)

  add_executable(economy_viewer
      viewer/main.cpp
      viewer/app.cpp
      viewer/panel_agents.cpp
      viewer/panel_dist.cpp
      viewer/panel_time.cpp
  )
  target_link_libraries(economy_viewer PRIVATE viewer_core imgui)
endif()
```

- [ ] **Step 3: Write `App`**

Create `viewer/app.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "agents.hpp"
#include "ring_recorder.hpp"
#include "sim.hpp"

// All viewer state. Holds no graphics types, so it can be reasoned about
// (and later tested) without a window.
struct App {
    explicit App(uint64_t seed) : sim(seed) {}

    Sim sim;
    RingRecorder metrics{4096};

    bool paused = true;
    int months_per_frame = 1;

    uint32_t selected_firm = NO_FIRM;
    uint32_t selected_household = NO_HOUSEHOLD;

    // Steps the simulation if running, recording each month's metrics.
    void advance();
};

void draw_agents_panel(App& app);
void draw_dist_panel(App& app);
void draw_time_panel(App& app);
```

Create `viewer/app.cpp`:

```cpp
#include "app.hpp"

#include "recorder.hpp"

void App::advance() {
    if (paused) return;
    for (int i = 0; i < months_per_frame; ++i) {
        sim.step();
        metrics.record(compute_metrics(sim.world(), sim.month()));
    }
}
```

- [ ] **Step 4: Write the frame loop**

Create `viewer/main.cpp`:

```cpp
#include <SDL3/SDL.h>

#include <cstdio>

#include "app.hpp"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("economy", 1600, 1000, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (window == nullptr || renderer == nullptr) {
        std::fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    App app(1);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        app.advance();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        ImGui::Begin("run");
        ImGui::Text("month %d", app.sim.month());
        if (ImGui::Button(app.paused ? "run" : "pause")) app.paused = !app.paused;
        ImGui::SameLine();
        if (ImGui::Button("step")) {
            const bool was_paused = app.paused;
            app.paused = false;
            app.advance();
            app.paused = was_paused;
        }
        ImGui::SliderInt("months/frame", &app.months_per_frame, 1, 60);
        ImGui::End();

        draw_agents_panel(app);
        draw_dist_panel(app);
        draw_time_panel(app);

        ImGui::Render();
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

- [ ] **Step 5: Add empty panel stubs so it links**

Create `viewer/panel_agents.cpp`, `viewer/panel_dist.cpp`,
`viewer/panel_time.cpp`, each with the corresponding one-line definition, e.g.
`viewer/panel_agents.cpp`:

```cpp
#include "app.hpp"
#include "imgui.h"

void draw_agents_panel(App& app) {
    ImGui::Begin("agents");
    ImGui::Text("%zu firms, %zu households",
                app.sim.world().firms.size(), app.sim.world().households.size());
    ImGui::End();
}
```

The other two are the same shape with `"distributions"` / `"time"` and a
`ImGui::TextUnformatted("not yet");` body.

- [ ] **Step 6: Build and run**

Run: `cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "warning|error" ; ./build/economy_viewer`
Expected: no warnings from `viewer/` sources; a window opens; pressing "run"
advances the month counter; `./build/economy_tests` still passes and
`./build/economy --seed 1 --burn-in 10 --months 10 --out /tmp/x.csv` still works.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt viewer third_party/imgui
git commit -m "feat(viewer): add SDL3 + ImGui window running the simulation live"
```

---

### Task 5: Phase B — the agent table and detail panel

**Files:**
- Modify: `viewer/panel_agents.cpp` (replace the stub), `viewer/app.hpp`

**Interfaces:**
- Consumes: `App`, `Firm`, `Household`, `NO_FIRM`, `NO_HOUSEHOLD`.
- Produces: `App` gains `struct Filters { bool wage_at_floor = false;
  bool vacancy_open = false; bool inventory_zero = false;
  bool unemployed = false; std::vector<uint32_t> pinned_firms; } filters;`
  `pinned_firms`, when non-empty, restricts the firm table to those indices —
  this is what task 6's histogram click writes to.

- [ ] **Step 1: Add the filter state to `App`**

In `viewer/app.hpp`, add `#include <vector>` and, inside `struct App`:

```cpp
    struct Filters {
        bool wage_at_floor = false;
        bool vacancy_open = false;
        bool inventory_zero = false;
        bool unemployed = false;
        // Non-empty means: show only these firms. Written by a histogram
        // bin click, cleared by the "clear" button.
        std::vector<uint32_t> pinned_firms;
    } filters;
```

- [ ] **Step 2: Write the panel**

Replace `viewer/panel_agents.cpp` with:

```cpp
#include <algorithm>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "params.hpp"

namespace {

bool firm_passes(const App& app, uint32_t i, const Firm& f) {
    const auto& pinned = app.filters.pinned_firms;
    if (!pinned.empty() && std::find(pinned.begin(), pinned.end(), i) == pinned.end()) {
        return false;
    }
    if (app.filters.wage_at_floor && f.wage > params::kMinWage) return false;
    if (app.filters.vacancy_open && !f.has_open_vacancy) return false;
    if (app.filters.inventory_zero && f.inventory > 0.0) return false;
    return true;
}

void draw_firm_table(App& app) {
    const auto& firms = app.sim.world().firms;

    ImGui::Checkbox("wage at floor", &app.filters.wage_at_floor);
    ImGui::SameLine();
    ImGui::Checkbox("vacancy open", &app.filters.vacancy_open);
    ImGui::SameLine();
    ImGui::Checkbox("inventory zero", &app.filters.inventory_zero);
    if (!app.filters.pinned_firms.empty()) {
        ImGui::SameLine();
        ImGui::Text("| %zu pinned", app.filters.pinned_firms.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("clear")) app.filters.pinned_firms.clear();
    }

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("firms", 8, kFlags, ImVec2(0, 300))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("firm");
    ImGui::TableSetupColumn("wage");
    ImGui::TableSetupColumn("price");
    ImGui::TableSetupColumn("inventory");
    ImGui::TableSetupColumn("staff");
    ImGui::TableSetupColumn("liquidity");
    ImGui::TableSetupColumn("vacancy");
    ImGui::TableSetupColumn("demand");
    ImGui::TableHeadersRow();

    std::vector<uint32_t> rows;
    rows.reserve(firms.size());
    for (uint32_t i = 0; i < firms.size(); ++i) {
        if (firm_passes(app, i, firms[i])) rows.push_back(i);
    }

    if (ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
        sort != nullptr && sort->SpecsCount > 0) {
        const ImGuiTableColumnSortSpecs& s = sort->Specs[0];
        const bool asc = s.SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
            const Firm& fa = firms[a];
            const Firm& fb = firms[b];
            double va = 0.0;
            double vb = 0.0;
            switch (s.ColumnIndex) {
                case 0: va = a; vb = b; break;
                case 1: va = fa.wage; vb = fb.wage; break;
                case 2: va = fa.price; vb = fb.price; break;
                case 3: va = fa.inventory; vb = fb.inventory; break;
                case 4: va = static_cast<double>(fa.employees.size());
                        vb = static_cast<double>(fb.employees.size()); break;
                case 5: va = fa.liquidity; vb = fb.liquidity; break;
                case 6: va = fa.has_open_vacancy; vb = fb.has_open_vacancy; break;
                default: va = fa.demand_last_month; vb = fb.demand_last_month; break;
            }
            return asc ? (va < vb) : (vb < va);
        });
    }

    for (uint32_t i : rows) {
        const Firm& f = firms[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        char label[16];
        std::snprintf(label, sizeof(label), "%u", i);
        if (ImGui::Selectable(label, app.selected_firm == i,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            app.selected_firm = i;
        }
        ImGui::TableNextColumn(); ImGui::Text("%.2f", f.wage);
        ImGui::TableNextColumn(); ImGui::Text("%.3f", f.price);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.inventory);
        ImGui::TableNextColumn(); ImGui::Text("%zu", f.employees.size());
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.liquidity);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(f.has_open_vacancy ? "yes" : "");
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.demand_last_month);
    }
    ImGui::EndTable();
}

void draw_household_table(App& app) {
    const auto& households = app.sim.world().households;

    ImGui::Checkbox("unemployed only", &app.filters.unemployed);

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("households", 5, kFlags, ImVec2(0, 300))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("household");
    ImGui::TableSetupColumn("employer");
    ImGui::TableSetupColumn("liquidity");
    ImGui::TableSetupColumn("reservation wage");
    ImGui::TableSetupColumn("planned consumption");
    ImGui::TableHeadersRow();

    // 1000 rows: let ImGui skip the off-screen ones rather than formatting
    // text for all of them every frame.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(households.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const uint32_t i = static_cast<uint32_t>(row);
            const Household& h = households[i];
            if (app.filters.unemployed && h.employed()) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[16];
            std::snprintf(label, sizeof(label), "%u", i);
            if (ImGui::Selectable(label, app.selected_household == i,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                app.selected_household = i;
            }
            ImGui::TableNextColumn();
            if (h.employed()) {
                ImGui::Text("%u", h.employer);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.liquidity);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.reservation_wage);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.planned_consumption);
        }
    }
    ImGui::EndTable();
}

void draw_detail(App& app) {
    const World& w = app.sim.world();

    if (app.selected_firm != NO_FIRM && app.selected_firm < w.firms.size()) {
        const Firm& f = w.firms[app.selected_firm];
        ImGui::SeparatorText("selected firm");
        ImGui::Text("firm %u", app.selected_firm);
        ImGui::Text("wage %.4f   price %.4f   inventory %.3f", f.wage, f.price, f.inventory);
        ImGui::Text("liquidity %.3f", f.liquidity);
        ImGui::Text("demand this month %.2f, last month %.2f",
                    f.demand_this_month, f.demand_last_month);
        ImGui::Text("months fully staffed %u", f.months_fully_staffed);
        ImGui::Text("vacancy open %s, went unfilled %s",
                    f.has_open_vacancy ? "yes" : "no",
                    f.vacancy_went_unfilled ? "yes" : "no");
        if (f.pending_fire != NO_HOUSEHOLD) {
            ImGui::Text("pending fire: household %u", f.pending_fire);
        }
        ImGui::Text("employees (%zu):", f.employees.size());
        for (uint32_t e : f.employees) {
            ImGui::SameLine();
            char b[16];
            std::snprintf(b, sizeof(b), "%u", e);
            if (ImGui::SmallButton(b)) app.selected_household = e;
        }
    }

    if (app.selected_household != NO_HOUSEHOLD && app.selected_household < w.households.size()) {
        const Household& h = w.households[app.selected_household];
        ImGui::SeparatorText("selected household");
        ImGui::Text("household %u", app.selected_household);
        ImGui::Text("liquidity %.4f   reservation wage %.4f", h.liquidity, h.reservation_wage);
        ImGui::Text("planned consumption %.4f", h.planned_consumption);
        if (h.employed()) {
            ImGui::Text("employer:");
            ImGui::SameLine();
            char b[16];
            std::snprintf(b, sizeof(b), "%u", h.employer);
            if (ImGui::SmallButton(b)) app.selected_firm = h.employer;
        } else {
            ImGui::TextUnformatted("unemployed");
        }
        ImGui::TextUnformatted("shops:");
        for (size_t s = 0; s < h.shops.size(); ++s) {
            ImGui::SameLine();
            char b[24];
            std::snprintf(b, sizeof(b), "%u##shop%zu", h.shops[s], s);
            if (ImGui::SmallButton(b)) app.selected_firm = h.shops[s];
            if (h.unsatisfied[s] > 0.0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(-%.1f)", h.unsatisfied[s]);
            }
        }
    }
}

}  // namespace

void draw_agents_panel(App& app) {
    ImGui::Begin("agents");
    if (ImGui::BeginTabBar("agent_tabs")) {
        if (ImGui::BeginTabItem("firms")) {
            draw_firm_table(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("households")) {
            draw_household_table(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    draw_detail(app);
    ImGui::End();
}
```

Add `#include <cstdio>` at the top for `std::snprintf`.

- [ ] **Step 3: Build and check by hand**

Run: `cmake --build build -j && ./build/economy_viewer`
Expected: warning-clean; the firms tab sorts by clicking a header; selecting a
firm shows its detail; clicking an employee index jumps to that household and
its employer button jumps back.

- [ ] **Step 4: Commit**

```bash
git add viewer/app.hpp viewer/panel_agents.cpp
git commit -m "feat(viewer): add the agent table and detail panel"
```

---

### Task 6: Phase A — distributions, with bin-click filtering

**Files:**
- Modify: `viewer/panel_dist.cpp` (replace the stub)

**Interfaces:**
- Consumes: `bin()` / `Histogram` (task 3), `App::filters::pinned_firms`
  (task 5).

- [ ] **Step 1: Write the panel**

Replace `viewer/panel_dist.cpp` with:

```cpp
#include <cstdio>
#include <vector>

#include "app.hpp"
#include "histogram.hpp"
#include "imgui.h"

namespace {

// Draws one distribution. `over_firms` decides whether a bin click pins the
// firm table to that bin's members.
void draw_one(App& app, const char* name, const std::vector<double>& values,
              int bins, bool over_firms) {
    const Histogram h = bin(values, bins);

    int max_count = 1;
    for (int c : h.counts) max_count = c > max_count ? c : max_count;

    ImGui::SeparatorText(name);
    // The mean is an annotation on the distribution, never the headline --
    // see the averaging-artifact note in CLAUDE.md.
    ImGui::TextDisabled("range [%.4f, %.4f]   mean %.4f   n=%zu   lowest bin holds %d",
                        h.lo, h.hi, h.mean, values.size(), h.counts[0]);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float w = avail / static_cast<float>(h.counts.size()) - 1.0f;
    for (size_t b = 0; b < h.counts.size(); ++b) {
        if (b > 0) ImGui::SameLine(0.0f, 1.0f);
        const float frac = static_cast<float>(h.counts[b]) / static_cast<float>(max_count);
        char id[32];
        std::snprintf(id, sizeof(id), "##%s%zu", name, b);
        // A button whose height encodes the count: clickable, and no plotting
        // library needed.
        ImGui::BeginGroup();
        const float height = 80.0f;
        ImGui::Dummy(ImVec2(w, height * (1.0f - frac)));
        if (ImGui::Button(id, ImVec2(w, height * frac + 1.0f)) && over_firms) {
            app.filters.pinned_firms = h.members[b];
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            const double width = (h.hi - h.lo) / static_cast<double>(h.counts.size());
            ImGui::SetTooltip("[%.4f, %.4f): %d\n%s", h.lo + width * b,
                              h.lo + width * (b + 1), h.counts[b],
                              over_firms ? "click to pin these firms" : "");
        }
    }
}

std::vector<double> firm_field(const World& w, int which) {
    std::vector<double> v;
    v.reserve(w.firms.size());
    for (const Firm& f : w.firms) {
        switch (which) {
            case 0: v.push_back(f.wage); break;
            case 1: v.push_back(f.price); break;
            case 2: v.push_back(f.inventory); break;
            case 3: v.push_back(static_cast<double>(f.employees.size())); break;
            default: v.push_back(f.liquidity); break;
        }
    }
    return v;
}

}  // namespace

void draw_dist_panel(App& app) {
    static int bins = 20;
    ImGui::Begin("distributions");
    ImGui::SliderInt("bins", &bins, 4, 60);
    ImGui::TextDisabled("click a firm bin to pin those firms in the agents panel");

    const World& w = app.sim.world();
    draw_one(app, "firm wage", firm_field(w, 0), bins, true);
    draw_one(app, "firm price", firm_field(w, 1), bins, true);
    draw_one(app, "firm inventory", firm_field(w, 2), bins, true);
    draw_one(app, "firm headcount", firm_field(w, 3), bins, true);
    draw_one(app, "firm liquidity", firm_field(w, 4), bins, true);

    std::vector<double> hl;
    std::vector<double> rw;
    hl.reserve(w.households.size());
    rw.reserve(w.households.size());
    for (const Household& h : w.households) {
        hl.push_back(h.liquidity);
        rw.push_back(h.reservation_wage);
    }
    draw_one(app, "household liquidity", hl, bins, false);
    draw_one(app, "household reservation wage", rw, bins, false);

    ImGui::End();
}
```

- [ ] **Step 2: Build and check by hand**

Run: `cmake --build build -j && ./build/economy_viewer`
Expected: seven histograms; clicking a firm-wage bin pins the agents panel's
firm table to exactly those firms, and the "clear" button releases it.

- [ ] **Step 3: Commit**

```bash
git add viewer/panel_dist.cpp
git commit -m "feat(viewer): add agent distributions with bin-click filtering"
```

---

### Task 7: Phase C — time series and the scrub bar

`ImGui::PlotLines` is used rather than ImPlot. The spec allows ImPlot with
PlotLines as the documented fallback (§6); starting at the fallback avoids
vendoring a library with a known compatibility risk for charts this simple.
If axis control is later wanted, that is the moment to revisit ImPlot.

**Files:**
- Modify: `viewer/panel_time.cpp` (replace the stub), `viewer/app.hpp`,
  `viewer/app.cpp`

**Interfaces:**
- Consumes: `RingRecorder` (task 2), `Sim::seek` (task 1).
- Produces: `App` gains `void seek_to(int month);` which seeks the sim and
  clears the metrics ring (the ring's contents no longer describe the
  displayed state).

- [ ] **Step 1: Add `seek_to` to `App`**

In `viewer/app.hpp`, inside `struct App`, below `advance()`:

```cpp
    // Moves the displayed state to `month`. The metrics ring is cleared
    // because its contents describe months that are no longer the ones being
    // shown; it refills as the run continues.
    void seek_to(int month);
```

In `viewer/app.cpp`:

```cpp
void App::seek_to(int month) {
    sim.seek(month);
    metrics.clear();
}
```

- [ ] **Step 2: Write the panel**

Replace `viewer/panel_time.cpp` with:

```cpp
#include <vector>

#include "app.hpp"
#include "imgui.h"

namespace {

void plot(const RingRecorder& r, const char* name, double Metrics::* field) {
    if (r.size() < 2) {
        ImGui::TextDisabled("%s: not enough months yet", name);
        return;
    }
    std::vector<float> v;
    v.reserve(r.size());
    for (size_t i = 0; i < r.size(); ++i) v.push_back(static_cast<float>(r.at(i).*field));
    ImGui::PlotLines(name, v.data(), static_cast<int>(v.size()), 0, nullptr,
                     FLT_MAX, FLT_MAX, ImVec2(0, 60));
}

}  // namespace

void draw_time_panel(App& app) {
    ImGui::Begin("time");

    int month = app.sim.month();
    ImGui::SetNextItemWidth(-120.0f);
    // The scrub bar. Seeking backward restores the nearest snapshot and
    // re-simulates forward, so what is shown is real state, not interpolation.
    if (ImGui::SliderInt("month", &month, 0, app.sim.month() > 0 ? app.sim.month() : 1)) {
        app.seek_to(month);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("latest")) {
        // The slider's own maximum already is the latest month; nothing to do
        // but release any earlier seek.
        app.seek_to(app.sim.month());
    }
    ImGui::TextDisabled("showing month %d of %d", app.sim.month(), app.sim.month());

    ImGui::SeparatorText("recorded since the last seek");
    plot(app.metrics, "unemployment", &Metrics::unemployment_rate);
    plot(app.metrics, "mean wage", &Metrics::mean_wage);
    plot(app.metrics, "mean price", &Metrics::mean_price);
    plot(app.metrics, "gini", &Metrics::gini);
    plot(app.metrics, "inventory", &Metrics::inventory);
    plot(app.metrics, "planned consumption", &Metrics::planned_consumption);

    ImGui::End();
}
```

Add `#include <cfloat>` at the top for `FLT_MAX`.

- [ ] **Step 3: Build and check by hand**

Run: `cmake --build build -j && ./build/economy_viewer`
Expected: let it run a few hundred months, then drag the scrub bar backward —
the agent table and the histograms show that month's real state, and dragging
back to the latest month resumes cleanly. Backward seeks feel instant
(bounded by 49 months of re-simulation, ~70 ms).

- [ ] **Step 4: Update `CLAUDE.md`**

Add a "Viewer" subsection under Commands documenting:

```bash
cmake -S . -B build && cmake --build build -j   # builds economy_viewer if SDL3 is present
./build/economy_viewer
```

and one paragraph: the viewer owns the `World` and ticks it in its frame loop;
scrubbing backward is exact re-simulation from a snapshot every 50 months, not
interpolation; testable viewer logic lives in `viewer_core` and is covered by
`tests/test_viewer_*.cpp`; steering (live parameter mutation) is deliberately
not implemented, because `params.hpp` is `constexpr` and mutating it needs its
own answer for determinism.

- [ ] **Step 5: Commit**

```bash
git add viewer/app.hpp viewer/app.cpp viewer/panel_time.cpp CLAUDE.md
git commit -m "feat(viewer): add time series and an exact scrub bar"
```

---

## Deferred, deliberately

Named here so a later reader knows they were decided rather than forgotten:

- **Steering** — needs a runtime `Params` threaded through the decision
  modules and an intervention log to preserve determinism. Its own spec.
- **Spatial rendering of the network** — the "watch it become real" goal.
  Phase B's data access is its prerequisite; it is not blocked by anything
  in this plan.
- **Full-history sparklines per agent** (spec §4.3's 512-month per-agent ring)
  — the scrub bar covers the "what was this firm doing at month N" question
  well enough that the extra 18 MB ring should wait until it is missed.
