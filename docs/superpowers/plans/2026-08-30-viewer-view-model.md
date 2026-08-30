# Viewer View-Model Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move every display-independent decision out of the ImGui panels into a testable view-model layer in `viewer_core`, so the display technology can be replaced without re-deriving what is worth showing.

**Architecture:** Pull-based. `viewer_core` gains a cell vocabulary plus model functions that materialize the column schema and the filtered/sorted row list, and serve cells on demand by `(agent, column)`. `panel_*.cpp` keep only drawing. Nothing in `viewer_core` may include ImGui or SDL.

**Tech Stack:** C++23, CMake, doctest. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-08-30-viewer-view-model-design.md`

## Global Constraints

- Build must stay warning-clean under `-Wall -Wextra -Wpedantic` (invariant 6). The `-w` relaxation applies to the vendored `imgui` target only.
- `viewer_core` must contain zero `imgui` and zero `SDL` references. Verify with `grep -rn "imgui\|SDL" viewer/cell.hpp viewer/model_*.{hpp,cpp}` returning nothing.
- No changes to `src/`. Invariants 2 (determinism) and 5 (core I/O only through `Recorder`) are untouched; the model layer only reads `World`.
- This is a refactor. Same columns, same numbers, same behaviour on screen. A visible difference is a bug.
- Cells never carry pre-formatted strings. Formatting is the skin's job.
- New model files go in the `viewer_core` library target; `panel_*.cpp` stay in `economy_viewer`.
- Every new test file must be added to the `economy_tests` source list in `CMakeLists.txt`.
- Full suite must pass before every commit: `cmake --build build -j && ./build/economy_tests`.

---

### Task 1: The cell vocabulary

**Files:**
- Create: `viewer/cell.hpp`
- Create: `viewer/cell.cpp`
- Create: `tests/test_viewer_cell.cpp`
- Modify: `CMakeLists.txt` (add `viewer/cell.cpp` to `viewer_core`, add the test file to `economy_tests`)

**Interfaces:**
- Consumes: nothing.
- Produces: `enum class CellKind`, `struct Cell`, the `number_cell`/`count_cell`/`flag_cell`/`ref_cell`/`missing_cell` constructors, `bool cell_less(const Cell&, const Cell&)`, `enum class AgentKind`, `struct ColumnSpec`, `struct SortSpec`, `struct Filters`. Every later task depends on this header.

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_cell.cpp`:

```cpp
#include "cell.hpp"
#include "doctest.h"

TEST_CASE("every cell kind carries a sort key in number") {
    CHECK(number_cell(2.5, 2).number == 2.5);
    CHECK(count_cell(7).number == 7.0);
    CHECK(flag_cell(true).number == 1.0);
    CHECK(flag_cell(false).number == 0.0);
    CHECK(ref_cell(42u).number == 42.0);
    CHECK(ref_cell(42u).ref == 42u);
}

TEST_CASE("cells compare on their numeric key") {
    CHECK(cell_less(number_cell(1.0, 2), number_cell(2.0, 2)));
    CHECK_FALSE(cell_less(number_cell(2.0, 2), number_cell(1.0, 2)));
    CHECK_FALSE(cell_less(number_cell(1.0, 2), number_cell(1.0, 2)));
}

TEST_CASE("missing sorts below every present value, including negatives") {
    // An unemployed household has no employer. Absence is a value, and it
    // must not be ordered by whatever number happens to sit in the struct.
    CHECK(cell_less(missing_cell(), number_cell(-1000.0, 2)));
    CHECK_FALSE(cell_less(number_cell(-1000.0, 2), missing_cell()));
}

TEST_CASE("two missing cells are equivalent, so sorting them is stable") {
    CHECK_FALSE(cell_less(missing_cell(), missing_cell()));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL — `cell.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

Create `viewer/cell.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// What a cell means, independent of how any skin draws it. A skin decides
// pixels; this decides significance. Nothing display-specific belongs here.
enum class CellKind {
    Number,    // a real quantity, rendered to `precision` decimals
    Count,     // a whole number of things
    Flag,      // a boolean
    AgentRef,  // an agent index: navigable, not merely printable
    Missing,   // no value exists (an unemployed household's employer)
};

struct Cell {
    CellKind kind = CellKind::Number;
    // Populated for EVERY kind -- flags as 0/1, counts and refs as their
    // value -- so one comparator sorts any column and no skin ever sorts
    // formatted text.
    double number = 0.0;
    uint32_t ref = 0;    // AgentRef only
    int precision = 2;   // Number only
};

inline Cell number_cell(double v, int precision) {
    return Cell{CellKind::Number, v, 0u, precision};
}
inline Cell count_cell(size_t n) {
    return Cell{CellKind::Count, static_cast<double>(n), 0u, 0};
}
inline Cell flag_cell(bool b) {
    return Cell{CellKind::Flag, b ? 1.0 : 0.0, 0u, 0};
}
inline Cell ref_cell(uint32_t r) {
    return Cell{CellKind::AgentRef, static_cast<double>(r), r, 0};
}
inline Cell missing_cell() {
    return Cell{CellKind::Missing, 0.0, 0u, 0};
}

// Strict weak ordering. Missing sorts below every present value regardless
// of the numbers involved, and two Missing cells are equivalent.
bool cell_less(const Cell& a, const Cell& b);

enum class AgentKind { Firm, Household };

struct ColumnSpec {
    const char* header;
    CellKind kind;
    int precision;
    bool sortable;
};

struct SortSpec {
    int column = -1;  // -1 means unsorted: rows keep ascending index order
    bool ascending = true;
};

// Which agents the tables show. Lives here rather than in App because the
// model layer must not know a running app exists.
struct Filters {
    bool wage_at_floor = false;
    bool vacancy_open = false;
    bool inventory_zero = false;
    bool unemployed = false;
    // Non-empty means: show only these firms. Written by a histogram bin
    // click, cleared by the "clear" button.
    std::vector<uint32_t> pinned_firms;
};
```

Create `viewer/cell.cpp`:

```cpp
#include "cell.hpp"

bool cell_less(const Cell& a, const Cell& b) {
    const bool a_missing = a.kind == CellKind::Missing;
    const bool b_missing = b.kind == CellKind::Missing;
    if (a_missing || b_missing) return a_missing && !b_missing;
    return a.number < b.number;
}
```

In `CMakeLists.txt`, add `viewer/cell.cpp` to the `add_library(viewer_core ...)` source list, and `tests/test_viewer_cell.cpp` to the `economy_tests` source list.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS, with the case count up by 4.

- [ ] **Step 5: Commit**

```bash
git add viewer/cell.hpp viewer/cell.cpp tests/test_viewer_cell.cpp CMakeLists.txt
git commit -m "feat(viewer): add the cell vocabulary for the view model"
```

---

### Task 2: The firm table model

**Files:**
- Create: `viewer/model_agents.hpp`
- Create: `viewer/model_agents.cpp`
- Create: `tests/test_viewer_model_agents.cpp`
- Modify: `CMakeLists.txt` (add `viewer/model_agents.cpp` to `viewer_core`, add the test file to `economy_tests`)

**Interfaces:**
- Consumes: everything from `viewer/cell.hpp` (Task 1).
- Produces: `std::span<const ColumnSpec> firm_columns()`, `std::vector<uint32_t> firm_rows(const World&, const Filters&, SortSpec)`, `Cell firm_cell(const World&, uint32_t firm, int col)`. Task 6 calls all three.

The eight firm columns, in order, are: `firm`, `wage`, `price`, `inventory`, `staff`, `liquidity`, `vacancy`, `demand` — matching what `panel_agents.cpp` draws today, including its precisions.

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_model_agents.cpp`:

```cpp
#include <vector>

#include "doctest.h"
#include "model_agents.hpp"
#include "params.hpp"
#include "world.hpp"

namespace {

// A world small enough to reason about, with hand-set firm fields.
World tiny_world() {
    World w;
    w.firms.resize(3);
    w.households.clear();
    w.firms[0].wage = 10.0;
    w.firms[0].price = 1.0;
    w.firms[0].inventory = 5.0;
    w.firms[0].liquidity = 100.0;
    w.firms[0].has_open_vacancy = false;
    w.firms[0].demand_last_month = 7.0;

    w.firms[1].wage = params::kMinWage;
    w.firms[1].price = 2.0;
    w.firms[1].inventory = 0.0;
    w.firms[1].liquidity = 50.0;
    w.firms[1].has_open_vacancy = true;
    w.firms[1].demand_last_month = 9.0;

    w.firms[2].wage = 30.0;
    w.firms[2].price = 3.0;
    w.firms[2].inventory = 2.0;
    w.firms[2].liquidity = 25.0;
    w.firms[2].has_open_vacancy = true;
    w.firms[2].demand_last_month = 1.0;
    w.firms[2].employees = {0u, 1u};
    return w;
}

}  // namespace

TEST_CASE("the schema and the cell accessor agree on column count") {
    const World w = tiny_world();
    const auto cols = firm_columns();
    REQUIRE(cols.size() == 8);
    for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
        const Cell cell = firm_cell(w, 0u, c);
        CHECK(cell.kind == cols[static_cast<size_t>(c)].kind);
    }
}

TEST_CASE("an unfiltered, unsorted table is every firm in index order") {
    const World w = tiny_world();
    const std::vector<uint32_t> rows = firm_rows(w, Filters{}, SortSpec{});
    REQUIRE(rows.size() == 3);
    CHECK(rows[0] == 0u);
    CHECK(rows[1] == 1u);
    CHECK(rows[2] == 2u);
}

TEST_CASE("each filter selects exactly the firms it names") {
    const World w = tiny_world();

    Filters at_floor;
    at_floor.wage_at_floor = true;
    const std::vector<uint32_t> floor_rows = firm_rows(w, at_floor, SortSpec{});
    REQUIRE(floor_rows.size() == 1);
    CHECK(floor_rows[0] == 1u);

    Filters vacancy;
    vacancy.vacancy_open = true;
    const std::vector<uint32_t> vacancy_rows = firm_rows(w, vacancy, SortSpec{});
    REQUIRE(vacancy_rows.size() == 2);
    CHECK(vacancy_rows[0] == 1u);
    CHECK(vacancy_rows[1] == 2u);

    Filters empty_shelf;
    empty_shelf.inventory_zero = true;
    const std::vector<uint32_t> empty_rows = firm_rows(w, empty_shelf, SortSpec{});
    REQUIRE(empty_rows.size() == 1);
    CHECK(empty_rows[0] == 1u);
}

TEST_CASE("filters intersect rather than union") {
    const World w = tiny_world();
    Filters both;
    both.vacancy_open = true;
    both.inventory_zero = true;
    const std::vector<uint32_t> rows = firm_rows(w, both, SortSpec{});
    REQUIRE(rows.size() == 1);
    CHECK(rows[0] == 1u);
}

TEST_CASE("pinned firms restrict the table to that set") {
    const World w = tiny_world();
    Filters pinned;
    pinned.pinned_firms = {2u, 0u};
    const std::vector<uint32_t> rows = firm_rows(w, pinned, SortSpec{});
    REQUIRE(rows.size() == 2);
    // Pinning restricts membership; it does not impose the pin's order.
    CHECK(rows[0] == 0u);
    CHECK(rows[1] == 2u);
}

TEST_CASE("sorting works in both directions on a numeric column") {
    const World w = tiny_world();
    const int wage_col = 1;

    const std::vector<uint32_t> asc = firm_rows(w, Filters{}, SortSpec{wage_col, true});
    REQUIRE(asc.size() == 3);
    CHECK(asc[0] == 1u);  // kMinWage
    CHECK(asc[2] == 2u);  // 30.0

    const std::vector<uint32_t> desc = firm_rows(w, Filters{}, SortSpec{wage_col, false});
    CHECK(desc[0] == 2u);
    CHECK(desc[2] == 1u);
}

