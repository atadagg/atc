# Production Economy (Piece 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firms make goods out of other goods, hold balance sheets, buy on trade credit, and are born and die — with today's economy surviving as the single-good special case.

**Architecture:** A `Good` table on `World` holds recipes. Each `Firm` makes one good. Buying is generalised so a firm purchasing iron runs the same code as a household buying bread, differing only by a per-good `commoditization` number derived from how far the good sits from the consumer. Production becomes Leontief. Trade credit settles monthly; insolvency kills firms and the write-off cascades to their suppliers.

**Tech Stack:** C++23, CMake, doctest. No new dependencies.

**Spec:** `docs/superpowers/specs/2026-09-13-production-economy-design.md`

## Global Constraints

Copied verbatim from CLAUDE.md — every task's requirements implicitly include these.

- **Invariant 1:** `ledger::transfer` is the only code that writes `liquidity`. A new module that moves money calls `transfer` or it is wrong. If the books fail to balance, that is the highest-priority bug — do not loosen a tolerance to hide it.
- **Invariant 2:** Same seed produces a byte-identical run. All randomness flows through an explicitly passed `Rng&`. No `rand()`, no globals. Do not replace `Rng::uniform`/`index`/`shuffle` with `std::uniform_int_distribution`/`std::shuffle`.
- **Invariant 3:** Indices, never pointers, between agents. Sentinel `0xFFFFFFFFu`.
- **Invariant 4:** No virtuals on agent types; `Recorder` is the sole exception. Variation arrives as fields and enums, never subclasses. `Firm` and `Household` must stay assignable, because `SimSnapshot` is a plain assignment.
- **Invariant 5:** No `std::cout` in `src/` outside `main.cpp`.
- **Invariant 6:** Warning-clean under `-Wall -Wextra -Wpedantic`.
- **Structural vs behavioural:** the test is "could a slider move it mid-run?" If no, `constexpr` in `params`. If yes, a field on `Params`. `Params` must stay a plain aggregate.
- **Do not unify the two shop-rewiring draws.** `random_unconnected_firm_by_size` (price path, size-weighted for visibility) and `random_unconnected_firm` (rationed path, unweighted) draw differently on purpose. Unifying them once concentrated two thirds of the economy into dead firms.

**Build and test commands:**

```bash
cmake -S . -B build && cmake --build build -j
./build/atc_tests
```

**The gate.** Every task from Task 2 onward ends by running `./build/atc_tests`, which includes the golden-run test built in Task 1. If the golden hash changes, the task is wrong — do not update the constant to match. The only exception is Task 14, which is allowed to change it and says so explicitly.

---

## File Structure

**Created:**

- `src/goods.hpp` — `Good`, `NO_GOOD`, `kMaxInputs`, `kSuppliersPerInput`. Recipe data only.
- `src/goods.cpp` — pure graph functions: `compute_remove`, `compute_commoditization`, `assert_acyclic`.
- `src/market.hpp` / `src/market.cpp` — per-good trade accumulation and the published reference price.
- `src/balance_sheet.hpp` / `src/balance_sheet.cpp` — computed valuation of a firm. No stored equity.
- `src/firm_lifecycle.hpp` / `src/firm_lifecycle.cpp` — insolvency, death with creditor write-off, and entry.
- `tests/test_golden_run.cpp` — the byte-exactness gate.
- `tests/test_goods.cpp`, `tests/test_market.cpp`, `tests/test_balance_sheet.cpp`, `tests/test_firm_lifecycle.cpp`, `tests/test_supply_chain.cpp`

**Modified:**

- `src/agents.hpp` — `Firm` gains `good`, `input_stock`, `suppliers`, `payables`, `receivables`, `input_cost_basis`, `output_cost_basis`.
- `src/params.hpp` — `Params` gains `commoditization_k`, `payment_terms_months`, `firm_entry_prob`, `firm_entry_margin`.
- `src/world.hpp` / `src/world.cpp` — `goods`, `markets`, and the single-consumer-good default.
- `src/ledger.hpp` — `Reason` gains `InputPurchase`, `TradeCreditSettlement`, `FirmFounding`, `FirmLiquidation`.
- `src/daily.cpp` / `src/daily.hpp` — the unified buyer, input buying, Leontief production.
- `src/network.cpp` / `src/network.hpp` — supplier links alongside shop links.
- `src/firm_decisions.cpp` — the reference-price pull.
- `src/month_close.cpp` / `src/month_close.hpp` — settlement, lifecycle, and an `Rng&` parameter.
- `src/scheduler.cpp` — passes `Rng&` to `month_close::run`.
- `CMakeLists.txt` — new sources and tests.

---

### Task 1: The golden-run gate

Capture a hash of today's simulation **before changing anything**. This constant is the contract for every later task.

**Files:**
- Create: `tests/test_golden_run.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `uint64_t golden::run_hash(const World& w)` — order-sensitive FNV-1a over every agent's state. Later tasks reuse it.

- [ ] **Step 1: Write the test with a placeholder hash**

Create `tests/test_golden_run.cpp`:

```cpp
#include <cstdint>
#include <cstring>

#include "doctest.h"
#include "scheduler.hpp"
#include "world.hpp"

namespace golden {

// FNV-1a over the raw bytes of a double, so the hash is byte-exact rather
// than tolerance-based. A tolerance here would defeat the entire point.
inline void mix(uint64_t& h, double v) {
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<uint64_t>((bits >> (i * 8)) & 0xFFu);
        h *= 1099511628211ull;
    }
}

inline void mix_u32(uint64_t& h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xFFu);
        h *= 1099511628211ull;
    }
}

inline uint64_t run_hash(const World& w) {
    uint64_t h = 14695981039346656037ull;
    for (const Firm& f : w.firms) {
        mix(h, f.liquidity);
        mix(h, f.inventory);
        mix(h, f.price);
        mix(h, f.wage);
        mix(h, f.demand_this_month);
        mix(h, f.demand_last_month);
        mix_u32(h, static_cast<uint32_t>(f.employees.size()));
        for (uint32_t e : f.employees) mix_u32(h, e);
        mix_u32(h, f.has_open_vacancy ? 1u : 0u);
        mix_u32(h, f.vacancy_went_unfilled ? 1u : 0u);
        mix_u32(h, f.months_fully_staffed);
        mix_u32(h, f.pending_fire);
    }
    for (const Household& hh : w.households) {
        mix(h, hh.liquidity);
        mix(h, hh.reservation_wage);
        mix_u32(h, hh.employer);
        for (uint32_t s : hh.shops) mix_u32(h, s);
        mix(h, hh.planned_consumption);
        for (double u : hh.unsatisfied) mix(h, u);
    }
    return h;
}

}  // namespace golden

// The byte-exactness gate for the production-economy work.
//
// Every task in the supply-chain plan must leave this hash untouched. A
// changed hash means the generalisation altered behaviour it was supposed to
// preserve. DO NOT update the constant to make a failing run pass -- that
// converts the one test that can catch a silent behavioural regression into a
// test that records whatever the code happens to do.
TEST_CASE("golden run: seed 1, 120 months, byte-identical") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler sched(w, rng);
    for (int m = 0; m < 120; ++m) sched.run_month();

    CHECK(golden::run_hash(w) == 0ull);  // placeholder, replaced in step 3
}

TEST_CASE("golden run: same seed twice agrees") {
    auto run = []() {
        Rng rng(7);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler sched(w, rng);
        for (int m = 0; m < 60; ++m) sched.run_month();
        return golden::run_hash(w);
    };
    CHECK(run() == run());
}
```

- [ ] **Step 2: Register the test and build**

In `CMakeLists.txt`, add `tests/test_golden_run.cpp` to the `atc_tests` source list, immediately after `tests/test_scheduler.cpp`.

Run: `cmake -S . -B build && cmake --build build -j`
Expected: builds clean.

- [ ] **Step 3: Run it to learn the real hash**

Run: `./build/atc_tests -tc="golden run: seed 1, 120 months, byte-identical"`
Expected: FAIL, reporting the actual hash value.

Copy the reported value and replace the `0ull` placeholder with it, written as an unsigned literal, e.g. `CHECK(golden::run_hash(w) == 12345678901234567890ull);`

- [ ] **Step 4: Verify the whole suite passes**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, 252 test cases.

- [ ] **Step 5: Commit**

```bash
git add tests/test_golden_run.cpp CMakeLists.txt
git commit -m "test: golden-run hash, the gate for the supply-chain work

Captured on unmodified code. Every task in the production-economy plan
must leave this untouched; a changed hash means the generalisation
altered behaviour it was meant to preserve.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 2: The goods table

Add the data structure and give `World` one consumer good. Pure addition — no behaviour reads it yet.

**Files:**
- Create: `src/goods.hpp`
- Modify: `src/agents.hpp`, `src/world.hpp`, `src/world.cpp`, `CMakeLists.txt`
- Test: `tests/test_goods.cpp`