TEST_CASE("sorting a flag column orders false before true") {
    const World w = tiny_world();
    const int vacancy_col = 6;
    const std::vector<uint32_t> rows = firm_rows(w, Filters{}, SortSpec{vacancy_col, true});
    CHECK(rows[0] == 0u);  // the only firm with no vacancy
}

TEST_CASE("staff is a count of employees") {
    const World w = tiny_world();
    const Cell staff = firm_cell(w, 2u, 4);
    CHECK(staff.kind == CellKind::Count);
    CHECK(staff.number == 2.0);
}

TEST_CASE("the firm id column is a navigable reference") {
    const World w = tiny_world();
    const Cell id = firm_cell(w, 2u, 0);
    CHECK(id.kind == CellKind::AgentRef);
    CHECK(id.ref == 2u);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL — `model_agents.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

Create `viewer/model_agents.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "cell.hpp"

class World;

// The firm table's schema. Column identity is the index into this span.
std::span<const ColumnSpec> firm_columns();

// The firms to show, filtered and sorted. Returns agent indices -- the
// materialized part of the view model. Cells are pulled per row on demand,
// so a skin that draws only visible rows formats only visible rows.
std::vector<uint32_t> firm_rows(const World& w, const Filters& f, SortSpec sort);

// One cell. `col` indexes firm_columns(). Out-of-range columns return the
// last column rather than reading out of bounds.
Cell firm_cell(const World& w, uint32_t firm, int col);
```

Create `viewer/model_agents.cpp`:

```cpp
#include "model_agents.hpp"

#include <algorithm>

#include "agents.hpp"
#include "params.hpp"
#include "world.hpp"

namespace {

constexpr ColumnSpec kFirmColumns[] = {
    {"firm",      CellKind::AgentRef, 0, true},
    {"wage",      CellKind::Number,   2, true},
    {"price",     CellKind::Number,   3, true},
    {"inventory", CellKind::Number,   1, true},
    {"staff",     CellKind::Count,    0, true},
    {"liquidity", CellKind::Number,   1, true},
    {"vacancy",   CellKind::Flag,     0, true},
    {"demand",    CellKind::Number,   1, true},
};

bool firm_passes(const Filters& f, uint32_t i, const Firm& firm) {
    if (!f.pinned_firms.empty() &&
        std::find(f.pinned_firms.begin(), f.pinned_firms.end(), i) == f.pinned_firms.end()) {
        return false;
    }
    if (f.wage_at_floor && firm.wage > params::kMinWage) return false;
    if (f.vacancy_open && !firm.has_open_vacancy) return false;
    if (f.inventory_zero && firm.inventory > 0.0) return false;
    return true;
}

}  // namespace

std::span<const ColumnSpec> firm_columns() {
    return std::span<const ColumnSpec>(kFirmColumns);
}

Cell firm_cell(const World& w, uint32_t firm, int col) {
    const Firm& f = w.firms[firm];
    switch (col) {
        case 0: return ref_cell(firm);
        case 1: return number_cell(f.wage, 2);
        case 2: return number_cell(f.price, 3);
        case 3: return number_cell(f.inventory, 1);
        case 4: return count_cell(f.employees.size());
        case 5: return number_cell(f.liquidity, 1);
        case 6: return flag_cell(f.has_open_vacancy);
        default: return number_cell(f.demand_last_month, 1);
    }
}

std::vector<uint32_t> firm_rows(const World& w, const Filters& f, SortSpec sort) {
    std::vector<uint32_t> rows;
    rows.reserve(w.firms.size());
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        if (firm_passes(f, i, w.firms[i])) rows.push_back(i);
    }

    const int columns = static_cast<int>(firm_columns().size());
    if (sort.column < 0 || sort.column >= columns) return rows;

    // Stable, so equal keys keep index order and the table does not shuffle
    // under the reader between frames.
    std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
        const Cell ca = firm_cell(w, a, sort.column);
        const Cell cb = firm_cell(w, b, sort.column);
        return sort.ascending ? cell_less(ca, cb) : cell_less(cb, ca);
    });
    return rows;
}
```

In `CMakeLists.txt`, add `viewer/model_agents.cpp` to `viewer_core` and `tests/test_viewer_model_agents.cpp` to `economy_tests`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add viewer/model_agents.hpp viewer/model_agents.cpp tests/test_viewer_model_agents.cpp CMakeLists.txt
git commit -m "feat(viewer): add the firm table view model"
```

---

### Task 3: The household table model

**Files:**
- Modify: `viewer/model_agents.hpp`
- Modify: `viewer/model_agents.cpp`
- Modify: `tests/test_viewer_model_agents.cpp`

**Interfaces:**
- Consumes: `viewer/cell.hpp` (Task 1) and the file layout from Task 2.
- Produces: `std::span<const ColumnSpec> household_columns()`, `std::vector<uint32_t> household_rows(const World&, const Filters&, SortSpec)`, `Cell household_cell(const World&, uint32_t household, int col)`. Task 6 calls all three.

The five household columns, in order: `household`, `employer`, `liquidity`, `reservation wage`, `planned consumption`. The household table is not sortable in the current UI, so every `ColumnSpec.sortable` here is `false` — but `household_rows` still honours a `SortSpec`, because sortability is a skin's choice and the model should not foreclose it.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_viewer_model_agents.cpp`:

```cpp
namespace {

World household_world() {
    World w;
    w.firms.resize(2);
    w.households.resize(3);
    w.households[0].employer = 1u;
    w.households[0].liquidity = 10.0;
    w.households[0].reservation_wage = 5.0;
    w.households[0].planned_consumption = 2.0;

    w.households[1].employer = NO_FIRM;  // unemployed
    w.households[1].liquidity = 20.0;
    w.households[1].reservation_wage = 6.0;
    w.households[1].planned_consumption = 3.0;

    w.households[2].employer = 0u;
    w.households[2].liquidity = 30.0;
    w.households[2].reservation_wage = 7.0;
    w.households[2].planned_consumption = 4.0;
    return w;
}

}  // namespace

TEST_CASE("the household schema and cell accessor agree on column count") {
    const World w = household_world();
    const auto cols = household_columns();
    REQUIRE(cols.size() == 5);
    for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
        const Cell cell = household_cell(w, 0u, c);
        CHECK(cell.kind == cols[static_cast<size_t>(c)].kind);
    }
}

TEST_CASE("an unemployed household's employer is Missing, not a sentinel") {
    const World w = household_world();
    const Cell employed = household_cell(w, 0u, 1);
    CHECK(employed.kind == CellKind::AgentRef);
    CHECK(employed.ref == 1u);

    const Cell jobless = household_cell(w, 1u, 1);
    CHECK(jobless.kind == CellKind::Missing);
}

TEST_CASE("the unemployed filter selects only the jobless") {
    const World w = household_world();
    Filters f;
    f.unemployed = true;
    const std::vector<uint32_t> rows = household_rows(w, f, SortSpec{});
    REQUIRE(rows.size() == 1);
    CHECK(rows[0] == 1u);
}

TEST_CASE("sorting households on employer puts the unemployed first") {
    // The point of Missing: 0xFFFFFFFF would otherwise sort the jobless to
    // the top as if they had the highest-numbered employer in the world.
    const World w = household_world();
    const std::vector<uint32_t> rows = household_rows(w, Filters{}, SortSpec{1, true});
    REQUIRE(rows.size() == 3);
    CHECK(rows[0] == 1u);  // unemployed
    CHECK(rows[1] == 2u);  // employer 0
    CHECK(rows[2] == 0u);  // employer 1
}
```

Add `#include "agents.hpp"` to the test file's includes if it is not already present (it needs `NO_FIRM`).

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL — `household_columns` was not declared in this scope.

- [ ] **Step 3: Write minimal implementation**

Append to `viewer/model_agents.hpp`:

```cpp
// The household table's schema. Column identity is the index into this span.
std::span<const ColumnSpec> household_columns();

std::vector<uint32_t> household_rows(const World& w, const Filters& f, SortSpec sort);

Cell household_cell(const World& w, uint32_t household, int col);
```

In `viewer/model_agents.cpp`, add to the anonymous namespace:

```cpp
// Not sortable: the current table offers no sort. household_rows still
// honours a SortSpec -- whether to expose sorting is a skin's decision, and
// the model should not foreclose it.
constexpr ColumnSpec kHouseholdColumns[] = {
    {"household",           CellKind::AgentRef, 0, false},
    {"employer",            CellKind::AgentRef, 0, false},
    {"liquidity",           CellKind::Number,   2, false},
    {"reservation wage",    CellKind::Number,   2, false},
    {"planned consumption", CellKind::Number,   2, false},
};
```

and append the implementations:

```cpp
std::span<const ColumnSpec> household_columns() {
    return std::span<const ColumnSpec>(kHouseholdColumns);
}

Cell household_cell(const World& w, uint32_t household, int col) {
    const Household& h = w.households[household];
    switch (col) {
        case 0: return ref_cell(household);
        // Absence as a value. The sentinel must never reach a skin, which
        // would print 4294967295 or sort it as the largest employer index.
        case 1: return h.employed() ? ref_cell(h.employer) : missing_cell();
        case 2: return number_cell(h.liquidity, 2);
        case 3: return number_cell(h.reservation_wage, 2);
        default: return number_cell(h.planned_consumption, 2);
    }
}

std::vector<uint32_t> household_rows(const World& w, const Filters& f, SortSpec sort) {
    std::vector<uint32_t> rows;
    rows.reserve(w.households.size());
    for (uint32_t i = 0; i < w.households.size(); ++i) {
        if (f.unemployed && w.households[i].employed()) continue;
        rows.push_back(i);
    }

    const int columns = static_cast<int>(household_columns().size());
    if (sort.column < 0 || sort.column >= columns) return rows;

    std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
        const Cell ca = household_cell(w, a, sort.column);
        const Cell cb = household_cell(w, b, sort.column);
        return sort.ascending ? cell_less(ca, cb) : cell_less(cb, ca);
    });
    return rows;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add viewer/model_agents.hpp viewer/model_agents.cpp tests/test_viewer_model_agents.cpp
git commit -m "feat(viewer): add the household table view model"
```

---

### Task 4: The detail model

**Files:**
- Modify: `viewer/model_agents.hpp`
- Modify: `viewer/model_agents.cpp`
- Modify: `tests/test_viewer_model_agents.cpp`

**Interfaces:**
- Consumes: `viewer/cell.hpp` (Task 1), the file layout from Tasks 2-3.
- Produces: `struct DetailField`, `struct DetailLink`, `struct LinkGroup`, `struct DetailModel`, `DetailModel firm_detail(const World&, uint32_t)`, `DetailModel household_detail(const World&, uint32_t)`. Task 6 calls both functions.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_viewer_model_agents.cpp`:

```cpp
TEST_CASE("a firm detail lists its fields and its employees as links") {
    World w = tiny_world();
    w.firms[2].demand_this_month = 4.0;
    w.firms[2].months_fully_staffed = 3u;
    w.firms[2].pending_fire = 1u;

    const DetailModel d = firm_detail(w, 2u);
    REQUIRE(d.present);
    CHECK(d.kind == AgentKind::Firm);
    CHECK(d.id == 2u);
    CHECK_FALSE(d.fields.empty());

    REQUIRE(d.groups.size() == 1);
    CHECK(d.groups[0].links.size() == 2);
    CHECK(d.groups[0].links[0].target == 0u);
    CHECK(d.groups[0].links[0].kind == AgentKind::Household);
    CHECK_FALSE(d.groups[0].links[0].has_annotation);
}

TEST_CASE("a firm with no pending fire omits that field") {
    World w = tiny_world();
    w.firms[0].pending_fire = NO_HOUSEHOLD;
    const DetailModel absent = firm_detail(w, 0u);

    w.firms[0].pending_fire = 1u;
    const DetailModel present = firm_detail(w, 0u);

    CHECK(present.fields.size() == absent.fields.size() + 1);
}