**Interfaces:**
- Consumes: `golden::run_hash` from Task 1.
- Produces: `NO_GOOD`, `params::kMaxInputs`, `params::kSuppliersPerInput`, `struct Good`, `World::goods`, `Firm::good`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_goods.cpp`:

```cpp
#include "doctest.h"
#include "goods.hpp"
#include "world.hpp"

TEST_CASE("a fresh World has exactly one consumer good") {
    Rng rng(1);
    World w(10, 5, rng);

    REQUIRE(w.goods.size() == 1);
    CHECK(w.goods[0].consumer == true);
    CHECK(w.goods[0].remove == 0);
    CHECK(w.goods[0].labour_per_unit > 0.0);
    for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
        CHECK(w.goods[0].inputs[i] == NO_GOOD);
    }
}

TEST_CASE("every firm makes the consumer good by default") {
    Rng rng(1);
    World w(10, 5, rng);
    for (const Firm& f : w.firms) CHECK(f.good == 0u);
}

TEST_CASE("Good stays trivially copyable") {
    CHECK(std::is_trivially_copyable_v<Good>);
}
```

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_goods.cpp` to `CMakeLists.txt` after `tests/test_golden_run.cpp`.

Run: `cmake --build build -j`
Expected: FAIL to compile — `goods.hpp` not found.

- [ ] **Step 3: Write the implementation**

Create `src/goods.hpp`:

```cpp
#pragma once

#include <array>
#include <cstdint>

#include "params.hpp"

inline constexpr uint32_t NO_GOOD = 0xFFFFFFFFu;

// A recipe. Fixed-capacity members only: Good lives in a vector on World,
// which SimSnapshot copies by plain assignment (invariants 3 and 4).
//
// `remove` and `commoditization` are DERIVED from the graph, not authored.
// goods.cpp recomputes both whenever the graph changes. Never set by hand.
struct Good {
    std::array<uint32_t, params::kMaxInputs> inputs{};    // good indices, NO_GOOD padded
    std::array<double, params::kMaxInputs> input_qty{};   // per unit of output
    double labour_per_unit = 1.0;
    bool consumer = false;

    // Longest path to a consumer good. Consumer goods are 0; ore feeding a
    // four-deep chain is 3. Counted from the consumer end, not the raw end,
    // because distance from the consumer is the distance over which a
    // producer's choices stop being visible to anyone -- and because it makes
    // today's single good land at 0, so the current economy needs no
    // exception carved for it.
    uint8_t remove = 0;

    // In [0,1], rising with `remove`. Drives price-taking, how many sellers a
    // buyer compares, and supplier stickiness. At 0 every existing rule runs
    // unchanged.
    double commoditization = 0.0;
};
```

In `src/params.hpp`, inside the STRUCTURAL block after `kShopsPerHousehold`:

```cpp
// Recipe and supplier-link capacity. Structural for the same reason as
// kShopsPerHousehold: these size fixed arrays inside Good and Firm, and a
// runtime size would stop agents being trivially copyable.
inline constexpr uint32_t kMaxInputs = 3;
inline constexpr uint32_t kSuppliersPerInput = 3;
```

In `src/agents.hpp`, add to `Firm` after `wage`:

```cpp
    // Which good this firm makes. A firm makes exactly one.
    uint32_t good = 0;
```

In `src/world.hpp`, add the include `#include "goods.hpp"` and, after `std::vector<Household> households;`:

```cpp
    // The production graph. A fresh World holds exactly one consumer good
    // made from labour alone -- which is the economy as it was before goods
    // existed, expressed in the new vocabulary rather than special-cased.
    std::vector<Good> goods;
```

In `src/world.cpp`, in the constructor body before any agent setup:

```cpp
    Good consumer;
    consumer.inputs.fill(NO_GOOD);
    consumer.input_qty.fill(0.0);
    consumer.labour_per_unit = 1.0;
    consumer.consumer = true;
    consumer.remove = 0;
    consumer.commoditization = 0.0;
    goods.push_back(consumer);
```

Add `#include <type_traits>` to `tests/test_goods.cpp`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, including the golden-run test — no behaviour reads the new fields yet.

- [ ] **Step 5: Commit**

```bash
git add src/goods.hpp src/agents.hpp src/params.hpp src/world.hpp src/world.cpp tests/test_goods.cpp CMakeLists.txt
git commit -m "feat(goods): the recipe table, with today's good as its single entry

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 3: Remove and commoditization, computed from the graph

Pure functions over the goods table. Still nothing reads the result.

**Files:**
- Create: `src/goods.cpp`
- Modify: `src/goods.hpp`, `src/params.hpp`, `CMakeLists.txt`
- Test: `tests/test_goods.cpp`

**Interfaces:**
- Consumes: `Good`, `NO_GOOD` from Task 2.
- Produces: `goods::assert_acyclic(const std::vector<Good>&)`, `goods::recompute(std::vector<Good>&, double k)`.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_goods.cpp`:

```cpp
#include <stdexcept>
#include <vector>

namespace {

// ore -> iron -> cloth -> garments(consumer)
std::vector<Good> four_deep_chain() {
    std::vector<Good> g(4);
    for (Good& x : g) {
        x.inputs.fill(NO_GOOD);
        x.input_qty.fill(0.0);
    }
    // 0 = ore (raw), 1 = iron, 2 = cloth, 3 = garments
    g[1].inputs[0] = 0; g[1].input_qty[0] = 2.0;
    g[2].inputs[0] = 1; g[2].input_qty[0] = 1.5;
    g[3].inputs[0] = 2; g[3].input_qty[0] = 1.0;
    g[3].consumer = true;
    return g;
}

}  // namespace

TEST_CASE("remove counts steps to the nearest consumer good") {
    auto g = four_deep_chain();
    goods::recompute(g, 0.7);

    CHECK(g[3].remove == 0);  // garments: the consumer good
    CHECK(g[2].remove == 1);  // cloth
    CHECK(g[1].remove == 2);  // iron
    CHECK(g[0].remove == 3);  // ore
}

TEST_CASE("remove takes the LONGEST path when a good feeds two chains") {
    // ore feeds both iron (deep) and a consumer good directly (shallow).
    auto g = four_deep_chain();
    g.push_back(Good{});
    g[4].inputs.fill(NO_GOOD);
    g[4].input_qty.fill(0.0);
    g[4].inputs[0] = 0;
    g[4].input_qty[0] = 1.0;
    g[4].consumer = true;

    goods::recompute(g, 0.7);

    // Longest, not shortest: ore is still a deeply upstream commodity even
    // though one of its buyers sells straight to households.
    CHECK(g[0].remove == 3);
}

TEST_CASE("commoditization rises with remove and is zero at the consumer") {
    auto g = four_deep_chain();
    goods::recompute(g, 0.7);

    CHECK(g[3].commoditization == doctest::Approx(0.0));
    CHECK(g[2].commoditization > g[3].commoditization);
    CHECK(g[1].commoditization > g[2].commoditization);
    CHECK(g[0].commoditization > g[1].commoditization);
    CHECK(g[0].commoditization < 1.0);
}

TEST_CASE("a single consumer good is not a commodity") {
    Rng rng(1);
    World w(10, 5, rng);
    goods::recompute(w.goods, w.params.commoditization_k);

    CHECK(w.goods[0].remove == 0);
    CHECK(w.goods[0].commoditization == doctest::Approx(0.0));
}

TEST_CASE("a cycle in the graph is rejected") {
    std::vector<Good> g(2);
    for (Good& x : g) { x.inputs.fill(NO_GOOD); x.input_qty.fill(0.0); }
    g[0].inputs[0] = 1;
    g[1].inputs[0] = 0;
    g[1].consumer = true;

    CHECK_THROWS_AS(goods::assert_acyclic(g), std::invalid_argument);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `goods::recompute`.

- [ ] **Step 3: Write the implementation**

Add to `src/goods.hpp`, after the `Good` struct:

```cpp
#include <vector>

namespace goods {

// Throws std::invalid_argument if the recipe graph contains a cycle.
// The graph must be a DAG: a good cannot be an input to making itself.
void assert_acyclic(const std::vector<Good>& goods);

// Recomputes `remove` and `commoditization` for every good. Call after any
// change to the graph. Throws std::invalid_argument on a cycle, or if no good
// is marked `consumer` (there is then nothing to measure distance to).
void recompute(std::vector<Good>& goods, double k);

}  // namespace goods
```

Create `src/goods.cpp`:

```cpp
#include "goods.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace goods {
namespace {

// Depth-first cycle detection. 0 = unvisited, 1 = on the current path,
// 2 = finished.
void visit(const std::vector<Good>& g, uint32_t i, std::vector<uint8_t>& state) {
    if (state[i] == 1) throw std::invalid_argument("goods: recipe graph has a cycle");
    if (state[i] == 2) return;
    state[i] = 1;
    for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
        uint32_t in = g[i].inputs[s];
        if (in == NO_GOOD) continue;
        if (in >= g.size()) throw std::invalid_argument("goods: input index out of range");
        visit(g, in, state);
    }
    state[i] = 2;
}

}  // namespace

void assert_acyclic(const std::vector<Good>& g) {
    std::vector<uint8_t> state(g.size(), 0);
    for (uint32_t i = 0; i < g.size(); ++i) visit(g, i, state);
}

void recompute(std::vector<Good>& g, double k) {
    assert_acyclic(g);

    bool any_consumer = false;
    for (const Good& x : g) any_consumer = any_consumer || x.consumer;
    if (!any_consumer) {
        throw std::invalid_argument("goods: no consumer good to measure distance to");
    }

    // `remove` is the LONGEST path from a good to a consumer good -- not the
    // shortest. A good that feeds both a deep chain and a shallow one is
    // still an upstream commodity; taking the shortest path would call it
    // consumer-facing on the strength of its least typical buyer.
    //
    // Relaxation rather than a topological sort: the graph is tiny (tens of
    // goods) and acyclicity is already established, so |V| passes always
    // converge and the code stays short enough to be obviously right.
    const int unset = -1;
    std::vector<int> rem(g.size(), unset);
    for (uint32_t i = 0; i < g.size(); ++i) {
        if (g[i].consumer) rem[i] = 0;
    }

    for (std::size_t pass = 0; pass < g.size(); ++pass) {
        for (uint32_t i = 0; i < g.size(); ++i) {
            for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
                uint32_t in = g[i].inputs[s];
                if (in == NO_GOOD) continue;
                if (rem[i] == unset) continue;
                int candidate = rem[i] + 1;
                if (candidate > rem[in]) rem[in] = candidate;
            }
        }
    }

    for (uint32_t i = 0; i < g.size(); ++i) {
        // A good that reaches no consumer good is dead stock: nothing it can
        // become is ever bought. Treat it as maximally upstream rather than
        // failing the run -- innovation can strand a branch.
        int r = (rem[i] == unset) ? 255 : rem[i];
        if (r > 255) r = 255;
        g[i].remove = static_cast<uint8_t>(r);

        // 1 - exp(-k * remove): zero at the consumer, rising and saturating
        // upstream. Never reaches 1, so even ore keeps a sliver of pricing
        // discretion.
        g[i].commoditization = 1.0 - std::exp(-k * static_cast<double>(g[i].remove));
    }
}

}  // namespace goods
```

In `src/params.hpp`, add to `Params` in a new section at the end:

```cpp
    // --- Supply chain (not in Table 1) ---
    // How fast a good becomes commodity-like as it moves upstream, in
    // 1 - exp(-k * remove). Behavioural: a slider could move it mid-run, and
    // moving it re-prices the whole chain.
    double commoditization_k = 0.7;
```

Add `src/goods.cpp` to the `atc_core` source list in `CMakeLists.txt`, after `src/world.cpp`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged — nothing calls `recompute` in the tick yet.

- [ ] **Step 5: Commit**

```bash
git add src/goods.hpp src/goods.cpp src/params.hpp tests/test_goods.cpp CMakeLists.txt
git commit -m "feat(goods): remove and commoditization, derived from the graph

Longest path to a consumer good, not shortest: a good feeding both a deep
and a shallow chain is still an upstream commodity.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 4: The published reference price

Accumulate real trades per good; publish last month's volume-weighted mean at month open. Nothing consumes it yet.

**Files:**
- Create: `src/market.hpp`, `src/market.cpp`
- Modify: `src/world.hpp`, `src/world.cpp`, `src/daily.cpp`, `src/scheduler.cpp`, `CMakeLists.txt`
- Test: `tests/test_market.cpp`

**Interfaces:**
- Consumes: `World::goods` from Task 2.
- Produces: `struct GoodMarket { double traded_value; double traded_qty; double reference_price; bool has_reference; }`, `World::markets`, `market::record_trade(World&, uint32_t good, double qty, double value)`, `market::publish(World&)`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_market.cpp`:

```cpp
#include "doctest.h"
#include "market.hpp"
#include "world.hpp"

TEST_CASE("a fresh market has no reference price") {
    Rng rng(1);
    World w(10, 5, rng);
    REQUIRE(w.markets.size() == w.goods.size());
    CHECK(w.markets[0].has_reference == false);
}

TEST_CASE("the reference price is the volume-weighted mean of real trades") {
    Rng rng(1);
    World w(10, 5, rng);

    market::record_trade(w, 0, 10.0, 20.0);  // 10 units at 2.0
    market::record_trade(w, 0, 30.0, 30.0);  // 30 units at 1.0
    market::publish(w);

    // (20 + 30) / (10 + 30) = 1.25 -- weighted by volume, so the big cheap
    // trade dominates. A plain mean of 2.0 and 1.0 would say 1.5.
    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(1.25));
}

TEST_CASE("publishing clears the accumulator for the next month") {
    Rng rng(1);
    World w(10, 5, rng);

    market::record_trade(w, 0, 10.0, 20.0);
    market::publish(w);
    market::publish(w);

    // No trades in the second month: the last published price stands rather
    // than snapping to zero. A quiet month is not a free good.
    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
}

TEST_CASE("household purchases feed the consumer good's market") {
    Rng rng(1);
    World w(20, 5, rng);
    for (Firm& f : w.firms) { f.inventory = 1000.0; f.price = 2.0; }
    for (Household& h : w.households) h.planned_consumption = 21.0;

    daily::run_day(w, rng);
    market::publish(w);

    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
}
```

Add `#include "daily.hpp"` to that file.

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_market.cpp` to `CMakeLists.txt`.

Run: `cmake --build build -j`
Expected: FAIL to compile — no `market.hpp`.

- [ ] **Step 3: Write the implementation**

Create `src/market.hpp`:

```cpp
#pragma once

#include <cstdint>

class World;

namespace market {

// What a good traded at, aggregated over a month.
//
// This is a MEASUREMENT, not a clearing price. Nothing computes a price at
// which supply equals demand, and nothing guarantees an order fills at the
// published number -- it is the average of trades that actually happened,
// reported with a one-month lag. It is free to spike and free to crash.
struct GoodMarket {
    double traded_value = 0.0;   // accumulating, this month
    double traded_qty = 0.0;     // accumulating, this month
    double reference_price = 0.0;
    bool has_reference = false;  // false until a first month has traded
};

// Record one completed trade. Called from the buying path, never speculatively.
void record_trade(World& w, uint32_t good, double qty, double value);

// Turn this month's accumulation into next month's published price and reset
// the accumulator. A month with no trades leaves the previous price standing.
void publish(World& w);

}  // namespace market
```

Create `src/market.cpp`:

```cpp
#include "market.hpp"

#include "world.hpp"

namespace market {

void record_trade(World& w, uint32_t good, double qty, double value) {
    if (good >= w.markets.size()) return;
    if (qty <= 0.0) return;
    w.markets[good].traded_qty += qty;
    w.markets[good].traded_value += value;
}

void publish(World& w) {
    for (GoodMarket& m : w.markets) {
        if (m.traded_qty > 0.0) {
            m.reference_price = m.traded_value / m.traded_qty;
            m.has_reference = true;
        }
        // A month with no trades leaves the last price standing. Snapping to
        // zero would tell every buyer the good had become free.
        m.traded_qty = 0.0;
        m.traded_value = 0.0;
    }
}

}  // namespace market
```

In `src/world.hpp`, add `#include "market.hpp"` and after `goods`:

```cpp
    // One market per good, index-parallel to `goods`.
    std::vector<market::GoodMarket> markets;
```

In `src/world.cpp`, after pushing the consumer good:

```cpp
    markets.resize(goods.size());
```

In `src/daily.cpp`, inside `shop`, immediately after the existing `ledger::transfer` call in the `bought > 0.0` branch:

```cpp
            market::record_trade(w, f.good, bought, cost);
```

Add `#include "market.hpp"` to `src/daily.cpp`.

In `src/scheduler.cpp`, in `run_month`, call `market::publish(world_);` at month open — after interventions are applied and before `firm_decisions::run_monthly`. Add `#include "market.hpp"`.

Add `src/market.cpp` to `atc_core` in `CMakeLists.txt`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS. The golden hash is unchanged: recording and publishing read no RNG and write no agent state.

- [ ] **Step 5: Commit**

```bash
git add src/market.hpp src/market.cpp src/world.hpp src/world.cpp src/daily.cpp src/scheduler.cpp tests/test_market.cpp CMakeLists.txt
git commit -m "feat(market): a published reference price, measured not cleared

Volume-weighted mean of last month's real trades. No auctioneer: it never
guarantees a fill and is free to spike and crash.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 5: Price is pulled toward the reference by commoditization

**Files:**
- Modify: `src/firm_decisions.cpp`
- Test: `tests/test_firm_decisions.cpp`

**Interfaces:**
- Consumes: `World::markets`, `Good::commoditization`.
- Produces: no new names; `firm_decisions::adjust_price` gains the pull.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_firm_decisions.cpp`:

```cpp
TEST_CASE("at commoditization 0 the price rule is untouched by the reference") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 0.0;
    w.markets[0].reference_price = 99.0;
    w.markets[0].has_reference = true;

    for (Firm& f : w.firms) f.price = 1.0;
    World before = w;

    firm_decisions::adjust_price(w, rng);

    Rng rng2(3);
    World w2 = before;
    w2.markets[0].has_reference = false;  // no reference at all
    firm_decisions::adjust_price(w2, rng2);

    for (std::size_t i = 0; i < w.firms.size(); ++i) {
        CHECK(w.firms[i].price == doctest::Approx(w2.firms[i].price));
    }
}

TEST_CASE("at commoditization 1 the firm is a price taker") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 1.0;
    w.markets[0].reference_price = 5.0;
    w.markets[0].has_reference = true;
    for (Firm& f : w.firms) f.price = 1.0;

    firm_decisions::adjust_price(w, rng);

    for (const Firm& f : w.firms) CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("partial commoditization moves the price part of the way") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 0.5;
    w.markets[0].reference_price = 3.0;
    w.markets[0].has_reference = true;
    for (Firm& f : w.firms) f.price = 1.0;

    firm_decisions::adjust_price(w, rng);

    // Whatever the firm's own rule decided, the result sits strictly between
    // that and the reference -- the pull is a blend, never a jump.
    for (const Firm& f : w.firms) {
        CHECK(f.price > 1.0);
        CHECK(f.price < 3.0);
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j && ./build/atc_tests -tc="*price taker*"`
Expected: FAIL — price stays near 1.0.

- [ ] **Step 3: Write the implementation**

At the very end of `firm_decisions::adjust_price` in `src/firm_decisions.cpp`, after the existing gated adjustment has set `f.price` for every firm, apply the pull:

```cpp
        // The commoditization pull. The firm's own gated rule (raise only
        // while below the cap, lower only while above the floor -- see
        // CLAUDE.md, it is gated rather than clamped) decides what the firm
        // WANTS. This blends that toward what the good actually trades at,
        // weighted by how far the good sits from the consumer.
        //
        // At 0 this is the identity and today's economy is untouched. At 1
        // the firm is a pure price taker: iron ore sells at the iron ore
        // price whatever its producer would prefer. Nothing here is an
        // auctioneer -- the reference is last month's realised average, and
        // a firm at the reference price can still fail to sell.
        const Good& g = w.goods[f.good];
        const market::GoodMarket& m = w.markets[f.good];
        if (m.has_reference && g.commoditization > 0.0) {
            f.price = f.price + g.commoditization * (m.reference_price - f.price);
        }
```

Add `#include "goods.hpp"` and `#include "market.hpp"` to `src/firm_decisions.cpp`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS. The golden hash is unchanged: the single consumer good has `commoditization == 0.0`, so the branch never fires.

- [ ] **Step 5: Commit**