TEST_CASE("an out-of-range or sentinel selection yields an absent detail") {
    const World w = tiny_world();
    CHECK_FALSE(firm_detail(w, NO_FIRM).present);
    CHECK_FALSE(firm_detail(w, 99u).present);
    CHECK_FALSE(household_detail(w, NO_HOUSEHOLD).present);
}

TEST_CASE("a household detail annotates shops with unmet demand") {
    World w = household_world();
    w.households[0].shops[0] = 1u;
    w.households[0].unsatisfied[0] = 2.5;
    w.households[0].unsatisfied[1] = 0.0;

    const DetailModel d = household_detail(w, 0u);
    REQUIRE(d.present);
    CHECK(d.kind == AgentKind::Household);

    // Two groups: the employer, then the shops.
    REQUIRE(d.groups.size() == 2);
    REQUIRE(d.groups[0].links.size() == 1);
    CHECK(d.groups[0].links[0].target == 1u);
    CHECK(d.groups[0].links[0].kind == AgentKind::Firm);

    const LinkGroup& shops = d.groups[1];
    REQUIRE(shops.links.size() == params::kShopsPerHousehold);
    CHECK(shops.links[0].target == 1u);
    CHECK(shops.links[0].has_annotation);
    CHECK(shops.links[0].annotation == doctest::Approx(2.5));
    CHECK_FALSE(shops.links[1].has_annotation);
}

TEST_CASE("an unemployed household has no employer link group") {
    const World w = household_world();
    const DetailModel d = household_detail(w, 1u);
    REQUIRE(d.present);
    REQUIRE(d.groups.size() == 1);  // shops only
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL — `DetailModel` does not name a type.

- [ ] **Step 3: Write minimal implementation**

Append to `viewer/model_agents.hpp`:

```cpp
struct DetailField {
    const char* label;
    Cell value;
};

// A navigable reference out of a detail view. The annotation carries a
// number that belongs to the link rather than to the target -- a shop's
// unmet demand, for instance.
struct DetailLink {
    uint32_t target;
    AgentKind kind;
    bool has_annotation;
    double annotation;
};

struct LinkGroup {
    const char* label;
    std::vector<DetailLink> links;
};

// What a skin needs to render the selected agent. The link graph is data;
// only the button is ImGui.
struct DetailModel {
    bool present = false;  // false when nothing valid is selected
    AgentKind kind = AgentKind::Firm;
    uint32_t id = 0;
    std::vector<DetailField> fields;
    std::vector<LinkGroup> groups;
};

DetailModel firm_detail(const World& w, uint32_t firm);
DetailModel household_detail(const World& w, uint32_t household);
```

Append to `viewer/model_agents.cpp`:

```cpp
DetailModel firm_detail(const World& w, uint32_t firm) {
    DetailModel d;
    if (firm == NO_FIRM || firm >= w.firms.size()) return d;

    const Firm& f = w.firms[firm];
    d.present = true;
    d.kind = AgentKind::Firm;
    d.id = firm;
    d.fields = {
        {"wage", number_cell(f.wage, 4)},
        {"price", number_cell(f.price, 4)},
        {"inventory", number_cell(f.inventory, 3)},
        {"liquidity", number_cell(f.liquidity, 3)},
        {"demand this month", number_cell(f.demand_this_month, 2)},
        {"demand last month", number_cell(f.demand_last_month, 2)},
        {"months fully staffed", count_cell(f.months_fully_staffed)},
        {"vacancy open", flag_cell(f.has_open_vacancy)},
        {"vacancy went unfilled", flag_cell(f.vacancy_went_unfilled)},
    };
    if (f.pending_fire != NO_HOUSEHOLD) {
        d.fields.push_back({"pending fire", ref_cell(f.pending_fire)});
    }

    LinkGroup employees{"employees", {}};
    employees.links.reserve(f.employees.size());
    for (uint32_t e : f.employees) {
        employees.links.push_back({e, AgentKind::Household, false, 0.0});
    }
    d.groups.push_back(std::move(employees));
    return d;
}

DetailModel household_detail(const World& w, uint32_t household) {
    DetailModel d;
    if (household == NO_HOUSEHOLD || household >= w.households.size()) return d;

    const Household& h = w.households[household];
    d.present = true;
    d.kind = AgentKind::Household;
    d.id = household;
    d.fields = {
        {"liquidity", number_cell(h.liquidity, 4)},
        {"reservation wage", number_cell(h.reservation_wage, 4)},
        {"planned consumption", number_cell(h.planned_consumption, 4)},
        {"employer", h.employed() ? ref_cell(h.employer) : missing_cell()},
    };

    if (h.employed()) {
        d.groups.push_back({"employer", {{h.employer, AgentKind::Firm, false, 0.0}}});
    }

    LinkGroup shops{"shops", {}};
    shops.links.reserve(h.shops.size());
    for (size_t s = 0; s < h.shops.size(); ++s) {
        const double unmet = h.unsatisfied[s];
        shops.links.push_back({h.shops[s], AgentKind::Firm, unmet > 0.0, unmet});
    }
    d.groups.push_back(std::move(shops));
    return d;
}
```

Add `#include <utility>` to `viewer/model_agents.cpp` for `std::move`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add viewer/model_agents.hpp viewer/model_agents.cpp tests/test_viewer_model_agents.cpp
git commit -m "feat(viewer): add the agent detail view model"
```

---

### Task 5: The distribution and series models

**Files:**
- Create: `viewer/model_dist.hpp`
- Create: `viewer/model_dist.cpp`
- Create: `viewer/model_time.hpp`
- Create: `viewer/model_time.cpp`
- Create: `tests/test_viewer_model_views.cpp`
- Modify: `CMakeLists.txt` (add both `.cpp` files to `viewer_core`, add the test file to `economy_tests`)

**Interfaces:**
- Consumes: `viewer/cell.hpp` (Task 1) for `AgentKind`; `viewer/ring_recorder.hpp` and `src/recorder.hpp` for `Metrics`.
- Produces: `struct DistSpec`, `std::span<const DistSpec> distributions()`, `std::vector<double> dist_values(const World&, int which)`; `struct SeriesSpec`, `std::span<const SeriesSpec> series()`, `std::vector<double> series_values(const RingRecorder&, const SeriesSpec&)`. Task 6 calls all of them.

The seven distributions, in order, matching `panel_dist.cpp` today: firm wage, firm price, firm inventory, firm headcount, firm liquidity (all `Firm`, pinnable), then household liquidity and household reservation wage (both `Household`, not pinnable). The six series, matching `panel_time.cpp`: unemployment, mean wage, mean price, gini, inventory, planned consumption.

- [ ] **Step 1: Write the failing test**

Create `tests/test_viewer_model_views.cpp`:

```cpp
#include <vector>

#include "doctest.h"
#include "histogram.hpp"
#include "model_dist.hpp"
#include "model_time.hpp"
#include "recorder.hpp"
#include "ring_recorder.hpp"
#include "world.hpp"

TEST_CASE("every distribution yields one value per agent of its kind") {
    World w;
    w.firms.resize(4);
    w.households.resize(6);

    const auto specs = distributions();
    REQUIRE(specs.size() == 7);
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        const std::vector<double> v = dist_values(w, i);
        const size_t expected =
            specs[static_cast<size_t>(i)].over == AgentKind::Firm ? 4u : 6u;
        CHECK(v.size() == expected);
    }
}

TEST_CASE("only firm distributions are pinnable") {
    // Pinning filters the firm table, so a household bin has nothing to pin.
    for (const DistSpec& s : distributions()) {
        if (s.over == AgentKind::Household) CHECK_FALSE(s.pinnable);
    }
}

TEST_CASE("a distribution's bin members index back into the agent array") {
    World w;
    w.firms.resize(3);
    w.firms[0].wage = 0.0;
    w.firms[1].wage = 0.0;
    w.firms[2].wage = 10.0;

    const std::vector<double> wages = dist_values(w, 0);
    const Histogram h = bin(wages, 2);
    REQUIRE(h.members.front().size() == 2);
    CHECK(h.members.front()[0] == 0u);
    CHECK(h.members.front()[1] == 1u);
    REQUIRE(h.members.back().size() == 1);
    CHECK(h.members.back()[0] == 2u);
}

TEST_CASE("series read the months the ring retains, oldest first") {
    RingRecorder r(4);
    for (int m = 0; m < 3; ++m) {
        Metrics metrics;
        metrics.month = m;
        metrics.unemployment_rate = 0.1 * m;
        r.record(metrics);
    }

    const auto specs = series();
    REQUIRE(specs.size() == 6);
    const std::vector<double> v = series_values(r, specs[0]);
    REQUIRE(v.size() == 3);
    CHECK(v[0] == doctest::Approx(0.0));
    CHECK(v[2] == doctest::Approx(0.2));
}

TEST_CASE("an empty ring yields an empty series rather than throwing") {
    const RingRecorder r(4);
    CHECK(series_values(r, series()[0]).empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL — `model_dist.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

Create `viewer/model_dist.hpp`:

```cpp
#pragma once

#include <span>
#include <vector>

#include "cell.hpp"

class World;

// One distribution the viewer can show. `pinnable` means a bin click can
// restrict the firm table to that bin's members.
struct DistSpec {
    const char* name;
    AgentKind over;
    bool pinnable;
};

std::span<const DistSpec> distributions();

// The values behind distributions()[which], one per agent, in agent-index
// order -- so a Histogram's members index straight back into the world.
std::vector<double> dist_values(const World& w, int which);
```

Create `viewer/model_dist.cpp`:

```cpp
#include "model_dist.hpp"

#include "agents.hpp"
#include "world.hpp"

namespace {

constexpr DistSpec kDists[] = {
    {"firm wage",                  AgentKind::Firm,      true},
    {"firm price",                 AgentKind::Firm,      true},
    {"firm inventory",             AgentKind::Firm,      true},
    {"firm headcount",             AgentKind::Firm,      true},
    {"firm liquidity",             AgentKind::Firm,      true},
    {"household liquidity",        AgentKind::Household, false},
    {"household reservation wage", AgentKind::Household, false},
};

}  // namespace

std::span<const DistSpec> distributions() {
    return std::span<const DistSpec>(kDists);
}

std::vector<double> dist_values(const World& w, int which) {
    std::vector<double> v;
    switch (which) {
        case 0:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.wage);
            break;
        case 1:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.price);
            break;
        case 2:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.inventory);
            break;
        case 3:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) {
                v.push_back(static_cast<double>(f.employees.size()));
            }
            break;
        case 4:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.liquidity);
            break;
        case 5:
            v.reserve(w.households.size());
            for (const Household& h : w.households) v.push_back(h.liquidity);
            break;
        default:
            v.reserve(w.households.size());
            for (const Household& h : w.households) v.push_back(h.reservation_wage);
            break;
    }
    return v;
}
```

Create `viewer/model_time.hpp`:

```cpp
#pragma once

#include <span>
#include <vector>

#include "recorder.hpp"

class RingRecorder;

// One plottable series. The member pointer names the quantity without
// naming a plotting library.
struct SeriesSpec {
    const char* name;
    double Metrics::* field;
};

std::span<const SeriesSpec> series();

// The retained months of one series, oldest first. Empty if nothing has
// been recorded since the last seek.
std::vector<double> series_values(const RingRecorder& r, const SeriesSpec& s);
```

Create `viewer/model_time.cpp`:

```cpp
#include "model_time.hpp"

#include "ring_recorder.hpp"

namespace {

constexpr SeriesSpec kSeries[] = {
    {"unemployment",        &Metrics::unemployment_rate},
    {"mean wage",           &Metrics::mean_wage},
    {"mean price",          &Metrics::mean_price},
    {"gini",                &Metrics::gini},
    {"inventory",           &Metrics::inventory},
    {"planned consumption", &Metrics::planned_consumption},
};

}  // namespace

std::span<const SeriesSpec> series() {
    return std::span<const SeriesSpec>(kSeries);
}