```bash
git add src/firm_decisions.cpp tests/test_firm_decisions.cpp
git commit -m "feat(price): pull toward the reference price by commoditization

At 0 the existing gated rule is untouched; at 1 the firm is a price taker.
One dial, not two pricing systems.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 6: The unified buyer

Behaviour-preserving refactor. This is the highest-risk task in the plan and exists on its own so it can be rejected on its own.

**Files:**
- Modify: `src/daily.hpp`, `src/daily.cpp`
- Test: `tests/test_daily.cpp`

**Interfaces:**
- Consumes: everything from Tasks 2-5.
- Produces: `daily::Buyer` — a value describing who is buying, so one routine serves households and firms.

```cpp
namespace daily {
// Who is buying. A tagged index (invariant 3), not a pointer and not a
// subclass (invariant 4): variation arrives as a field and an enum.
enum class BuyerKind : uint8_t { Household, Firm };
struct Buyer {
    BuyerKind kind;
    uint32_t index;
    uint32_t good;  // what it is trying to buy
};
}
```

- [ ] **Step 1: Write the characterisation test**

Append to `tests/test_daily.cpp`:

```cpp
TEST_CASE("household buying through the Buyer path is unchanged") {
    Rng rng(11);
    World w(50, 10, rng);
    for (Firm& f : w.firms) { f.inventory = 100.0; f.price = 1.5; }
    for (Household& h : w.households) h.planned_consumption = 21.0;

    World before = w;

    Rng ra(11);
    daily::run_day(w, ra);

    // The same day, driven through the explicit Buyer entry point, must
    // produce identical state. If these diverge the generalisation changed
    // behaviour it was meant to preserve.
    World w2 = before;
    Rng rb(11);
    std::vector<uint32_t> order(w2.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rb.shuffle(order);
    for (uint32_t h : order) {
        daily::buy(w2, daily::Buyer{daily::BuyerKind::Household, h, 0}, rb);
    }
    daily::produce(w2);

    for (std::size_t i = 0; i < w.firms.size(); ++i) {
        CHECK(w.firms[i].liquidity == doctest::Approx(w2.firms[i].liquidity));
        CHECK(w.firms[i].inventory == doctest::Approx(w2.firms[i].inventory));
    }
    for (std::size_t i = 0; i < w.households.size(); ++i) {
        CHECK(w.households[i].liquidity == doctest::Approx(w2.households[i].liquidity));
    }
}
```

Add `#include <numeric>` and `#include <vector>` to `tests/test_daily.cpp` if absent.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `daily::buy`, no `daily::Buyer`.

- [ ] **Step 3: Write the implementation**

In `src/daily.hpp`, add the `BuyerKind` and `Buyer` definitions shown in the Interfaces block above, plus:

```cpp
// One buyer's purchasing for one day. A firm buying iron and a household
// buying bread run this same routine; what differs between them is the good's
// commoditization number, not the code.
void buy(World& w, Buyer b, Rng& rng, ledger::Journal* journal = nullptr);
```

Keep `void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal = nullptr);` as a thin wrapper so existing callers and tests are untouched.

In `src/daily.cpp`, rename the body of `shop` to `buy` taking a `Buyer`, and introduce small accessors at the top of the anonymous namespace so the body reads the right agent:

```cpp
namespace {

// The buyer's cash account, its supplier slots, and its unmet-demand slots.
// A household shops for the consumer good; a firm buys one input good. Both
// hold a fixed-size set of links and a parallel set of shortfall counters,
// which is why one routine can serve both.
struct BuyerView {
    double* liquidity;
    uint32_t* links;
    double* unsatisfied;
    uint32_t link_count;
    ledger::Account account;
};

}  // namespace
```

Implement `BuyerView view_of(World&, Buyer)` returning the household's `shops`/`unsatisfied`/`liquidity` for `BuyerKind::Household`. The firm branch returns `NO_FIRM`-filled links and a zero count for now — Task 7 fills it in. Then rewrite the loop in terms of `BuyerView`, changing nothing else: same random slot order, same `visits` cap, same `demand_this_month` accumulation of what was *wanted*, same stock-shortfall accounting, same `market::record_trade`.

`shop` becomes:

```cpp
void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal) {
    buy(w, Buyer{BuyerKind::Household, household, 0}, rng, journal);
}
```

**Do not touch `network::random_unconnected_firm_by_size` or `network::random_unconnected_firm`.** They stay two functions that draw differently — CLAUDE.md records what happened the last time they were merged.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, and the golden hash unchanged. If the golden hash moved, the refactor is not behaviour-preserving: find the difference, do not update the constant.

- [ ] **Step 5: Commit**

```bash
git add src/daily.hpp src/daily.cpp tests/test_daily.cpp
git commit -m "refactor(daily): one buyer, households and firms alike

Behaviour-preserving: the golden hash is unchanged. A firm buying iron
will run this same routine; what differs is commoditization, not code.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 7: Supplier links and firm input buying

**Files:**
- Modify: `src/agents.hpp`, `src/network.hpp`, `src/network.cpp`, `src/daily.cpp`, `src/ledger.hpp`
- Test: `tests/test_supply_chain.cpp` (create), `CMakeLists.txt`

**Interfaces:**
- Consumes: `daily::Buyer`, `BuyerView` from Task 6.
- Produces: `Firm::suppliers` (`std::array<uint32_t, kMaxInputs * kSuppliersPerInput>`), `Firm::input_stock` (`std::array<double, kMaxInputs>`), `Firm::input_unsatisfied` (same shape as suppliers), `network::random_unconnected_producer(const World&, uint32_t firm, uint32_t good, Rng&)`, `ledger::Reason::InputPurchase`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_supply_chain.cpp`:

```cpp
#include <numeric>
#include <vector>

#include "doctest.h"
#include "daily.hpp"
#include "goods.hpp"
#include "market.hpp"
#include "world.hpp"

namespace {

// ore(0, raw) -> bread(1, consumer). Firms 0-1 make ore, firms 2-4 make bread.
World two_stage_world(Rng& rng) {
    World w(50, 5, rng);
    w.goods.resize(2);
    for (Good& g : w.goods) { g.inputs.fill(NO_GOOD); g.input_qty.fill(0.0); }
    w.goods[0].consumer = false;
    w.goods[0].labour_per_unit = 1.0;
    w.goods[1].consumer = true;
    w.goods[1].labour_per_unit = 1.0;
    w.goods[1].inputs[0] = 0;
    w.goods[1].input_qty[0] = 0.5;
    goods::recompute(w.goods, w.params.commoditization_k);
    w.markets.resize(w.goods.size());

    w.firms[0].good = 0; w.firms[1].good = 0;
    for (uint32_t i = 2; i < 5; ++i) w.firms[i].good = 1;
    return w;
}

}  // namespace

TEST_CASE("a firm with no material inputs draws no suppliers and no randomness") {
    Rng rng(5);
    World w(20, 4, rng);  // the default single-good world

    Rng a(99);
    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 0, NO_GOOD}, a);

    // A firm making a good with no inputs must consume no RNG at all --
    // otherwise every existing run's draw sequence shifts and determinism
    // across the single-good special case is lost.
    Rng b(99);
    CHECK(a.uniform() == doctest::Approx(b.uniform()));
}

TEST_CASE("a bread firm buys ore from an ore firm") {
    Rng rng(5);
    World w = two_stage_world(rng);

    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;      // bread firm 2 buys from ore firm 0
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    double ore_before = w.firms[0].inventory;
    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);

    CHECK(w.firms[0].inventory < ore_before);
    CHECK(w.firms[2].input_stock[0] > 0.0);
}

TEST_CASE("input purchases are recorded in the ore market, not the bread market") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    market::publish(w);

    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
    CHECK(w.markets[1].has_reference == false);
}

TEST_CASE("a firm's supplier draw only ever returns producers of that good") {
    Rng rng(5);
    World w = two_stage_world(rng);
    for (int i = 0; i < 50; ++i) {
        uint32_t s = network::random_unconnected_producer(w, 2, 0, rng);
        if (s == NO_FIRM) continue;
        CHECK(w.firms[s].good == 0u);
    }
}
```

Add `#include "network.hpp"` to that file.

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_supply_chain.cpp` to `CMakeLists.txt`.

Run: `cmake --build build -j`
Expected: FAIL to compile — no `Firm::suppliers`, no `network::random_unconnected_producer`.

- [ ] **Step 3: Write the implementation**

In `src/agents.hpp`, add to `Firm`:

```cpp
    // Input stock, one slot per recipe input, parallel to Good::inputs.
    std::array<double, params::kMaxInputs> input_stock{};

    // What this firm still wants of each input this month, set at month open
    // from planned output and drawn down as it buys. The firm-side analogue
    // of Household::planned_consumption.
    std::array<double, params::kMaxInputs> input_demand{};

    // Supplier links: kSuppliersPerInput slots per input, laid out
    // input-major, so input i owns slots [i*kSuppliersPerInput,
    // (i+1)*kSuppliersPerInput). NO_FIRM means the slot is empty.
    std::array<uint32_t, params::kMaxInputs * params::kSuppliersPerInput> suppliers{};

    // Unmet input demand per supplier slot, parallel to `suppliers`. Drives
    // supplier rewiring exactly as Household::unsatisfied drives shop
    // rewiring.
    std::array<double, params::kMaxInputs * params::kSuppliersPerInput> input_unsatisfied{};
```

In `src/world.cpp`, fill `suppliers` with `NO_FIRM` for every firm at construction.

In `src/ledger.hpp`, extend `Reason`:

```cpp
    InputPurchase,  // firm -> firm, a daily input buy
```

In `src/network.hpp` / `.cpp`, add:

```cpp
// Draw a firm that produces `good` and that `firm` does not already buy from,
// with probability proportional to size in employees -- the same visibility
// weighting the household price-comparison path uses, and for the same
// reason. Zero-employee producers keep weight 1 so they are not permanently
// excluded. Returns NO_FIRM if every producer is already connected.
//
// Deliberately NOT shared with random_unconnected_firm_by_size: that one
// searches all firms, this one filters by good. See CLAUDE.md on why the
// rewiring draws are kept apart.
uint32_t random_unconnected_producer(const World& w, uint32_t firm, uint32_t good, Rng& rng);
```

In `src/daily.cpp`, complete the firm branch of `view_of`: for `BuyerKind::Firm` with `b.good == NO_GOOD` return a view with `link_count == 0` so the routine returns immediately having drawn nothing. Otherwise return the firm's cash, the supplier slots for that input, and the matching `input_unsatisfied` slots. Purchases add to `input_stock[slot]` rather than reducing a consumption target, and use `ledger::Reason::InputPurchase`.

**Determinism note for the implementer:** the early return for a firm with no inputs must happen *before* any `rng` call, including the visit-order shuffle. If a no-input firm draws even one number, every existing run's sequence shifts and the golden hash breaks.

Add the daily driver: in `run_day`, after household shopping and before `produce`, iterate firms in a shuffled order and call `buy` once per input slot of their good. Guard the whole block with `if (w.goods.size() > 1)` so the single-good world consumes no extra randomness.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/agents.hpp src/network.hpp src/network.cpp src/daily.cpp src/ledger.hpp src/world.cpp tests/test_supply_chain.cpp CMakeLists.txt
git commit -m "feat(chain): firms buy inputs from firms

Same routine as household shopping. A no-input firm draws no randomness,
which is what keeps the single-good world byte-identical.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 8: Leontief production

**Files:**
- Modify: `src/daily.cpp`
- Test: `tests/test_supply_chain.cpp`

**Interfaces:**
- Consumes: `Firm::input_stock`, `Good::input_qty`, `Good::labour_per_unit`.
- Produces: no new names; `daily::produce` becomes input-limited.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_supply_chain.cpp`:

```cpp
TEST_CASE("output is limited by the scarcest input, not by labour alone") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};       // labour capacity = 3 * productivity = 9
    bread.input_stock[0] = 2.0;        // enough ore for 2 / 0.5 = 4 units

    daily::produce(w);

    CHECK(bread.inventory == doctest::Approx(4.0));
    CHECK(bread.input_stock[0] == doctest::Approx(0.0));
}

TEST_CASE("an input stockout upstream stops output downstream") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};
    bread.input_stock[0] = 0.0;

    daily::produce(w);

    CHECK(bread.inventory == doctest::Approx(0.0));
}

TEST_CASE("a firm with no material inputs still produces on labour alone") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& ore = w.firms[0];
    ore.employees = {10, 11};

    daily::produce(w);

    CHECK(ore.inventory == doctest::Approx(2.0 * w.params.productivity));
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j && ./build/atc_tests -tc="*scarcest input*"`
Expected: FAIL — output is 9.0, ignoring the input limit.

- [ ] **Step 3: Write the implementation**

Replace `daily::produce` in `src/daily.cpp`:

```cpp
void produce(World& w) {
    for (Firm& f : w.firms) {
        const Good& g = w.goods[f.good];

        double by_labour = w.params.productivity * static_cast<double>(f.employees.size());

        // Leontief: no substitution between inputs. Short of iron, extra
        // fibre does not help. That is exactly what makes a stockout travel
        // up the chain instead of being absorbed at the first firm that hits
        // it.
        double limit = by_labour;
        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            if (g.input_qty[s] <= 0.0) continue;
            limit = std::min(limit, f.input_stock[s] / g.input_qty[s]);
        }
        if (limit <= 0.0) continue;

        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            f.input_stock[s] -= limit * g.input_qty[s];
            if (f.input_stock[s] < 0.0) f.input_stock[s] = 0.0;
        }
        f.inventory += limit;
    }
}
```

Add `#include "goods.hpp"` to `src/daily.cpp` if absent.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged — with no inputs the loop never binds and `limit == by_labour`.

- [ ] **Step 5: Commit**

```bash
git add src/daily.cpp tests/test_supply_chain.cpp
git commit -m "feat(chain): Leontief production, so stockouts travel upstream

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 9: The balance sheet

**Files:**
- Create: `src/balance_sheet.hpp`, `src/balance_sheet.cpp`
- Modify: `src/agents.hpp`, `CMakeLists.txt`
- Test: `tests/test_balance_sheet.cpp`

**Interfaces:**
- Consumes: `Firm`, `World`.
- Produces: `struct Sheet { double cash, input_stock, output_stock, receivables, payables; double assets() const; double liabilities() const; double equity() const; bool insolvent() const; }`, `balance_sheet::of(const World&, uint32_t firm)`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_balance_sheet.cpp`:

```cpp
#include "doctest.h"
#include "balance_sheet.hpp"
#include "world.hpp"

TEST_CASE("equity is assets minus liabilities") {
    Rng rng(1);
    World w(10, 3, rng);
    Firm& f = w.firms[0];
    f.liquidity = 100.0;
    f.inventory = 10.0;
    f.output_cost_basis = 2.0;   // 10 units held at cost 2 = 20
    f.receivables = 5.0;
    f.payables = 30.0;

    balance_sheet::Sheet s = balance_sheet::of(w, 0);

    CHECK(s.assets() == doctest::Approx(100.0 + 20.0 + 5.0));
    CHECK(s.liabilities() == doctest::Approx(30.0));
    CHECK(s.equity() == doctest::Approx(95.0));
    CHECK(s.insolvent() == false);
}

TEST_CASE("a firm owing more than it holds is insolvent") {
    Rng rng(1);
    World w(10, 3, rng);
    Firm& f = w.firms[0];
    f.liquidity = 1.0;
    f.inventory = 0.0;
    f.payables = 50.0;

    CHECK(balance_sheet::of(w, 0).insolvent() == true);
}

TEST_CASE("a firm with no debt can never be insolvent") {
    Rng rng(1);
    World w(10, 3, rng);
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        CHECK(balance_sheet::of(w, i).insolvent() == false);
    }
}
```

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_balance_sheet.cpp` to `CMakeLists.txt`.

Run: `cmake --build build -j`
Expected: FAIL to compile — no `balance_sheet.hpp`, no `Firm::payables`.

- [ ] **Step 3: Write the implementation**

In `src/agents.hpp`, add to `Firm`:

```cpp
    // Trade credit. `payables` is owed to suppliers, `receivables` is owed by
    // customers; both settle at month close.
    double payables = 0.0;
    double receivables = 0.0;

    // Inventory is carried at cost, so equity does not move when a firm
    // merely re-prices its shelf. One moving-average cost per firm: a firm
    // makes one good, so one number is enough.
    double output_cost_basis = 0.0;
    std::array<double, params::kMaxInputs> input_cost_basis{};
```

Create `src/balance_sheet.hpp`:

```cpp
#pragma once

#include <cstdint>

class World;

namespace balance_sheet {

// A firm's position, COMPUTED from firm state. Equity is never stored --
// the same discipline as World::books_balance(), so it cannot silently drift
// away from the things it is the sum of.
struct Sheet {
    double cash = 0.0;
    double input_stock = 0.0;   // at cost
    double output_stock = 0.0;  // at cost
    double receivables = 0.0;
    double payables = 0.0;

    double assets() const { return cash + input_stock + output_stock + receivables; }
    double liabilities() const { return payables; }
    double equity() const { return assets() - liabilities(); }

    // With no debt a firm's equity is a sum of non-negative terms, so a firm
    // that has never bought on credit cannot reach this state. That is what
    // keeps the single-good world's behaviour unchanged by the lifecycle.
    bool insolvent() const { return equity() < 0.0; }
};

Sheet of(const World& w, uint32_t firm);

}  // namespace balance_sheet
```

Create `src/balance_sheet.cpp` implementing `of`, valuing `input_stock` as the sum over recipe slots of `input_stock[s] * input_cost_basis[s]` and `output_stock` as `inventory * output_cost_basis`.

Add `src/balance_sheet.cpp` to `atc_core` in `CMakeLists.txt`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged — nothing in the tick consults the sheet yet.

- [ ] **Step 5: Commit**

```bash
git add src/balance_sheet.hpp src/balance_sheet.cpp src/agents.hpp tests/test_balance_sheet.cpp CMakeLists.txt
git commit -m "feat(finance): the firm balance sheet, with equity computed not stored

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 10: Trade credit and monthly settlement

**Files:**
- Modify: `src/daily.cpp`, `src/month_close.hpp`, `src/month_close.cpp`, `src/scheduler.cpp`, `src/ledger.hpp`
- Test: `tests/test_supply_chain.cpp`, plus signature updates in `tests/test_month_close.cpp` and `tests/test_scheduler.cpp`

**Interfaces:**
- Consumes: `Firm::payables`, `Firm::receivables` from Task 9.
- Produces: `month_close::settle_trade_credit(World&, Rng&, ledger::Journal*)`; `month_close::run` gains an `Rng&` as its second parameter: `void run(World& w, Rng& rng, ledger::Journal* journal = nullptr);`.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_supply_chain.cpp`:

```cpp
TEST_CASE("an input purchase creates a payable, not an immediate payment") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    double buyer_cash = w.firms[2].liquidity;
    double seller_cash = w.firms[0].liquidity;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);

    CHECK(w.firms[2].liquidity == doctest::Approx(buyer_cash));
    CHECK(w.firms[0].liquidity == doctest::Approx(seller_cash));
    CHECK(w.firms[2].payables > 0.0);
    CHECK(w.firms[0].receivables == doctest::Approx(w.firms[2].payables));
}