std::vector<double> series_values(const RingRecorder& r, const SeriesSpec& s) {
    std::vector<double> v;
    v.reserve(r.size());
    for (size_t i = 0; i < r.size(); ++i) v.push_back(r.at(i).*s.field);
    return v;
}
```

In `CMakeLists.txt`, add `viewer/model_dist.cpp` and `viewer/model_time.cpp` to `viewer_core`, and `tests/test_viewer_model_views.cpp` to `economy_tests`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build -j && ./build/economy_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add viewer/model_dist.hpp viewer/model_dist.cpp viewer/model_time.hpp viewer/model_time.cpp tests/test_viewer_model_views.cpp CMakeLists.txt
git commit -m "feat(viewer): add the distribution and series view models"
```

---

### Task 6: Rewire the panels onto the view model

**Files:**
- Modify: `viewer/app.hpp` (remove the nested `Filters` struct; include `cell.hpp` and hold a `Filters` by value)
- Modify: `viewer/panel_agents.cpp` (rewrite onto the table and detail models)
- Modify: `viewer/panel_dist.cpp` (rewrite onto `distributions()`/`dist_values`)
- Modify: `viewer/panel_time.cpp` (rewrite onto `series()`/`series_values`)
- Modify: `CLAUDE.md` (document the layer and the updated test count)

**Interfaces:**
- Consumes: everything produced by Tasks 1-5.
- Produces: nothing new. This task deletes the display-independent logic from the panels; it must not add any.

This is the task where the refactor becomes real, and the acceptance test is **no visible change**. Same columns, same precisions, same filters, same sort behaviour, same distributions, same series.

- [ ] **Step 1: Move `Filters` out of `App`**

In `viewer/app.hpp`, delete the nested `struct Filters { ... }` definition and add `#include "cell.hpp"`. Replace the member with:

```cpp
    Filters filters;
```

`App::filters.pinned_firms` and the four booleans keep their names, so every use site keeps compiling.

- [ ] **Step 2: Rewrite the cell renderer and the firm table**

In `viewer/panel_agents.cpp`, delete `firm_passes` and the sort comparator entirely — they now live in `model_agents.cpp`. Add one helper that turns a `Cell` into ImGui output, which is the only thing this file should now know how to do:

```cpp
// The whole skin, in one function: a Cell in, pixels out.
void draw_cell(App& app, const Cell& c, AgentKind kind, uint32_t row_id, int col) {
    char buf[32];
    switch (c.kind) {
        case CellKind::Missing:
            ImGui::TextUnformatted("-");
            break;
        case CellKind::Flag:
            ImGui::TextUnformatted(c.number != 0.0 ? "yes" : "");
            break;
        case CellKind::Count:
            ImGui::Text("%d", static_cast<int>(c.number));
            break;
        case CellKind::AgentRef:
            if (col == 0) {
                // The id column selects the row rather than navigating away.
                std::snprintf(buf, sizeof(buf), "%u", c.ref);
                if (ImGui::Selectable(buf, kind == AgentKind::Firm
                                               ? app.selected_firm == row_id
                                               : app.selected_household == row_id,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (kind == AgentKind::Firm) {
                        app.selected_firm = row_id;
                    } else {
                        app.selected_household = row_id;
                    }
                }
            } else {
                ImGui::Text("%u", c.ref);
            }
            break;
        case CellKind::Number:
            std::snprintf(buf, sizeof(buf), "%%.%df", c.precision);
            ImGui::Text(buf, c.number);
            break;
    }
}
```

Note: `ImGui::Text` with a runtime format string will trip `-Wformat-nonliteral` if it is enabled. If the build warns, replace the `Number` case with:

```cpp
        case CellKind::Number:
            std::snprintf(buf, sizeof(buf), "%.*f", c.precision, c.number);
            ImGui::TextUnformatted(buf);
            break;
```

Prefer this second form — it is warning-proof and does the same job.

Rewrite `draw_firm_table` to read the schema and rows from the model:

```cpp
void draw_firm_table(App& app) {
    const World& w = app.sim.world();

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

    const auto cols = firm_columns();
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("firms", static_cast<int>(cols.size()), kFlags, ImVec2(0, 300))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    for (const ColumnSpec& c : cols) {
        ImGui::TableSetupColumn(c.header, c.sortable ? ImGuiTableColumnFlags_None
                                                     : ImGuiTableColumnFlags_NoSort);
    }
    ImGui::TableHeadersRow();

    // Translate ImGui's sort state into the model's, and let the model sort.
    SortSpec sort;
    if (ImGuiTableSortSpecs* s = ImGui::TableGetSortSpecs();
        s != nullptr && s->SpecsCount > 0) {
        sort.column = s->Specs[0].ColumnIndex;
        sort.ascending = s->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
    }

    for (uint32_t i : firm_rows(w, app.filters, sort)) {
        ImGui::TableNextRow();
        for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
            ImGui::TableNextColumn();
            draw_cell(app, firm_cell(w, i, c), AgentKind::Firm, i, c);
        }
    }
    ImGui::EndTable();
}
```

- [ ] **Step 3: Rewrite the household table and the detail panel**

`draw_household_table` follows the same shape, keeping the `ImGuiListClipper` — that is the whole reason cells are pulled rather than materialized. Filter first, then clip over the row vector, exactly as the current comment in that file explains:

```cpp
void draw_household_table(App& app) {
    const World& w = app.sim.world();

    ImGui::Checkbox("unemployed only", &app.filters.unemployed);

    const auto cols = household_columns();
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("households", static_cast<int>(cols.size()), kFlags,
                           ImVec2(0, 300))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    for (const ColumnSpec& c : cols) ImGui::TableSetupColumn(c.header);
    ImGui::TableHeadersRow();

    // Filter first, then clip over the filtered index vector -- ImGuiListClipper
    // sizes the scroll area from item_count x row_height, so skipping rows
    // inside the loop desyncs the scrollbar from what actually renders.
    const std::vector<uint32_t> rows = household_rows(w, app.filters, SortSpec{});

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const uint32_t i = rows[static_cast<size_t>(row)];
            ImGui::TableNextRow();
            for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
                ImGui::TableNextColumn();
                draw_cell(app, household_cell(w, i, c), AgentKind::Household, i, c);
            }
        }
    }
    ImGui::EndTable();
}
```

Rewrite `draw_detail` to render a `DetailModel` generically, so it no longer knows which fields a firm or a household has:

```cpp
void draw_one_detail(App& app, const DetailModel& d) {
    if (!d.present) return;

    ImGui::SeparatorText(d.kind == AgentKind::Firm ? "selected firm" : "selected household");
    ImGui::Text("%s %u", d.kind == AgentKind::Firm ? "firm" : "household", d.id);

    char buf[64];
    for (const DetailField& f : d.fields) {
        switch (f.value.kind) {
            case CellKind::Missing:
                ImGui::Text("%s: -", f.label);
                break;
            case CellKind::Flag:
                ImGui::Text("%s: %s", f.label, f.value.number != 0.0 ? "yes" : "no");
                break;
            case CellKind::Count:
                ImGui::Text("%s: %d", f.label, static_cast<int>(f.value.number));
                break;
            case CellKind::AgentRef:
                ImGui::Text("%s: %u", f.label, f.value.ref);
                break;
            case CellKind::Number:
                std::snprintf(buf, sizeof(buf), "%.*f", f.value.precision, f.value.number);
                ImGui::Text("%s: %s", f.label, buf);
                break;
        }
    }

    for (size_t g = 0; g < d.groups.size(); ++g) {
        const LinkGroup& group = d.groups[g];
        ImGui::Text("%s (%zu):", group.label, group.links.size());
        for (size_t i = 0; i < group.links.size(); ++i) {
            const DetailLink& link = group.links[i];
            ImGui::SameLine();
            // The ## suffix keeps ids unique when the same agent appears twice.
            std::snprintf(buf, sizeof(buf), "%u##g%zul%zu", link.target, g, i);
            if (ImGui::SmallButton(buf)) {
                if (link.kind == AgentKind::Firm) {
                    app.selected_firm = link.target;
                } else {
                    app.selected_household = link.target;
                }
            }
            if (link.has_annotation) {
                ImGui::SameLine();
                ImGui::TextDisabled("(-%.1f)", link.annotation);
            }
        }
    }
}

void draw_detail(App& app) {
    const World& w = app.sim.world();
    draw_one_detail(app, firm_detail(w, app.selected_firm));
    draw_one_detail(app, household_detail(w, app.selected_household));
}
```

Add `#include "model_agents.hpp"` to the file's includes.

- [ ] **Step 4: Rewrite the distribution and time panels**

In `viewer/panel_dist.cpp`, delete `firm_field` and drive the loop from the spec table:

```cpp
    const World& w = app.sim.world();
    const auto specs = distributions();
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        const DistSpec& s = specs[static_cast<size_t>(i)];
        draw_one(app, s.name, dist_values(w, i), effective_bins, s.pinnable);
    }
```

Keep `draw_one` and its defensive `effective_bins` floor unchanged — bar geometry and tooltips are skin. Add `#include "model_dist.hpp"`.

In `viewer/panel_time.cpp`, change `plot` to take a `SeriesSpec` and drive the calls from `series()`:

```cpp
void plot(const RingRecorder& r, const SeriesSpec& s) {
    const std::vector<double> values = series_values(r, s);
    if (values.size() < 2) {
        ImGui::TextDisabled("%s: not enough months yet", s.name);
        return;
    }
    std::vector<float> v;
    v.reserve(values.size());
    for (double d : values) v.push_back(static_cast<float>(d));
    ImGui::PlotLines(s.name, v.data(), static_cast<int>(v.size()), 0, nullptr,
                     FLT_MAX, FLT_MAX, ImVec2(0, 60));
}
```

and replace the six explicit `plot(...)` calls with:

```cpp
    for (const SeriesSpec& s : series()) plot(app.metrics, s);
```

Add `#include "model_time.hpp"`. The scrub bar is untouched.

- [ ] **Step 5: Verify the seam holds and nothing regressed**

Run:

```bash
grep -rn "imgui\|SDL" viewer/cell.hpp viewer/cell.cpp viewer/model_*.hpp viewer/model_*.cpp
```

Expected: no output. If anything matches, the seam has leaked and the task is not done.

Run: `cmake --build build -j && ./build/economy_tests`
Expected: a warning-clean build and every test passing.

Then run the viewer and confirm by eye, against the description in `CLAUDE.md`: `./build/economy_viewer`

- the firms tab shows all eight columns, sorts on a header click both ways, and the three checkboxes filter
- clicking a firm-wage bin pins the firm table; "clear" unpins
- the households tab scrolls smoothly through 1000 rows and shows `-` for the unemployed
- clicking an employee id in the detail panel selects that household, and a shop id selects that firm, with `(-n)` still shown for unmet demand
- the scrub bar still seeks, and the six series still plot

- [ ] **Step 6: Update CLAUDE.md**

In the Viewer section, after the sentence about `viewer_core`, add:

```markdown
What the viewer shows is decided in a view-model layer (`viewer/cell.hpp`,
`model_agents`, `model_dist`, `model_time`), separate from how it is drawn.
Panels render `Cell`s and never reach into `Firm`/`Household` themselves:
filters, sorting, the column schema and the detail panel's link graph are
all display-independent and tested without a window. A `Cell` carries a raw
number for every kind -- that number is the sort key -- and never a
pre-formatted string, so a future skin (ASCII, 2D, 3D) reuses the decisions
and reimplements only the drawing. See
`docs/superpowers/specs/2026-08-30-viewer-view-model-design.md`.
```

Update the test and assertion counts in the Status section to whatever `./build/economy_tests` now reports.

- [ ] **Step 7: Commit**

```bash
git add viewer/app.hpp viewer/panel_agents.cpp viewer/panel_dist.cpp viewer/panel_time.cpp CLAUDE.md
git commit -m "refactor(viewer): draw the panels from the view model"
```