TEST_CASE("settlement moves the money and clears both sides") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[2].liquidity = 1000.0;
    w.firms[2].payables = 40.0;
    w.firms[2].payable_to[0] = 0;
    w.firms[2].payable_amount[0] = 40.0;
    w.firms[0].receivables = 40.0;

    month_close::settle_trade_credit(w, rng);

    CHECK(w.firms[2].liquidity == doctest::Approx(960.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(40.0));
    CHECK(w.firms[2].payables == doctest::Approx(0.0));
    CHECK(w.firms[0].receivables == doctest::Approx(0.0));
}

TEST_CASE("the books balance with trade credit outstanding") {
    Rng rng(5);
    World w = two_stage_world(rng);
    double before = w.books_balance();

    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    CHECK(w.books_balance() == doctest::Approx(before + 1000.0));

    month_close::settle_trade_credit(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before + 1000.0));
}
```

Add `#include "month_close.hpp"` to that file.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `settle_trade_credit`, no `Firm::payable_to`.

- [ ] **Step 3: Write the implementation**

In `src/agents.hpp`, add to `Firm`, parallel to the supplier slots:

```cpp
    // Who this firm owes, and how much, for inputs bought this month. Slots
    // are parallel to `suppliers`. One month of credit -- everything here
    // settles at month close, so no per-invoice due dates are needed.
    std::array<uint32_t, params::kMaxInputs * params::kSuppliersPerInput> payable_to{};
    std::array<double, params::kMaxInputs * params::kSuppliersPerInput> payable_amount{};
```

Fill `payable_to` with `NO_FIRM` at construction in `src/world.cpp`.

In `src/daily.cpp`, in the firm branch of `buy`: instead of calling `ledger::transfer`, record the amount into the buyer's `payable_to`/`payable_amount` slot and add to both `payables` and the seller's `receivables`. Update the buyer's `input_cost_basis[slot]` as a moving average over quantity. Households continue to pay cash immediately — consumers do not get trade credit.

In `src/ledger.hpp`, extend `Reason`:

```cpp
    TradeCreditSettlement,  // firm -> firm, paying last month's input bills
```

In `src/month_close.hpp` / `.cpp`, add:

```cpp
// Pay every outstanding supplier bill. Firms are settled in a randomised
// order: with a fixed order, low-indexed firms would be paid first every
// month for the life of the run, which is a systematic advantage nobody
// chose. This is why month_close::run now takes an Rng& -- CLAUDE.md listed
// that as a deferred item and this is where it comes due.
void settle_trade_credit(World& w, Rng& rng, ledger::Journal* journal = nullptr);
```

A firm short of cash pays what it can, pro-rata across its creditors, and the remainder stays outstanding — that shortfall is what Task 11 turns into insolvency.

Change `month_close::run` to `void run(World& w, Rng& rng, ledger::Journal* journal = nullptr);` and call `settle_trade_credit` first in the sequence, before `pay_wages` — suppliers are paid before workers, which is the ordering that makes a payroll squeeze a consequence of a supply bill rather than the other way round.

Update `src/scheduler.cpp` to pass `rng_`, and update the call sites in `tests/test_month_close.cpp` and `tests/test_scheduler.cpp`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged. `settle_trade_credit` must return before touching `rng` when no firm has a payable, or the single-good world's draw sequence shifts.

- [ ] **Step 5: Commit**

```bash
git add src/agents.hpp src/daily.cpp src/month_close.hpp src/month_close.cpp src/scheduler.cpp src/ledger.hpp src/world.cpp tests/test_supply_chain.cpp tests/test_month_close.cpp tests/test_scheduler.cpp
git commit -m "feat(finance): trade credit, settled monthly in randomised order

month_close::run now takes an Rng&; CLAUDE.md listed that as deferred and
this is where it comes due.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 11: Insolvency, death, and the cascade

**Files:**
- Create: `src/firm_lifecycle.hpp`, `src/firm_lifecycle.cpp`
- Modify: `src/month_close.cpp`, `src/ledger.hpp`, `CMakeLists.txt`
- Test: `tests/test_firm_lifecycle.cpp`

**Interfaces:**
- Consumes: `balance_sheet::of`, `Firm::payable_to`.
- Produces: `firm_lifecycle::liquidate(World&, uint32_t firm, ledger::Journal*)`, `firm_lifecycle::run_deaths(World&, Rng&, ledger::Journal*)`, `Firm::alive`, `ledger::Reason::FirmLiquidation`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_firm_lifecycle.cpp`:

```cpp
#include "doctest.h"
#include "balance_sheet.hpp"
#include "firm_lifecycle.hpp"
#include "network.hpp"
#include "world.hpp"

TEST_CASE("an insolvent firm dies and releases its workers") {
    Rng rng(2);
    World w(30, 4, rng);
    network::hire(w, 0, 1);
    network::hire(w, 1, 1);
    w.firms[1].payables = 500.0;   // hopelessly underwater

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[1].employees.empty());
    CHECK(w.households[0].employed() == false);
    CHECK(w.households[1].employed() == false);
}

TEST_CASE("a dead firm's unpaid bill is written off by its supplier") {
    Rng rng(2);
    World w(30, 4, rng);

    w.firms[1].payables = 500.0;
    w.firms[1].payable_to[0] = 2;
    w.firms[1].payable_amount[0] = 500.0;
    w.firms[2].receivables = 500.0;

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[2].receivables == doctest::Approx(0.0));
}

TEST_CASE("a write-off can push an exposed supplier under: the cascade") {
    Rng rng(2);
    World w(30, 4, rng);

    // Firm 1 is doomed and owes firm 2 everything firm 2 is worth.
    w.firms[1].payables = 500.0;
    w.firms[1].payable_to[0] = 2;
    w.firms[1].payable_amount[0] = 500.0;

    // Firm 2 is solvent ONLY because of that receivable.
    w.firms[2].liquidity = 10.0;
    w.firms[2].inventory = 0.0;
    w.firms[2].receivables = 500.0;
    w.firms[2].payables = 400.0;
    REQUIRE(balance_sheet::of(w, 2).insolvent() == false);

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[2].alive == false);  // killed by the write-off, not by its own trading
}

TEST_CASE("liquidation conserves money") {
    Rng rng(2);
    World w(30, 4, rng);
    double before = w.books_balance();
    w.firms[1].liquidity = 100.0;
    w.firms[1].payables = 500.0;

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.books_balance() == doctest::Approx(before + 100.0));
}

TEST_CASE("in a world with no debt nothing dies") {
    Rng rng(2);
    World w(30, 4, rng);
    firm_lifecycle::run_deaths(w, rng);
    for (const Firm& f : w.firms) CHECK(f.alive == true);
}
```

- [ ] **Step 2: Run it to verify it fails**

Add `tests/test_firm_lifecycle.cpp` to `CMakeLists.txt`.

Run: `cmake --build build -j`
Expected: FAIL to compile — no `firm_lifecycle.hpp`.

- [ ] **Step 3: Write the implementation**

In `src/agents.hpp`, add to `Firm`:

```cpp
    // A dead firm is left in place rather than erased: every cross-agent
    // reference is an index (invariant 3), and erasing would invalidate every
    // index above it. Dead firms are skipped, not removed.
    bool alive = true;
```

In `src/ledger.hpp`, extend `Reason`:

```cpp
    FirmLiquidation,  // dying firm -> creditors, then the issuer
```

Create `src/firm_lifecycle.hpp` / `.cpp`:

- `liquidate`: fire every employee via `network::fire`; pay out residual cash to creditors pro-rata by `payable_amount`; zero each creditor's matching `receivables` so the unpaid part becomes their write-off; send any cash still left to issuer 0 with `Reason::FirmLiquidation`; clear inventory, input stock, payables, receivables; set `alive = false`.
- `run_deaths`: **iterate to a fixed point.** One pass is not enough — a write-off in pass one can make a supplier insolvent, and that is the cascade the whole piece exists for. Loop until a pass kills nobody, with a bound of `w.firms.size()` passes to guarantee termination.

Write a comment on the residual-cash line marking it explicitly:

```cpp
    // PLACEHOLDER until piece 2. A dying firm's residual cash should go to
    // its shareholders, and shareholders do not exist yet. Routing it to the
    // issuer keeps invariant 1 exact -- the money leaves circulation rather
    // than vanishing -- and Task 12's founding draws from the same account,
    // so entry and exit are symmetric. Piece 2 replaces both ends with real
    // owners.
```

Call `firm_lifecycle::run_deaths(w, rng, journal)` from `month_close::run`, after `settle_trade_credit` and after `apply_pending_fires`.

Skip dead firms in `daily::produce`, in the buying paths, and in the shop/supplier draws in `network.cpp`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged. In the single-good world no firm ever has a payable, so equity is a sum of non-negative terms and `insolvent()` is never true — `run_deaths` returns after one pass having touched nothing.

- [ ] **Step 5: Commit**

```bash
git add src/firm_lifecycle.hpp src/firm_lifecycle.cpp src/agents.hpp src/ledger.hpp src/month_close.cpp src/network.cpp src/daily.cpp tests/test_firm_lifecycle.cpp CMakeLists.txt
git commit -m "feat(lifecycle): insolvency, liquidation, and the write-off cascade

run_deaths iterates to a fixed point: a write-off in one pass can make a
supplier insolvent in the next. That cascade is the point of the piece.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 12: Firm entry

**Files:**
- Modify: `src/firm_lifecycle.hpp`, `src/firm_lifecycle.cpp`, `src/params.hpp`, `src/month_close.cpp`, `src/ledger.hpp`
- Test: `tests/test_firm_lifecycle.cpp`

**Interfaces:**
- Consumes: `firm_lifecycle` from Task 11, `World::markets`.
- Produces: `firm_lifecycle::run_entry(World&, Rng&, ledger::Journal*)`, `Params::firm_entry_prob`, `Params::firm_entry_margin`, `Params::firm_entry_capital`, `ledger::Reason::FirmFounding`.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_firm_lifecycle.cpp`:

```cpp
TEST_CASE("entry is off by default, so existing runs are untouched") {
    Rng rng(4);
    World w(30, 4, rng);
    CHECK(w.params.firm_entry_prob == doctest::Approx(0.0));

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.firms.size() == before);
}

TEST_CASE("a profitable good with a dead producer attracts entry") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.params.firm_entry_margin = 1.2;

    w.markets[0].reference_price = 100.0;   // far above any plausible cost
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;               // a vacancy in the market

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);

    CHECK(w.firms.size() > before);
    const Firm& founded = w.firms.back();
    CHECK(founded.alive == true);
    CHECK(founded.good == 0u);
    CHECK(founded.liquidity == doctest::Approx(w.params.firm_entry_capital));
}

TEST_CASE("founding conserves money") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.markets[0].reference_price = 100.0;
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;

    double before = w.books_balance();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("no entry when the price does not cover cost") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.markets[0].reference_price = 0.001;
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.firms.size() == before);
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `run_entry`.

- [ ] **Step 3: Write the implementation**

In `src/params.hpp`, add to `Params`:

```cpp
    // Firm entry. Default 0 so the single-good legacy world sees no entry at
    // all and its draw sequence is untouched; the multi-good configurations
    // raise it. Behavioural -- a slider could move it, and moving it changes
    // how fast a shortage is competed away.
    double firm_entry_prob = 0.0;
    double firm_entry_margin = 1.2;     // reference price must exceed cost by this
    double firm_entry_capital = 100.0;  // cash a founded firm starts with
```

In `src/ledger.hpp`:

```cpp
    FirmFounding,  // issuer -> a newly founded firm's opening capital
```

`run_entry`: return immediately if `firm_entry_prob <= 0.0` (before any RNG call). Otherwise, for each good, compute a unit cost from the recipe — labour at the prevailing wage plus each input at its reference price — and count living producers. If the reference price exceeds cost times `firm_entry_margin` and producers are scarce, draw against `firm_entry_prob` and, on success, append a `Firm` with that good, supplier slots filled with `NO_FIRM`, price seeded from the reference, and `firm_entry_capital` transferred from issuer 0 with `Reason::FirmFounding`.

Mark the funding line with the same placeholder comment as Task 11 — piece 2 replaces the issuer with households buying equity.

**Index-stability note for the implementer:** appending to `w.firms` may reallocate the vector. Any reference or pointer into `w.firms` held across a call to `run_entry` is invalidated. Take indices, not references, in `month_close::run`.

Call `firm_lifecycle::run_entry(w, rng, journal)` from `month_close::run`, after `run_deaths`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/firm_lifecycle.hpp src/firm_lifecycle.cpp src/params.hpp src/month_close.cpp src/ledger.hpp tests/test_firm_lifecycle.cpp
git commit -m "feat(lifecycle): firm entry where a good pays above cost

Off by default so the legacy single-good world is untouched.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 13: Input demand and supplier rewiring at month open

Wire the chain into the monthly cycle: a firm plans how much input it needs, and abandons suppliers that ration it.

**Files:**
- Modify: `src/firm_decisions.cpp`, `src/firm_decisions.hpp`, `src/scheduler.cpp`
- Test: `tests/test_supply_chain.cpp`

**Interfaces:**
- Consumes: `Firm::input_demand`, `Firm::input_unsatisfied`, `network::random_unconnected_producer`.
- Produces: `firm_decisions::plan_inputs(World&)`, `firm_decisions::rewire_suppliers(World&, Rng&)`.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_supply_chain.cpp`:

```cpp
TEST_CASE("planned input demand follows planned output") {
    Rng rng(6);
    World w = two_stage_world(rng);
    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};   // 3 * productivity units of output capacity

    firm_decisions::plan_inputs(w);

    // 9 units of bread at 0.5 ore each = 4.5 ore for the month.
    CHECK(bread.input_demand[0] ==
          doctest::Approx(3.0 * w.params.productivity * 0.5 * params::kDaysPerMonth));
}

TEST_CASE("a firm rationed by a supplier rewires away from it") {
    Rng rng(6);
    World w = two_stage_world(rng);
    Firm& bread = w.firms[2];
    bread.suppliers[0] = 0;
    bread.input_unsatisfied[0] = 500.0;   // firm 0 kept turning it away

    firm_decisions::rewire_suppliers(w, rng);

    CHECK(bread.suppliers[0] != 0u);
    CHECK(bread.input_unsatisfied[0] == doctest::Approx(0.0));
}

TEST_CASE("a firm with no material inputs draws nothing when rewiring") {
    Rng rng(6);
    World w(20, 4, rng);   // single-good world

    Rng a(77);
    firm_decisions::rewire_suppliers(w, a);
    Rng b(77);
    CHECK(a.uniform() == doctest::Approx(b.uniform()));
}
```

Add `#include "firm_decisions.hpp"` to that file.

Task 15 additionally needs `#include "scheduler.hpp"` and `#include "golden_hash.hpp"`.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `plan_inputs`.

- [ ] **Step 3: Write the implementation**

`plan_inputs`: for each living firm, `input_demand[s] = labour_capacity * kDaysPerMonth * input_qty[s]`, skipping empty recipe slots.

`rewire_suppliers`: mirrors the rationed-household path — return before touching `rng` if the firm's good has no inputs; for each supplier slot with `input_unsatisfied` above zero, draw `network::random_unconnected_producer` and replace the slot; clear the counter afterwards. An empty slot (`NO_FIRM`) is also filled by a draw, which is how a newly founded firm acquires suppliers.

Call both from `firm_decisions::run_monthly`, after `adjust_price`. Clear `input_unsatisfied` after rewiring, matching how `unsatisfied` is consumed by shop rewiring and then cleared (CLAUDE.md's ordering note).

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/firm_decisions.cpp src/firm_decisions.hpp src/scheduler.cpp tests/test_supply_chain.cpp
git commit -m "feat(chain): firms plan input demand and rewire away from rationing

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 14: Commoditization drives spread and stickiness

Task 5 wired the first of commoditization's three channels. This wires the other two, so the number means the same thing everywhere.

**Files:**
- Modify: `src/daily.cpp`, `src/firm_decisions.cpp`, `src/household_decisions.cpp`
- Test: `tests/test_supply_chain.cpp`

**Interfaces:**
- Consumes: `Good::commoditization`, `daily::Buyer`, `firm_decisions::rewire_suppliers`.
- Produces: no new names; two existing behaviours become commoditization-weighted.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_supply_chain.cpp`:

```cpp
TEST_CASE("a commodity buyer compares more sellers than a bespoke one") {
    Rng rng(8);
    World w = two_stage_world(rng);

    // Both goods, same buyer state, different commoditization.
    CHECK(daily::sellers_compared(w, 0) > daily::sellers_compared(w, 1));

    // The consumer good sits at commoditization 0 and must compare exactly
    // what the existing rule compares -- no more, no less, or the single-good
    // world changes.
    CHECK(daily::sellers_compared(w, 1) == 1u);
}

TEST_CASE("a commodity link is abandoned more readily than a bespoke one") {
    Rng rng(8);
    World w = two_stage_world(rng);

    // Stickiness is the inverse of commoditization: ore (upstream, fungible)
    // is switched away from freely; the consumer good is not.
    CHECK(daily::switch_probability(w, 0) > daily::switch_probability(w, 1));
    CHECK(daily::switch_probability(w, 1) == doctest::Approx(0.0));
}

TEST_CASE("at commoditization 0 the single-good world is untouched") {
    Rng rng(8);
    World w(20, 4, rng);
    CHECK(daily::sellers_compared(w, 0) == 1u);
    CHECK(daily::switch_probability(w, 0) == doctest::Approx(0.0));
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — no `daily::sellers_compared`.

- [ ] **Step 3: Write the implementation**

In `src/daily.hpp`:

```cpp
// How many sellers a buyer of this good compares before committing, and how
// readily it abandons a link it already has. Both are the SAME
// commoditization number that drives the price pull -- one dial, three
// channels, so "how commodity-like is this good" has a single answer rather
// than three that can drift apart.
//
// Both are the identity at commoditization 0: one seller compared, zero extra
// switching. That is what keeps the consumer good behaving exactly as it did
// before goods existed.
uint32_t sellers_compared(const World& w, uint32_t good);
double switch_probability(const World& w, uint32_t good);
```

Implement `sellers_compared` as `1 + floor(commoditization * (kSuppliersPerInput - 1))` and `switch_probability` as the commoditization value itself, both reading `w.goods[good]`.

In the firm input-buying branch of `daily::buy`, visit `sellers_compared` supplier slots and take the cheapest with stock, rather than the first with stock. At commoditization 0 that is one slot, which is the existing behaviour.

In `firm_decisions::rewire_suppliers`, draw against `switch_probability` for a slot that was *not* rationed — a commodity buyer shops around even when it was served, a bespoke buyer does not. The rationed path is unchanged and still always rewires.

**Determinism note:** the extra draw must be taken only when `switch_probability > 0.0`. A good at commoditization 0 must consume no additional randomness.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j && ./build/atc_tests`
Expected: PASS, golden hash unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/daily.hpp src/daily.cpp src/firm_decisions.cpp tests/test_supply_chain.cpp
git commit -m "feat(chain): commoditization drives spread and stickiness too

One dial, three channels: price, how many sellers are compared, and how
readily a link is dropped. All three are the identity at 0.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

### Task 15: A running multi-good economy, and the ensemble check

The first task allowed to produce behaviour nobody has seen. It does **not** change the golden hash — that test keeps guarding the single-good world forever.

**Files:**
- Create: `scenarios/two_stage.scenario`
- Modify: `src/main.cpp`, `tests/test_supply_chain.cpp`, `CLAUDE.md`
- Test: `tests/test_supply_chain.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces: `--chain <depth>` CLI flag that builds a linear chain of that depth before running.

- [ ] **Step 1: Write the failing test**

Append to `tests/test_supply_chain.cpp`:

```cpp
TEST_CASE("a two-stage economy runs 120 months with the books balanced") {
    Rng rng(1);
    World w = two_stage_world(rng);
    w.params.firm_entry_prob = 0.05;
    for (Firm& f : w.firms) f.liquidity = 50.0;

    double before = w.books_balance();
    Scheduler sched(w, rng);
    for (int m = 0; m < 120; ++m) sched.run_month();

    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("upstream commoditization exceeds downstream in a running economy") {
    Rng rng(1);
    World w = two_stage_world(rng);
    CHECK(w.goods[0].commoditization > w.goods[1].commoditization);
    CHECK(w.goods[1].commoditization == doctest::Approx(0.0));
}

TEST_CASE("two-stage economy: same seed, byte-identical") {
    auto run = []() {
        Rng rng(3);
        World w = two_stage_world(rng);
        w.params.firm_entry_prob = 0.05;
        Scheduler sched(w, rng);
        for (int m = 0; m < 60; ++m) sched.run_month();
        return golden::run_hash(w);
    };
    CHECK(run() == run());
}
```

Move `golden::run_hash` into a shared header `tests/golden_hash.hpp` and include it from both test files, so the two-stage determinism test reuses it rather than redefining it.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j`
Expected: FAIL to compile — `golden_hash.hpp` does not exist yet.

- [ ] **Step 3: Implement**

Extract `tests/golden_hash.hpp`. Add the `--chain <depth>` flag to `src/main.cpp`, building a linear chain of that depth and distributing firms evenly across the goods, then `goods::recompute` and `markets.resize`. Write `scenarios/two_stage.scenario` as a two-stage run.

- [ ] **Step 4: Run the tests and look at the economy**

```bash
cmake --build build -j && ./build/atc_tests
./build/atc --seed 1 --chain 3 --burn-in 200 --months 600 --out output/chain.csv
python3 scripts/ensemble.py   # follow its usage for a multi-seed run
```

Expected: tests PASS including the unchanged golden hash. Then **check per-firm distributions, not means** — CLAUDE.md: a "mean wage of 16" was once 67 dead firms at zero mixed with 33 healthy ones at 53. Confirm across seeds that upstream firms are not all dead and the chain has not collapsed into one survivor per good. **One seed is not a sample.**

- [ ] **Step 5: Update CLAUDE.md and commit**

Add to the architecture table: `goods`, `market`, `balance_sheet`, `firm_lifecycle`. Add to "Model rules that look like bugs but are not": Leontief production with no substitution; `remove` as longest path not shortest; liquidation routing residual cash to the issuer as a named placeholder. Remove `month_close::run takes no Rng&` from the deferred list — it now does.

```bash
git add -A
git commit -m "feat(chain): a running multi-good economy, and the CLI to drive it

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```
