# Lengnick Baseline v1 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a headless C++23 agent-based simulation of Lengnick's baseline macroeconomic model (1000 households, 100 firms) that produces endogenous business cycles, a Phillips curve, and a Beveridge curve with no exogenous shocks.

**Architecture:** Agents are plain structs in flat contiguous arrays, referenced by `uint32_t` index. A single explicitly-threaded seeded RNG makes runs byte-reproducible. The simulation core is headless and emits aggregate records through one `Recorder` interface (CSV in v1). Time advances in days, 21 days to a month; goods trade daily, labour monthly.

**Tech Stack:** C++23, GCC 16.2.1, CMake 4.4.2, doctest (vendored single header).

**Spec:** `docs/superpowers/specs/2026-08-27-agent-based-economy-design.md`

## Global Constraints

- **C++23.** `set(CMAKE_CXX_STANDARD 23)`, `CMAKE_CXX_STANDARD_REQUIRED ON`, no compiler extensions.
- **No pointers or references between agents.** Cross-agent references are `uint32_t` indices only. `NO_FIRM` / `NO_HOUSEHOLD` sentinel is `0xFFFFFFFFu`.
- **No virtual functions on agent types.** `Firm` and `Household` are plain aggregates. The only permitted virtual interface in the project is `Recorder`.
- **All randomness flows through an explicitly passed `Rng&`.** No `rand()`, no `std::random_device` outside seeding, no global or thread-local RNG state. Agent iteration order, where randomized, draws from that same `Rng`.
- **The core never does I/O except through `Recorder`.** No `std::cout` in `src/` outside `main.cpp`.
- **Money is conserved.** `sum(household.liquidity) + sum(firm.liquidity)` must remain constant across every tick. This is the project's master invariant.
- **Warnings:** build with `-Wall -Wextra -Wpedantic`.
- Populations are fixed: 1000 households, 100 firms. No firm entry or exit in v1.

---

## Calibration (Lengnick 2013, Table 1)

These are the exact published values. Use them verbatim; do not tune until the success criteria in Task 10 are met.

**Households**

| Symbol | Name in code | Value | Meaning |
|---|---|---|---|
| ξ | `kPriceSwitchThreshold` | 0.01 | New shop must be ≥1% cheaper to switch |
| β | `kUnemployedFirmVisits` | 5 | Firms visited per month when unemployed |
| π | `kEmployedSearchProb` | 0.1 | Job-search probability when satisfied and employed |
| α | `kConsumptionExponent` | 0.9 | Exponent in the consumption function |
| n | `kMaxShopVisitsPerDay` | 7 | Max firms tried per day to satisfy demand |
| ψ_Price | `kProbPriceSearch` | 0.25 | Probability of running the price-based shop search |
| ψ_Quant | `kProbQuantSearch` | 0.25 | Probability of running the rationing-based shop search |

**Firms**

| Symbol | Name in code | Value | Meaning |
|---|---|---|---|
| γ | `kMonthsBeforeWageCut` | 24 | Consecutive fully-staffed months before cutting wage |
| δ | `kWageAdjustMax` | 0.019 | Upper bound of the uniform wage adjustment draw |
| φ̄ | `kInventoryUpperFactor` | 1.0 | Upper inventory band, × last month's demand |
| φ | `kInventoryLowerFactor` | 0.25 | Lower inventory band, × last month's demand |
| ϕ̄ | `kPriceUpperFactor` | 1.15 | Upper price bound, × marginal cost |
| ϕ | `kPriceLowerFactor` | 1.025 | Lower price bound, × marginal cost |
| ϑ | `kPriceAdjustMax` | 0.02 | Upper bound of the uniform price adjustment draw |
| θ | `kPriceChangeProb` | 0.75 | Calvo probability that a computed price change is applied |
| λ | `kProductivity` | 3.0 | Goods produced per worker per day |
| χ | `kBufferFactor` | 0.1 | Liquidity buffer retained, × monthly wage bill |

**Run configuration:** 1000 households, 100 firms, 21 days/month, burn-in 1000 months, measurement 6000 months.

**Known ambiguity:** Lengnick's Table 1 also lists `φ = 0.25` under the *Households* column. No household rule in the paper's text uses a parameter named φ; the household rules use ξ, β, π, α, n, ψ_Price, ψ_Quant only. Treat it as a typesetting duplicate of the firm inventory band and **do not introduce a household φ**. If a success criterion in Task 10 fails, revisit this note before tuning anything else.

---

## File Structure

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | Build configuration; `atc_core` library, `economy` binary, `atc_tests` binary |
| `third_party/doctest.h` | Vendored single-header test framework |
| `src/params.hpp` | All calibration constants and run configuration. No logic. |
| `src/rng.hpp` / `src/rng.cpp` | Deterministic seeded RNG wrapper |
| `src/agents.hpp` | `Firm` and `Household` structs, sentinels |
| `src/world.hpp` / `src/world.cpp` | Owns agent arrays; construction and initialization; money-total helper |
| `src/network.hpp` / `src/network.cpp` | Shop and employment link management |
| `src/firm_decisions.hpp` / `.cpp` | Firm monthly rules: wage, hire/fire, price |
| `src/household_decisions.hpp` / `.cpp` | Household monthly rules: rewiring, job search, consumption budget |
| `src/daily.hpp` / `src/daily.cpp` | Daily shopping and production |
| `src/month_close.hpp` / `.cpp` | Wages, buffer, profit distribution, reservation-wage update |
| `src/scheduler.hpp` / `.cpp` | Assembles the full day/month tick |
| `src/recorder.hpp` / `src/recorder.cpp` | `Recorder` interface, `CsvRecorder`, aggregate metric computation |
| `src/main.cpp` | CLI entry point |
| `tests/*.cpp` | One test file per source module |

---

### Task 1: Project skeleton and deterministic RNG

**Files:**
- Create: `CMakeLists.txt`
- Create: `third_party/doctest.h` (downloaded)
- Create: `src/rng.hpp`, `src/rng.cpp`
- Test: `tests/test_rng.cpp`, `tests/test_main.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `class Rng` with `explicit Rng(uint64_t seed)`, `double uniform(double lo, double hi)`, `uint32_t index(uint32_t n)`, `bool chance(double p)`, `void shuffle(std::vector<uint32_t>& v)`

- [ ] **Step 1: Vendor doctest**

```bash
mkdir -p third_party
curl -fsSL -o third_party/doctest.h \
  https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
```

Verify it downloaded: `head -5 third_party/doctest.h` should show the doctest banner, and the file should be well over 100KB.

- [ ] **Step 2: Write CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.28)
project(atc CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(atc_core
    src/rng.cpp
)
target_include_directories(atc_core PUBLIC src)

enable_testing()
add_executable(atc_tests
    tests/test_main.cpp
    tests/test_rng.cpp
)
target_link_libraries(atc_tests PRIVATE atc_core)
target_include_directories(atc_tests PRIVATE third_party)
add_test(NAME atc_tests COMMAND atc_tests)
```

- [ ] **Step 3: Write the test main**

`tests/test_main.cpp`:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 4: Write the failing tests**

`tests/test_rng.cpp`:

```cpp
#include "doctest.h"
#include "rng.hpp"

#include <vector>

TEST_CASE("same seed produces identical sequences") {
    Rng a(12345);
    Rng b(12345);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.uniform(0.0, 1.0) == b.uniform(0.0, 1.0));
    }
}

TEST_CASE("different seeds produce different sequences") {
    Rng a(1);
    Rng b(2);
    bool any_different = false;
    for (int i = 0; i < 100; ++i) {
        if (a.uniform(0.0, 1.0) != b.uniform(0.0, 1.0)) any_different = true;
    }
    CHECK(any_different);
}

TEST_CASE("uniform respects bounds") {
    Rng r(7);
    for (int i = 0; i < 10000; ++i) {
        double v = r.uniform(2.0, 5.0);
        CHECK(v >= 2.0);
        CHECK(v < 5.0);
    }
}

TEST_CASE("index returns values below n") {
    Rng r(7);
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.index(10) < 10u);
    }
}

TEST_CASE("chance(0) never fires and chance(1) always fires") {
    Rng r(7);
    for (int i = 0; i < 1000; ++i) {
        CHECK_FALSE(r.chance(0.0));
        CHECK(r.chance(1.0));
    }
}

TEST_CASE("shuffle is deterministic for a given seed and permutes") {
    std::vector<uint32_t> a{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint32_t> b = a;
    Rng ra(99);
    Rng rb(99);
    ra.shuffle(a);
    rb.shuffle(b);
    CHECK(a == b);

    std::vector<uint32_t> sorted = a;
    std::sort(sorted.begin(), sorted.end());
    for (uint32_t i = 0; i < 10; ++i) CHECK(sorted[i] == i);
}
```

- [ ] **Step 5: Run tests to verify they fail**

```bash
cmake -S . -B build && cmake --build build -j
```

Expected: compile FAILS with `rng.hpp: No such file or directory`.

- [ ] **Step 6: Implement Rng**

`src/rng.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <random>
#include <vector>

// Deterministic RNG. Every source of randomness in the simulation must
// draw from an instance of this, passed explicitly. Same seed => same run.
class Rng {
public:
    explicit Rng(uint64_t seed) : engine_(seed) {}

    // Uniform real in [lo, hi).
    double uniform(double lo, double hi);

    // Uniform integer in [0, n). Precondition: n > 0.
    uint32_t index(uint32_t n);

    // True with probability p.
    bool chance(double p);

    // In-place Fisher-Yates using this engine.
    void shuffle(std::vector<uint32_t>& v);

private:
    std::mt19937_64 engine_;
};
```

`src/rng.cpp`:

```cpp
#include "rng.hpp"

#include <algorithm>

double Rng::uniform(double lo, double hi) {
    std::uniform_real_distribution<double> d(lo, hi);
    return d(engine_);
}

uint32_t Rng::index(uint32_t n) {
    std::uniform_int_distribution<uint32_t> d(0, n - 1);
    return d(engine_);
}

bool Rng::chance(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return uniform(0.0, 1.0) < p;
}

void Rng::shuffle(std::vector<uint32_t>& v) {
    // Explicit Fisher-Yates rather than std::shuffle: guarantees the same
    // sequence across standard library versions, which std::shuffle does not.
    if (v.size() < 2) return;
    for (size_t i = v.size() - 1; i > 0; --i) {
        uint32_t j = index(static_cast<uint32_t>(i + 1));
        std::swap(v[i], v[j]);
    }
}
```

Add `#include <algorithm>` to `tests/test_rng.cpp` for `std::sort`.

- [ ] **Step 7: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all test cases pass.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt third_party/doctest.h src/rng.hpp src/rng.cpp tests/
git commit -m "feat: project skeleton with deterministic seeded RNG"
```

---

### Task 2: Parameters, agent structs, and world initialization

**Files:**
- Create: `src/params.hpp`, `src/agents.hpp`, `src/world.hpp`, `src/world.cpp`
- Modify: `CMakeLists.txt` (add `src/world.cpp`, `tests/test_world.cpp`)
- Test: `tests/test_world.cpp`

**Interfaces:**
- Consumes: `Rng` from Task 1
- Produces: `struct Firm`, `struct Household`, `constexpr uint32_t NO_FIRM`, `NO_HOUSEHOLD`, `class World` with `World(uint32_t n_households, uint32_t n_firms, Rng& rng)`, `std::vector<Firm> firms`, `std::vector<Household> households`, `double total_money() const`

- [ ] **Step 1: Write params.hpp**

`src/params.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace params {

// --- Time ---
inline constexpr int kDaysPerMonth = 21;

// --- Population ---
inline constexpr uint32_t kNumHouseholds = 1000;
inline constexpr uint32_t kNumFirms = 100;
inline constexpr uint32_t kShopsPerHousehold = 7;

// --- Households (Lengnick 2013 Table 1) ---
inline constexpr double kPriceSwitchThreshold = 0.01;   // xi
inline constexpr uint32_t kUnemployedFirmVisits = 5;    // beta
inline constexpr double kEmployedSearchProb = 0.1;      // pi
inline constexpr double kConsumptionExponent = 0.9;     // alpha
inline constexpr uint32_t kMaxShopVisitsPerDay = 7;     // n
inline constexpr double kProbPriceSearch = 0.25;        // psi_Price
inline constexpr double kProbQuantSearch = 0.25;        // psi_Quant
inline constexpr double kUnemployedReservationCut = 0.10;

// --- Firms (Lengnick 2013 Table 1) ---
inline constexpr uint32_t kMonthsBeforeWageCut = 24;    // gamma
inline constexpr double kWageAdjustMax = 0.019;         // delta
inline constexpr double kInventoryUpperFactor = 1.0;    // phi upper
inline constexpr double kInventoryLowerFactor = 0.25;   // phi lower
inline constexpr double kPriceUpperFactor = 1.15;       // varphi upper
inline constexpr double kPriceLowerFactor = 1.025;      // varphi lower
inline constexpr double kPriceAdjustMax = 0.02;         // vartheta
inline constexpr double kPriceChangeProb = 0.75;        // theta
inline constexpr double kProductivity = 3.0;            // lambda
inline constexpr double kBufferFactor = 0.1;            // chi

// --- Shopping ---
inline constexpr double kDemandSatisfiedFraction = 0.95;

// --- Initial conditions ---
inline constexpr double kInitialFirmLiquidity = 0.0;
inline constexpr double kInitialHouseholdLiquidity = 100.0;
inline constexpr double kInitialPrice = 1.0;
inline constexpr double kInitialWage = 30.0;
inline constexpr double kInitialInventory = 0.0;

// --- Run configuration ---
inline constexpr int kBurnInMonths = 1000;
inline constexpr int kMeasurementMonths = 6000;

}  // namespace params
```

- [ ] **Step 2: Write agents.hpp**

`src/agents.hpp`:

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "params.hpp"

inline constexpr uint32_t NO_FIRM = 0xFFFFFFFFu;
inline constexpr uint32_t NO_HOUSEHOLD = 0xFFFFFFFFu;

// Plain aggregate. No virtuals, no inheritance. Cross-agent references are
// indices, never pointers.
struct Firm {
    double liquidity = params::kInitialFirmLiquidity;
    double inventory = params::kInitialInventory;
    double price = params::kInitialPrice;
    double wage = params::kInitialWage;

    // Demand encountered this month, and last month's total (drives all rules).
    double demand_this_month = 0.0;
    double demand_last_month = 0.0;

    std::vector<uint32_t> employees;

    bool has_open_vacancy = false;
    bool vacancy_went_unfilled = false;
    uint32_t months_fully_staffed = 0;

    // Firing is decided this month, applied next month (job-protection lag).
    uint32_t pending_fire = NO_HOUSEHOLD;
};

struct Household {
    double liquidity = params::kInitialHouseholdLiquidity;
    double reservation_wage = params::kInitialWage;
    uint32_t employer = NO_FIRM;

    std::array<uint32_t, params::kShopsPerHousehold> shops{};

    // Planned real consumption for the month, and the daily slice of it.
    double planned_consumption = 0.0;

    // Per-shop unmet demand accumulated over the month, used for rewiring.
    std::array<double, params::kShopsPerHousehold> unsatisfied{};

    bool employed() const { return employer != NO_FIRM; }
};
```

- [ ] **Step 3: Write the failing tests**

`tests/test_world.cpp`:

```cpp
#include "doctest.h"
#include "world.hpp"
#include "params.hpp"

#include <algorithm>
#include <set>

TEST_CASE("world is created with the requested populations") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    CHECK(w.households.size() == params::kNumHouseholds);
    CHECK(w.firms.size() == params::kNumFirms);
}

TEST_CASE("every household has seven distinct valid shops") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (const auto& h : w.households) {
        std::set<uint32_t> distinct(h.shops.begin(), h.shops.end());
        CHECK(distinct.size() == params::kShopsPerHousehold);
        for (uint32_t s : h.shops) CHECK(s < params::kNumFirms);
    }
}

TEST_CASE("employment links are mutually consistent") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (uint32_t h = 0; h < w.households.size(); ++h) {
        uint32_t f = w.households[h].employer;
        if (f == NO_FIRM) continue;
        const auto& emps = w.firms[f].employees;
        CHECK(std::find(emps.begin(), emps.end(), h) != emps.end());
    }
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        for (uint32_t h : w.firms[f].employees) {
            CHECK(w.households[h].employer == f);
        }
    }
}

TEST_CASE("initialization is deterministic for a given seed") {
    Rng r1(42);
    World a(params::kNumHouseholds, params::kNumFirms, r1);
    Rng r2(42);
    World b(params::kNumHouseholds, params::kNumFirms, r2);
    for (size_t i = 0; i < a.households.size(); ++i) {
        CHECK(a.households[i].shops == b.households[i].shops);
        CHECK(a.households[i].employer == b.households[i].employer);
    }
}

TEST_CASE("total money is the sum of all liquidity") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    double expected = params::kNumHouseholds * params::kInitialHouseholdLiquidity
                    + params::kNumFirms * params::kInitialFirmLiquidity;
    CHECK(w.total_money() == doctest::Approx(expected));
}
```

- [ ] **Step 4: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `world.hpp: No such file or directory`.

- [ ] **Step 5: Implement World**

`src/world.hpp`:

```cpp
#pragma once

#include <vector>

#include "agents.hpp"
#include "rng.hpp"

// Owns all agent state. The single source of truth for the simulation.
class World {
public:
    World(uint32_t n_households, uint32_t n_firms, Rng& rng);

    std::vector<Firm> firms;
    std::vector<Household> households;

    // Master invariant: this must not change across a tick.
    double total_money() const;
};
```

`src/world.cpp`:

```cpp
#include "world.hpp"

#include <numeric>

World::World(uint32_t n_households, uint32_t n_firms, Rng& rng) {
    firms.resize(n_firms);
    households.resize(n_households);

    for (uint32_t h = 0; h < n_households; ++h) {
        Household& hh = households[h];

        // Seven distinct shop connections, drawn without replacement.
        // The initial network is scaffolding only; it is expected to wash
        // out during burn-in.
        for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
            uint32_t candidate;
            bool duplicate;
            do {
                candidate = rng.index(n_firms);
                duplicate = false;
                for (uint32_t prior = 0; prior < s; ++prior) {
                    if (hh.shops[prior] == candidate) { duplicate = true; break; }
                }
            } while (duplicate);
            hh.shops[s] = candidate;
        }

        // Start fully employed, spread evenly across firms. Unemployment must
        // emerge from the dynamics, not from the initial conditions.
        uint32_t f = h % n_firms;
        hh.employer = f;
        firms[f].employees.push_back(h);
    }
}

double World::total_money() const {
    double total = 0.0;
    for (const auto& f : firms) total += f.liquidity;
    for (const auto& h : households) total += h.liquidity;
    return total;
}
```

- [ ] **Step 6: Register the new files in CMakeLists.txt**

Add `src/world.cpp` to the `atc_core` source list and `tests/test_world.cpp` to the `atc_tests` source list.

- [ ] **Step 7: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add src/params.hpp src/agents.hpp src/world.hpp src/world.cpp tests/test_world.cpp CMakeLists.txt
git commit -m "feat: calibration parameters, agent structs, and world initialization"
```

---

### Task 3: Network link management

**Files:**
- Create: `src/network.hpp`, `src/network.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_network.cpp`

**Interfaces:**
- Consumes: `World`, `Rng`
- Produces: free functions in `namespace network`: `void hire(World&, uint32_t household, uint32_t firm)`, `void fire(World&, uint32_t household)`, `void replace_shop(World&, uint32_t household, uint32_t slot, uint32_t new_firm)`, `bool has_shop(const Household&, uint32_t firm)`, `uint32_t random_unconnected_firm_by_size(const World&, uint32_t household, Rng&)`

- [ ] **Step 1: Write the failing tests**

`tests/test_network.cpp`:

```cpp
#include "doctest.h"
#include "network.hpp"
#include "world.hpp"

#include <algorithm>

TEST_CASE("hire links household and firm in both directions") {
    Rng rng(1);
    World w(10, 3, rng);
    network::fire(w, 0);
    REQUIRE(w.households[0].employer == NO_FIRM);

    network::hire(w, 0, 2);
    CHECK(w.households[0].employer == 2u);
    const auto& e = w.firms[2].employees;
    CHECK(std::count(e.begin(), e.end(), 0u) == 1);
}

TEST_CASE("hire removes the household from its previous employer") {
    Rng rng(1);
    World w(10, 3, rng);
    uint32_t old_firm = w.households[0].employer;
    REQUIRE(old_firm != NO_FIRM);

    network::hire(w, 0, (old_firm + 1) % 3);

    const auto& old_e = w.firms[old_firm].employees;
    CHECK(std::find(old_e.begin(), old_e.end(), 0u) == old_e.end());
}

TEST_CASE("fire unlinks in both directions and is idempotent") {
    Rng rng(1);
    World w(10, 3, rng);
    uint32_t f = w.households[0].employer;
    REQUIRE(f != NO_FIRM);

    network::fire(w, 0);
    CHECK(w.households[0].employer == NO_FIRM);
    const auto& e = w.firms[f].employees;
    CHECK(std::find(e.begin(), e.end(), 0u) == e.end());

    network::fire(w, 0);  // must not crash or corrupt
    CHECK(w.households[0].employer == NO_FIRM);
}

TEST_CASE("replace_shop swaps the slot and clears its unmet demand") {
    Rng rng(1);
    World w(10, 5, rng);
    w.households[0].unsatisfied[2] = 17.0;

    network::replace_shop(w, 0, 2, 4);

    CHECK(w.households[0].shops[2] == 4u);
    CHECK(w.households[0].unsatisfied[2] == doctest::Approx(0.0));
}

TEST_CASE("has_shop detects membership") {
    Rng rng(1);
    World w(10, 5, rng);
    network::replace_shop(w, 0, 0, 3);
    CHECK(network::has_shop(w.households[0], 3));
}

TEST_CASE("random_unconnected_firm_by_size never returns a connected firm") {
    Rng rng(1);
    World w(50, 20, rng);
    for (int trial = 0; trial < 500; ++trial) {
        uint32_t f = network::random_unconnected_firm_by_size(w, 0, rng);
        if (f == NO_FIRM) continue;
        CHECK_FALSE(network::has_shop(w.households[0], f));
        CHECK(f < 20u);
    }
}

TEST_CASE("random_unconnected_firm_by_size returns NO_FIRM when all are connected") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    CHECK(network::random_unconnected_firm_by_size(w, 0, rng) == NO_FIRM);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `network.hpp: No such file or directory`.

- [ ] **Step 3: Implement network**

`src/network.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace network {

// Employ `household` at `firm`, detaching it from any previous employer.
void hire(World& w, uint32_t household, uint32_t firm);

// Unemploy `household`. Safe to call on an already-unemployed household.
void fire(World& w, uint32_t household);

// Point shop slot `slot` at `new_firm`, clearing that slot's unmet demand.
void replace_shop(World& w, uint32_t household, uint32_t slot, uint32_t new_firm);

bool has_shop(const Household& h, uint32_t firm);

// Draw a firm the household does not already shop at, with probability
// proportional to firm size in employees. Firms with zero employees are
// still reachable (weight 1) so they are not permanently excluded.
// Returns NO_FIRM if every firm is already connected.
uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng);

}  // namespace network
```

`src/network.cpp`:

```cpp
#include "network.hpp"

#include <algorithm>
#include <vector>

#include "world.hpp"

namespace network {

void hire(World& w, uint32_t household, uint32_t firm) {
    fire(w, household);
    w.households[household].employer = firm;
    w.firms[firm].employees.push_back(household);
}

void fire(World& w, uint32_t household) {
    uint32_t f = w.households[household].employer;
    if (f == NO_FIRM) return;
    auto& e = w.firms[f].employees;
    e.erase(std::remove(e.begin(), e.end(), household), e.end());
    w.households[household].employer = NO_FIRM;
}

void replace_shop(World& w, uint32_t household, uint32_t slot, uint32_t new_firm) {
    w.households[household].shops[slot] = new_firm;
    w.households[household].unsatisfied[slot] = 0.0;
}

bool has_shop(const Household& h, uint32_t firm) {
    return std::find(h.shops.begin(), h.shops.end(), firm) != h.shops.end();
}

uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng) {
    const Household& h = w.households[household];

    double total_weight = 0.0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (has_shop(h, f)) continue;
        total_weight += 1.0 + static_cast<double>(w.firms[f].employees.size());
    }
    if (total_weight <= 0.0) return NO_FIRM;

    double draw = rng.uniform(0.0, total_weight);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (has_shop(h, f)) continue;
        draw -= 1.0 + static_cast<double>(w.firms[f].employees.size());
        if (draw <= 0.0) return f;
    }
    return NO_FIRM;  // unreachable except for floating-point edge cases
}

}  // namespace network
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/network.cpp` to `atc_core` and `tests/test_network.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/network.hpp src/network.cpp tests/test_network.cpp CMakeLists.txt
git commit -m "feat: household-firm network link management"
```

---

### Task 4: Firm monthly decisions

**Files:**
- Create: `src/firm_decisions.hpp`, `src/firm_decisions.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_firm_decisions.cpp`

**Interfaces:**
- Consumes: `World`, `Rng`, `network`
- Produces: `namespace firm_decisions`: `void adjust_wage(Firm&, Rng&)`, `void adjust_workforce(World&, uint32_t firm, Rng&)`, `void adjust_price(Firm&, Rng&)`, `double marginal_cost(const Firm&)`, `void run_monthly(World&, Rng&)`

**Rules being implemented** (spec §5.1 steps 1–3):
- Wage rises by `(1 + µ)`, `µ ~ U[0, δ]`, if a vacancy went unfilled last month. Wage falls by `(1 - µ)` if fully staffed for `γ` consecutive months, which then resets the counter.
- Inventory bands are `[φ_lo · demand_last_month, φ_hi · demand_last_month]`. Below → open one vacancy. Above → mark one random employee for firing next month. At most one of each per month.
- Price is reconsidered only when inventory is outside those bands. Marginal cost is `wage · employees / (λ · employees · days)`, which reduces to `wage / (λ · days)`. Low inventory → try to raise, capped at `ϕ_hi · mc`. High inventory → try to lower, floored at `ϕ_lo · mc`. The change applies only with probability `θ`.

- [ ] **Step 1: Write the failing tests**

`tests/test_firm_decisions.cpp`:

```cpp
#include "doctest.h"
#include "firm_decisions.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("wage rises when a vacancy went unfilled") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = true;
    firm_decisions::adjust_wage(f, rng);
    CHECK(f.wage > 100.0);
    CHECK(f.wage <= 100.0 * (1.0 + params::kWageAdjustMax));
}

TEST_CASE("wage falls after gamma consecutive fully-staffed months") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = params::kMonthsBeforeWageCut;
    firm_decisions::adjust_wage(f, rng);
    CHECK(f.wage < 100.0);
    CHECK(f.wage >= 100.0 * (1.0 - params::kWageAdjustMax));
    CHECK(f.months_fully_staffed == 0u);
}

TEST_CASE("wage is unchanged when neither condition holds") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = 3;
    firm_decisions::adjust_wage(f, rng);
    CHECK(f.wage == doctest::Approx(100.0));
}

TEST_CASE("low inventory opens a vacancy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    f.inventory = 10.0;  // below 0.25 * 100
    f.has_open_vacancy = false;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("high inventory marks one employee for firing next month") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(f.employees.size() > 0);
    size_t before = f.employees.size();
    f.demand_last_month = 100.0;
    f.inventory = 200.0;  // above 1.0 * 100
    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK(f.pending_fire != NO_HOUSEHOLD);
    CHECK(f.employees.size() == before);  // firing is lagged, not immediate
    CHECK_FALSE(f.has_open_vacancy);
}

TEST_CASE("inventory inside the bands changes nothing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    f.inventory = 50.0;  // between 25 and 100
    f.has_open_vacancy = false;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK_FALSE(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("a firm with no employees does not mark anyone for firing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    while (!f.employees.empty()) network::fire(w, f.employees.front());
    f.demand_last_month = 100.0;
    f.inventory = 200.0;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("marginal cost is wage per unit of monthly output per worker") {
    Firm f;
    f.wage = 63.0;
    double expected = 63.0 / (params::kProductivity * params::kDaysPerMonth);
    CHECK(firm_decisions::marginal_cost(f) == doctest::Approx(expected));
}

TEST_CASE("price rises toward the cap when inventory is low") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1.0;
    double mc = firm_decisions::marginal_cost(f);
    f.price = mc;  // well below the upper bound

    bool ever_rose = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
        if (f.price > before) ever_rose = true;
        CHECK(f.price <= mc * params::kPriceUpperFactor + 1e-9);
    }
    CHECK(ever_rose);
}

TEST_CASE("price falls toward the floor when inventory is high") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1000.0;
    double mc = firm_decisions::marginal_cost(f);
    f.price = mc * params::kPriceUpperFactor;

    bool ever_fell = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
        if (f.price < before) ever_fell = true;
        CHECK(f.price >= mc * params::kPriceLowerFactor - 1e-9);
    }
    CHECK(ever_fell);
}

TEST_CASE("price is untouched when inventory is inside the bands") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 50.0;
    f.price = 5.0;
    for (int i = 0; i < 50; ++i) firm_decisions::adjust_price(f, rng);
    CHECK(f.price == doctest::Approx(5.0));
}
```

Note: include `#include "network.hpp"` in this test file — it is needed by the empty-firm case.

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `firm_decisions.hpp: No such file or directory`.

- [ ] **Step 3: Implement firm decisions**

`src/firm_decisions.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace firm_decisions {

// Cost of producing one unit: one worker costs `wage` per month and yields
// lambda * days_per_month units, so marginal cost is independent of headcount.
double marginal_cost(const Firm& f);

void adjust_wage(Firm& f, Rng& rng);
void adjust_workforce(World& w, uint32_t firm, Rng& rng);
void adjust_price(Firm& f, Rng& rng);

// Runs the three rules above for every firm, in index order.
void run_monthly(World& w, Rng& rng);

}  // namespace firm_decisions
```

`src/firm_decisions.cpp`:

```cpp
#include "firm_decisions.hpp"

#include <algorithm>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace firm_decisions {

double marginal_cost(const Firm& f) {
    return f.wage / (params::kProductivity * params::kDaysPerMonth);
}

void adjust_wage(Firm& f, Rng& rng) {
    if (f.vacancy_went_unfilled) {
        double mu = rng.uniform(0.0, params::kWageAdjustMax);
        f.wage *= (1.0 + mu);
    } else if (f.months_fully_staffed >= params::kMonthsBeforeWageCut) {
        double mu = rng.uniform(0.0, params::kWageAdjustMax);
        f.wage *= (1.0 - mu);
        f.months_fully_staffed = 0;
    }
}

void adjust_workforce(World& w, uint32_t firm, Rng& rng) {
    Firm& f = w.firms[firm];
    double lower = params::kInventoryLowerFactor * f.demand_last_month;
    double upper = params::kInventoryUpperFactor * f.demand_last_month;

    f.has_open_vacancy = false;
    f.pending_fire = NO_HOUSEHOLD;

    if (f.inventory < lower) {
        f.has_open_vacancy = true;
    } else if (f.inventory > upper && !f.employees.empty()) {
        uint32_t slot = rng.index(static_cast<uint32_t>(f.employees.size()));
        f.pending_fire = f.employees[slot];
    }
}

void adjust_price(Firm& f, Rng& rng) {
    double lower_inv = params::kInventoryLowerFactor * f.demand_last_month;
    double upper_inv = params::kInventoryUpperFactor * f.demand_last_month;

    double mc = marginal_cost(f);
    double price_floor = params::kPriceLowerFactor * mc;
    double price_cap = params::kPriceUpperFactor * mc;

    double proposed = f.price;
    if (f.inventory < lower_inv) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        proposed = std::min(f.price * (1.0 + nu), price_cap);
    } else if (f.inventory > upper_inv) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        proposed = std::max(f.price * (1.0 - nu), price_floor);
    } else {
        return;  // inventory is comfortable; price is not reconsidered
    }

    // Calvo-style stickiness: the change is computed but only sometimes applied.
    if (rng.chance(params::kPriceChangeProb)) {
        f.price = proposed;
    }
}

void run_monthly(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        adjust_wage(w.firms[i], rng);
        adjust_workforce(w, i, rng);
        adjust_price(w.firms[i], rng);
    }
}

}  // namespace firm_decisions
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/firm_decisions.cpp` to `atc_core` and `tests/test_firm_decisions.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/firm_decisions.hpp src/firm_decisions.cpp tests/test_firm_decisions.cpp CMakeLists.txt
git commit -m "feat: firm monthly wage, workforce, and price rules"
```

---

### Task 5: Household monthly decisions

**Files:**
- Create: `src/household_decisions.hpp`, `src/household_decisions.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_household_decisions.cpp`

**Interfaces:**
- Consumes: `World`, `Rng`, `network`, `firm_decisions`
- Produces: `namespace household_decisions`: `void search_cheaper_shop(World&, uint32_t household, Rng&)`, `void search_reliable_shop(World&, uint32_t household, Rng&)`, `void search_job(World&, uint32_t household, Rng&)`, `double average_shop_price(const World&, uint32_t household)`, `void set_consumption_budget(World&, uint32_t household)`, `void run_monthly(World&, Rng&)`

**Rules being implemented** (spec §5.1 steps 4–6):
- With probability `ψ_Price`: compare one random connected shop with one random unconnected firm drawn by size; switch if the new price is at least `ξ` (1%) lower.
- With probability `ψ_Quant`: if any shop rationed the household last month, drop one — chosen with probability proportional to the shortfall — and replace it with a random unconnected firm.
- Job search: unemployed visit up to `β` firms; employed-and-underpaid visit one with probability 1; employed-and-satisfied visit one with probability `π`. A position is taken only if it has an open vacancy and pays more than current income (an unemployed household's current income is 0).
- Consumption budget: `c = min((m / P)^α, m / P)` where `P` is the mean price across connected shops.

- [ ] **Step 1: Write the failing tests**

`tests/test_household_decisions.cpp`:

```cpp
#include "doctest.h"
#include "household_decisions.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

#include <cmath>

TEST_CASE("average_shop_price averages the connected firms' prices") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = static_cast<double>(s + 1);
    }
    // prices 1..7, mean = 4
    CHECK(household_decisions::average_shop_price(w, 0) == doctest::Approx(4.0));
}

TEST_CASE("consumption budget follows (m/P)^alpha and respects the constraint") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 2.0;
    }
    w.households[0].liquidity = 200.0;

    household_decisions::set_consumption_budget(w, 0);

    double real_wealth = 200.0 / 2.0;  // 100
    double expected = std::pow(real_wealth, params::kConsumptionExponent);
    CHECK(w.households[0].planned_consumption == doctest::Approx(expected));
    CHECK(w.households[0].planned_consumption <= real_wealth);
}

TEST_CASE("consumption budget is capped by real wealth when wealth is below one") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 2.0;
    }
    w.households[0].liquidity = 1.0;  // real wealth 0.5, and 0.5^0.9 > 0.5

    household_decisions::set_consumption_budget(w, 0);
    CHECK(w.households[0].planned_consumption == doctest::Approx(0.5));
}

TEST_CASE("a strictly cheaper unconnected firm is adopted") {
    Rng rng(1);
    World w(10, 20, rng);
    for (auto& f : w.firms) f.price = 100.0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    // Every unconnected firm is far cheaper than every connected one.
    for (uint32_t f = params::kShopsPerHousehold; f < 20; ++f) w.firms[f].price = 1.0;

    bool switched = false;
    for (int i = 0; i < 500 && !switched; ++i) {
        household_decisions::search_cheaper_shop(w, 0, rng);
        for (uint32_t s : w.households[0].shops) {
            if (s >= params::kShopsPerHousehold) switched = true;
        }
    }
    CHECK(switched);
}

TEST_CASE("a marginally cheaper firm below the threshold is not adopted") {
    Rng rng(1);
    World w(10, 20, rng);
    for (auto& f : w.firms) f.price = 100.0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    // 0.5% cheaper — below the 1% switching threshold.
    for (uint32_t f = params::kShopsPerHousehold; f < 20; ++f) w.firms[f].price = 99.5;

    for (int i = 0; i < 500; ++i) household_decisions::search_cheaper_shop(w, 0, rng);
    for (uint32_t s : w.households[0].shops) {
        CHECK(s < params::kShopsPerHousehold);
    }
}

TEST_CASE("a rationing shop is dropped") {
    Rng rng(1);
    World w(10, 20, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    w.households[0].unsatisfied[3] = 50.0;  // only slot 3 rationed us

    bool replaced = false;
    for (int i = 0; i < 500 && !replaced; ++i) {
        household_decisions::search_reliable_shop(w, 0, rng);
        if (w.households[0].shops[3] != 3u) replaced = true;
    }
    CHECK(replaced);
}

TEST_CASE("an unemployed household takes an open vacancy") {
    Rng rng(1);
    World w(10, 3, rng);
    network::fire(w, 0);
    REQUIRE(w.households[0].employer == NO_FIRM);
    for (auto& f : w.firms) { f.has_open_vacancy = true; f.wage = 50.0; }
    w.households[0].reservation_wage = 10.0;

    household_decisions::search_job(w, 0, rng);
    CHECK(w.households[0].employer != NO_FIRM);
}

TEST_CASE("an unemployed household does not take a job at a firm with no vacancy") {
    Rng rng(1);
    World w(10, 3, rng);
    network::fire(w, 0);
    for (auto& f : w.firms) { f.has_open_vacancy = false; f.wage = 50.0; }

    for (int i = 0; i < 100; ++i) household_decisions::search_job(w, 0, rng);
    CHECK(w.households[0].employer == NO_FIRM);
}

TEST_CASE("an employed household only moves to a strictly better-paying vacancy") {
    Rng rng(1);
    World w(10, 3, rng);
    uint32_t start = w.households[0].employer;
    REQUIRE(start != NO_FIRM);
    for (auto& f : w.firms) { f.has_open_vacancy = true; f.wage = 10.0; }
    w.firms[start].wage = 100.0;          // current job is the best-paying
    w.households[0].reservation_wage = 5.0;

    for (int i = 0; i < 200; ++i) household_decisions::search_job(w, 0, rng);
    CHECK(w.households[0].employer == start);
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `household_decisions.hpp: No such file or directory`.

- [ ] **Step 3: Implement household decisions**

`src/household_decisions.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace household_decisions {

double average_shop_price(const World& w, uint32_t household);

void search_cheaper_shop(World& w, uint32_t household, Rng& rng);
void search_reliable_shop(World& w, uint32_t household, Rng& rng);
void search_job(World& w, uint32_t household, Rng& rng);
void set_consumption_budget(World& w, uint32_t household);

// Runs all four for every household, in a randomized order.
void run_monthly(World& w, Rng& rng);

}  // namespace household_decisions
```

`src/household_decisions.cpp`:

```cpp
#include "household_decisions.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace household_decisions {

double average_shop_price(const World& w, uint32_t household) {
    const Household& h = w.households[household];
    double sum = 0.0;
    for (uint32_t f : h.shops) sum += w.firms[f].price;
    return sum / static_cast<double>(params::kShopsPerHousehold);
}

void search_cheaper_shop(World& w, uint32_t household, Rng& rng) {
    if (!rng.chance(params::kProbPriceSearch)) return;

    Household& h = w.households[household];
    uint32_t slot = rng.index(params::kShopsPerHousehold);
    uint32_t incumbent = h.shops[slot];

    uint32_t challenger = network::random_unconnected_firm_by_size(w, household, rng);
    if (challenger == NO_FIRM) return;

    double incumbent_price = w.firms[incumbent].price;
    double challenger_price = w.firms[challenger].price;

    // Switch only if the challenger is at least xi cheaper, proportionally.
    if (challenger_price <= incumbent_price * (1.0 - params::kPriceSwitchThreshold)) {
        network::replace_shop(w, household, slot, challenger);
    }
}

void search_reliable_shop(World& w, uint32_t household, Rng& rng) {
    if (!rng.chance(params::kProbQuantSearch)) return;

    Household& h = w.households[household];
    double total_shortfall = 0.0;
    for (double s : h.unsatisfied) total_shortfall += s;
    if (total_shortfall <= 0.0) return;

    // Pick the shop to drop with probability proportional to its shortfall.
    double draw = rng.uniform(0.0, total_shortfall);
    uint32_t slot = 0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        draw -= h.unsatisfied[s];
        if (draw <= 0.0) { slot = s; break; }
    }

    uint32_t replacement = network::random_unconnected_firm_by_size(w, household, rng);
    if (replacement == NO_FIRM) return;
    network::replace_shop(w, household, slot, replacement);
}

void search_job(World& w, uint32_t household, Rng& rng) {
    Household& h = w.households[household];

    uint32_t visits = 0;
    if (!h.employed()) {
        visits = params::kUnemployedFirmVisits;
    } else if (w.firms[h.employer].wage < h.reservation_wage) {
        visits = 1;  // underpaid: always searching
    } else if (rng.chance(params::kEmployedSearchProb)) {
        visits = 1;  // satisfied: occasional look around
    }

    for (uint32_t v = 0; v < visits; ++v) {
        uint32_t candidate = rng.index(static_cast<uint32_t>(w.firms.size()));
        if (!w.firms[candidate].has_open_vacancy) continue;
        if (candidate == h.employer) continue;

        double current_income = h.employed() ? w.firms[h.employer].wage : 0.0;
        if (w.firms[candidate].wage > current_income) {
            network::hire(w, household, candidate);
            w.firms[candidate].has_open_vacancy = false;
            return;  // one job per month
        }
    }
}

void set_consumption_budget(World& w, uint32_t household) {
    Household& h = w.households[household];
    double avg_price = average_shop_price(w, household);
    if (avg_price <= 0.0) { h.planned_consumption = 0.0; return; }

    double real_wealth = h.liquidity / avg_price;
    double desired = std::pow(real_wealth, params::kConsumptionExponent);

    // The min enforces the budget constraint: for real_wealth < 1 the power
    // function would otherwise exceed available wealth.
    h.planned_consumption = std::min(desired, real_wealth);
}

void run_monthly(World& w, Rng& rng) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) {
        search_cheaper_shop(w, h, rng);
        search_reliable_shop(w, h, rng);
        search_job(w, h, rng);
        set_consumption_budget(w, h);

        // Shortfalls are consumed by this month's rewiring decision.
        w.households[h].unsatisfied.fill(0.0);
    }
}

}  // namespace household_decisions
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/household_decisions.cpp` to `atc_core` and `tests/test_household_decisions.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/household_decisions.hpp src/household_decisions.cpp tests/test_household_decisions.cpp CMakeLists.txt
git commit -m "feat: household monthly search and consumption rules"
```

---

### Task 6: The daily loop — shopping and production

**Files:**
- Create: `src/daily.hpp`, `src/daily.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_daily.cpp`

**Interfaces:**
- Consumes: `World`, `Rng`, `network`
- Produces: `namespace daily`: `void shop(World&, uint32_t household, Rng&)`, `void produce(World&)`, `void run_day(World&, Rng&)`

**Rules being implemented** (spec §5.2):
- Households act in randomized order. Each targets `planned_consumption / 21` units for the day.
- It visits one random connected shop. Purchase quantity is `min(target, firm.inventory, household.liquidity / price)`. Money moves from household to firm; inventory falls; `firm.demand_this_month` rises by the quantity the household *wanted* at that shop, and the shortfall is accumulated in `unsatisfied[slot]`.
- If unmet demand remains, retry at other connected shops, stopping after `n` visits or once 95% of the day's target is met. Any remainder is discarded.
- Firms then produce `λ × employees` into inventory.

- [ ] **Step 1: Write the failing tests**

`tests/test_daily.cpp`:

```cpp
#include "doctest.h"
#include "daily.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("production is linear in headcount") {
    Rng rng(1);
    World w(10, 2, rng);
    w.firms[0].inventory = 0.0;
    size_t n = w.firms[0].employees.size();
    daily::produce(w);
    CHECK(w.firms[0].inventory ==
          doctest::Approx(params::kProductivity * static_cast<double>(n)));
}

TEST_CASE("a firm with no employees produces nothing") {
    Rng rng(1);
    World w(10, 2, rng);
    while (!w.firms[0].employees.empty()) network::fire(w, w.firms[0].employees.front());
    w.firms[0].inventory = 0.0;
    daily::produce(w);
    CHECK(w.firms[0].inventory == doctest::Approx(0.0));
}

TEST_CASE("shopping moves money from household to firm and reduces inventory") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 2.0;
        w.firms[s].inventory = 1000.0;
        w.firms[s].liquidity = 0.0;
    }
    w.households[0].liquidity = 100.0;
    w.households[0].planned_consumption = 21.0;  // 1 unit per day

    double money_before = w.total_money();
    daily::shop(w, 0, rng);

    CHECK(w.households[0].liquidity == doctest::Approx(98.0));
    CHECK(w.total_money() == doctest::Approx(money_before));

    double sold = 0.0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        sold += 1000.0 - w.firms[s].inventory;
    }
    CHECK(sold == doctest::Approx(1.0));
}

TEST_CASE("a household cannot spend more money than it has") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 2.0;
        w.firms[s].inventory = 1000.0;
    }
    w.households[0].liquidity = 1.0;
    w.households[0].planned_consumption = 2100.0;  // 100/day, unaffordable

    daily::shop(w, 0, rng);
    CHECK(w.households[0].liquidity >= 0.0);
    CHECK(w.households[0].liquidity == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("inventory never goes negative and shortfall is recorded") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 1.0;
        w.firms[s].inventory = 0.0;  // nobody has anything
    }
    w.households[0].liquidity = 1000.0;
    w.households[0].planned_consumption = 210.0;  // 10/day

    daily::shop(w, 0, rng);

    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        CHECK(w.firms[s].inventory >= 0.0);
    }
    double total_unsatisfied = 0.0;
    for (double u : w.households[0].unsatisfied) total_unsatisfied += u;
    CHECK(total_unsatisfied > 0.0);
}

TEST_CASE("demand is recorded even when it cannot be satisfied") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
        w.firms[s].price = 1.0;
        w.firms[s].inventory = 0.0;
        w.firms[s].demand_this_month = 0.0;
    }
    w.households[0].liquidity = 1000.0;
    w.households[0].planned_consumption = 210.0;

    daily::shop(w, 0, rng);

    double recorded = 0.0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        recorded += w.firms[s].demand_this_month;
    }
    CHECK(recorded > 0.0);
}

TEST_CASE("a full day conserves money") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (auto& f : w.firms) { f.inventory = 500.0; f.price = 1.0; }
    for (auto& h : w.households) h.planned_consumption = 21.0;

    double before = w.total_money();
    daily::run_day(w, rng);
    CHECK(w.total_money() == doctest::Approx(before));
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `daily.hpp: No such file or directory`.

- [ ] **Step 3: Implement the daily loop**

`src/daily.hpp`:

```cpp
#pragma once

#include <cstdint>

#include "rng.hpp"

class World;

namespace daily {

// One household's shopping for one day.
void shop(World& w, uint32_t household, Rng& rng);

// All firms produce lambda * headcount into inventory.
void produce(World& w);

// Shopping (randomized household order) followed by production.
void run_day(World& w, Rng& rng);

}  // namespace daily
```

`src/daily.cpp`:

```cpp
#include "daily.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace daily {

void shop(World& w, uint32_t household, Rng& rng) {
    Household& h = w.households[household];

    double target = h.planned_consumption / params::kDaysPerMonth;
    if (target <= 0.0) return;

    double remaining = target;
    const double stop_at = target * (1.0 - params::kDemandSatisfiedFraction);

    for (uint32_t visit = 0; visit < params::kMaxShopVisitsPerDay; ++visit) {
        if (remaining <= stop_at) break;

        uint32_t slot = rng.index(params::kShopsPerHousehold);
        uint32_t firm_idx = h.shops[slot];
        Firm& f = w.firms[firm_idx];

        const double wanted = remaining;

        // The firm learns what was wanted, whether or not it can supply it.
        f.demand_this_month += wanted;

        if (f.price <= 0.0) continue;

        double affordable = h.liquidity / f.price;
        double bought = std::min({wanted, f.inventory, affordable});

        if (bought > 0.0) {
            // Clamp the cost to available liquidity so floating-point division
            // can never push the household slightly negative.
            double cost = std::min(bought * f.price, h.liquidity);
            h.liquidity -= cost;
            f.liquidity += cost;
            f.inventory -= bought;
            remaining -= bought;
        }

        // Whatever this shop could not supply is its share of the shortfall.
        // Only stock-outs count as rationing — running out of money is the
        // household's own constraint, not the firm's failure.
        double stock_shortfall = wanted - std::min(wanted, f.inventory + bought);
        if (stock_shortfall > 0.0) {
            h.unsatisfied[slot] += stock_shortfall;
        }

        if (h.liquidity <= 0.0) break;
    }
    // Any demand still unmet at the end of the day simply vanishes.
}

void produce(World& w) {
    for (auto& f : w.firms) {
        f.inventory += params::kProductivity * static_cast<double>(f.employees.size());
    }
}

void run_day(World& w, Rng& rng) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) shop(w, h, rng);
    produce(w);
}

}  // namespace daily
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/daily.cpp` to `atc_core` and `tests/test_daily.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass. If the "cannot spend more than it has" test leaves a small negative liquidity, clamp `bought` so that `cost <= h.liquidity` exactly rather than relying on the division.

- [ ] **Step 6: Commit**

```bash
git add src/daily.hpp src/daily.cpp tests/test_daily.cpp CMakeLists.txt
git commit -m "feat: daily shopping and production"
```

---

### Task 7: Month close — wages, buffer, profits, reservation wages

**Files:**
- Create: `src/month_close.hpp`, `src/month_close.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_month_close.cpp`

**Interfaces:**
- Consumes: `World`, `network`
- Produces: `namespace month_close`: `void pay_wages(World&)`, `void distribute_profits(World&)`, `void update_reservation_wages(World&)`, `void apply_pending_fires(World&)`, `void roll_demand(World&)`, `void run(World&)`

**Rules being implemented** (spec §5.3, plus Lengnick §2.4):
- Firms pay `wage` to each employee. If the wage bill exceeds liquidity even after draining the buffer, employees accept an immediate wage cut sufficient to keep the firm operating — this is what keeps firm count constant with no bankruptcy.
- Firms retain `χ × wage_bill` as a buffer; all liquidity above that is distributed to households as profit, **in proportion to each household's current liquidity** (a proxy for stock ownership: the wealthy own more).
- Reservation wage: raised to actual labour income if income exceeds it; cut 10% if the household was unemployed. Never lowered merely because the employer cut wages — instead the household searches harder.
- Pending fires from last month are applied. `demand_this_month` rolls into `demand_last_month` and resets.

- [ ] **Step 1: Write the failing tests**

`tests/test_month_close.cpp`:

```cpp
#include "doctest.h"
#include "month_close.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("wages move money from firms to their employees") {
    Rng rng(1);
    World w(10, 2, rng);
    for (auto& f : w.firms) { f.wage = 10.0; f.liquidity = 1000.0; }
    for (auto& h : w.households) h.liquidity = 0.0;

    double before = w.total_money();
    month_close::pay_wages(w);

    CHECK(w.total_money() == doctest::Approx(before));
    for (const auto& h : w.households) {
        if (h.employed()) CHECK(h.liquidity == doctest::Approx(10.0));
    }
}

TEST_CASE("an unaffordable wage bill triggers a wage cut rather than bankruptcy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.wage = 1000.0;
    f.liquidity = 10.0;  // nowhere near the wage bill
    size_t n = f.employees.size();
    REQUIRE(n > 0);

    double before = w.total_money();
    month_close::pay_wages(w);

    CHECK(f.wage < 1000.0);
    CHECK(f.liquidity >= -1e-9);
    CHECK(w.total_money() == doctest::Approx(before));
}

TEST_CASE("profits are distributed in proportion to household liquidity") {
    Rng rng(1);
    World w(2, 1, rng);
    w.firms[0].wage = 0.0;
    w.firms[0].liquidity = 300.0;
    w.households[0].liquidity = 100.0;
    w.households[1].liquidity = 200.0;

    double before = w.total_money();
    month_close::distribute_profits(w);

    // Buffer is chi * wage bill = 0, so all 300 is distributed 1:2.
    CHECK(w.households[0].liquidity == doctest::Approx(200.0));
    CHECK(w.households[1].liquidity == doctest::Approx(400.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(0.0));
    CHECK(w.total_money() == doctest::Approx(before));
}

TEST_CASE("a firm retains its buffer before distributing profit") {
    Rng rng(1);
    World w(4, 1, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    f.liquidity = 1000.0;
    double wage_bill = f.wage * static_cast<double>(f.employees.size());
    double expected_buffer = params::kBufferFactor * wage_bill;

    month_close::distribute_profits(w);
    CHECK(f.liquidity == doctest::Approx(expected_buffer));
}

TEST_CASE("a firm below its buffer distributes nothing") {
    Rng rng(1);
    World w(4, 1, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    f.liquidity = 1.0;
    double before = w.total_money();

    month_close::distribute_profits(w);

    CHECK(f.liquidity == doctest::Approx(1.0));
    CHECK(w.total_money() == doctest::Approx(before));
}

TEST_CASE("reservation wage rises to match a higher income") {
    Rng rng(1);
    World w(4, 1, rng);
    w.firms[0].wage = 80.0;
    w.households[0].reservation_wage = 50.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(80.0));
}

TEST_CASE("reservation wage does not fall when the employer cuts pay") {
    Rng rng(1);
    World w(4, 1, rng);
    w.firms[0].wage = 20.0;
    w.households[0].reservation_wage = 50.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(50.0));
}

TEST_CASE("an unemployed household cuts its reservation wage by ten percent") {
    Rng rng(1);
    World w(4, 2, rng);
    network::fire(w, 0);
    w.households[0].reservation_wage = 100.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(90.0));
}

TEST_CASE("pending fires are applied and cleared") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(!f.employees.empty());
    uint32_t victim = f.employees.front();
    f.pending_fire = victim;

    month_close::apply_pending_fires(w);

    CHECK(w.households[victim].employer == NO_FIRM);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("demand rolls over and resets") {
    Rng rng(1);
    World w(10, 2, rng);
    w.firms[0].demand_this_month = 42.0;
    w.firms[0].demand_last_month = 7.0;
    month_close::roll_demand(w);
    CHECK(w.firms[0].demand_last_month == doctest::Approx(42.0));
    CHECK(w.firms[0].demand_this_month == doctest::Approx(0.0));
}

TEST_CASE("a full month close conserves money") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (auto& f : w.firms) { f.liquidity = 500.0; f.wage = 30.0; }
    double before = w.total_money();
    month_close::run(w);
    CHECK(w.total_money() == doctest::Approx(before));
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `month_close.hpp: No such file or directory`.

- [ ] **Step 3: Implement month close**

`src/month_close.hpp`:

```cpp
#pragma once

class World;

namespace month_close {

void pay_wages(World& w);
void distribute_profits(World& w);
void update_reservation_wages(World& w);
void apply_pending_fires(World& w);
void roll_demand(World& w);

// The full end-of-month sequence, in order.
void run(World& w);

}  // namespace month_close
```

`src/month_close.cpp`:

```cpp
#include "month_close.hpp"

#include <algorithm>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace month_close {

void pay_wages(World& w) {
    for (auto& f : w.firms) {
        double headcount = static_cast<double>(f.employees.size());
        if (headcount == 0.0) continue;

        double bill = f.wage * headcount;

        // Lengnick 2013 §2.4: rather than allow bankruptcy, employees accept
        // a wage cut just large enough to keep the firm operating. This is
        // what keeps the firm count constant with no entry or exit.
        if (bill > f.liquidity) {
            f.wage = std::max(0.0, f.liquidity / headcount);
            bill = f.wage * headcount;
        }

        f.liquidity -= bill;
        for (uint32_t h : f.employees) w.households[h].liquidity += f.wage;
    }
}

void distribute_profits(World& w) {
    // Ownership proxy: profit shares are proportional to household liquidity,
    // so the wealthy receive more. This is the model's inequality engine.
    double total_household_liquidity = 0.0;
    for (const auto& h : w.households) total_household_liquidity += h.liquidity;

    for (auto& f : w.firms) {
        double wage_bill = f.wage * static_cast<double>(f.employees.size());
        double buffer = params::kBufferFactor * wage_bill;
        double profit = f.liquidity - buffer;
        if (profit <= 0.0) continue;

        f.liquidity -= profit;

        if (total_household_liquidity <= 0.0) {
            // Degenerate case: split evenly rather than destroying money.
            double share = profit / static_cast<double>(w.households.size());
            for (auto& h : w.households) h.liquidity += share;
        } else {
            for (auto& h : w.households) {
                h.liquidity += profit * (h.liquidity / total_household_liquidity);
            }
        }
    }
}

void update_reservation_wages(World& w) {
    for (auto& h : w.households) {
        if (!h.employed()) {
            h.reservation_wage *= (1.0 - params::kUnemployedReservationCut);
            continue;
        }
        double income = w.firms[h.employer].wage;
        if (income > h.reservation_wage) {
            h.reservation_wage = income;
        }
        // If income < reservation wage the claim is NOT lowered; the household
        // searches harder instead (see household_decisions::search_job).
    }
}

void apply_pending_fires(World& w) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        Firm& f = w.firms[i];
        if (f.pending_fire == NO_HOUSEHOLD) continue;
        uint32_t victim = f.pending_fire;
        f.pending_fire = NO_HOUSEHOLD;
        // Only fire if still employed here — they may have quit in the interim.
        if (w.households[victim].employer == i) {
            network::fire(w, victim);
        }
    }
}

void roll_demand(World& w) {
    for (auto& f : w.firms) {
        f.demand_last_month = f.demand_this_month;
        f.demand_this_month = 0.0;
    }
}

void run(World& w) {
    pay_wages(w);
    distribute_profits(w);
    update_reservation_wages(w);
    apply_pending_fires(w);
    roll_demand(w);
}

}  // namespace month_close
```

Note on `apply_pending_fires`: the loop is deliberately indexed rather than range-based, because the firm's own index is needed to confirm the household still works there. `w.firms.size()` returns `size_t`, so the comparison may emit a sign-compare warning under `-Wextra`; cast it: `i < static_cast<uint32_t>(w.firms.size())`.

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/month_close.cpp` to `atc_core` and `tests/test_month_close.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/month_close.hpp src/month_close.cpp tests/test_month_close.cpp CMakeLists.txt
git commit -m "feat: month close with wages, profits, and reservation wage updates"
```

---

### Task 8: Scheduler — the full tick

**Files:**
- Create: `src/scheduler.hpp`, `src/scheduler.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_scheduler.cpp`

**Interfaces:**
- Consumes: everything from Tasks 2–7
- Produces: `class Scheduler` with `Scheduler(World&, Rng&)`, `void run_month()`, `int months_elapsed() const`

**Order within a month** (spec §5): firm monthly decisions → household monthly decisions → 21 days of shopping and production → month close. Vacancy bookkeeping (`vacancy_went_unfilled`, `months_fully_staffed`) is settled after household job search, since that is when vacancies are either taken or left open.

- [ ] **Step 1: Write the failing tests**

`tests/test_scheduler.cpp`:

```cpp
#include "doctest.h"
#include "scheduler.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("a month advances the counter") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    CHECK(s.months_elapsed() == 0);
    s.run_month();
    CHECK(s.months_elapsed() == 1);
}

TEST_CASE("money is conserved across many months") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    double before = w.total_money();
    for (int i = 0; i < 120; ++i) s.run_month();
    CHECK(w.total_money() == doctest::Approx(before).epsilon(1e-9));
}

TEST_CASE("no liquidity ever goes negative") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 120; ++i) {
        s.run_month();
        for (const auto& h : w.households) CHECK(h.liquidity >= -1e-9);
        for (const auto& f : w.firms) CHECK(f.liquidity >= -1e-9);
    }
}

TEST_CASE("inventories never go negative") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 120; ++i) {
        s.run_month();
        for (const auto& f : w.firms) CHECK(f.inventory >= -1e-9);
    }
}

TEST_CASE("employment links stay consistent over time") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 60; ++i) s.run_month();

    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        for (uint32_t h : w.firms[f].employees) {
            CHECK(w.households[h].employer == f);
        }
    }
    for (uint32_t h = 0; h < w.households.size(); ++h) {
        uint32_t f = w.households[h].employer;
        if (f == NO_FIRM) continue;
        const auto& e = w.firms[f].employees;
        CHECK(std::find(e.begin(), e.end(), h) != e.end());
    }
}

TEST_CASE("no household is employed by two firms") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 60; ++i) s.run_month();

    std::vector<int> employer_count(w.households.size(), 0);
    for (const auto& f : w.firms) {
        for (uint32_t h : f.employees) employer_count[h]++;
    }
    for (int c : employer_count) CHECK(c <= 1);
}

TEST_CASE("identical seeds produce identical runs") {
    auto run = [](uint64_t seed) {
        Rng rng(seed);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler s(w, rng);
        for (int i = 0; i < 60; ++i) s.run_month();
        std::vector<double> snapshot;
        for (const auto& f : w.firms) {
            snapshot.push_back(f.price);
            snapshot.push_back(f.wage);
            snapshot.push_back(f.inventory);
        }
        for (const auto& h : w.households) snapshot.push_back(h.liquidity);
        return snapshot;
    };
    CHECK(run(777) == run(777));
}

TEST_CASE("different seeds produce different runs") {
    auto price_of_firm_zero = [](uint64_t seed) {
        Rng rng(seed);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler s(w, rng);
        for (int i = 0; i < 60; ++i) s.run_month();
        return w.firms[0].price;
    };
    CHECK(price_of_firm_zero(1) != price_of_firm_zero(2));
}
```

Add `#include <algorithm>` and `#include <vector>` to this test file.

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `scheduler.hpp: No such file or directory`.

- [ ] **Step 3: Implement the scheduler**

`src/scheduler.hpp`:

```cpp
#pragma once

#include "rng.hpp"

class World;

// Drives the day/month tick. Holds no state beyond the month counter;
// all simulation state lives in World.
class Scheduler {
public:
    Scheduler(World& world, Rng& rng) : world_(world), rng_(rng) {}

    void run_month();
    int months_elapsed() const { return months_elapsed_; }

private:
    void settle_vacancy_bookkeeping();

    World& world_;
    Rng& rng_;
    int months_elapsed_ = 0;
};
```

`src/scheduler.cpp`:

```cpp
#include "scheduler.hpp"

#include "daily.hpp"
#include "firm_decisions.hpp"
#include "household_decisions.hpp"
#include "month_close.hpp"
#include "params.hpp"
#include "world.hpp"

void Scheduler::settle_vacancy_bookkeeping() {
    // Called after job search: a vacancy still open was not filled.
    for (auto& f : world_.firms) {
        f.vacancy_went_unfilled = f.has_open_vacancy;
        if (f.has_open_vacancy) {
            f.months_fully_staffed = 0;
        } else {
            ++f.months_fully_staffed;
        }
    }
}

void Scheduler::run_month() {
    firm_decisions::run_monthly(world_, rng_);
    household_decisions::run_monthly(world_, rng_);
    settle_vacancy_bookkeeping();

    for (int day = 0; day < params::kDaysPerMonth; ++day) {
        daily::run_day(world_, rng_);
    }

    month_close::run(world_);
    ++months_elapsed_;
}
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/scheduler.cpp` to `atc_core` and `tests/test_scheduler.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass. The money-conservation test is the important one — if it fails, the bug is in `daily::shop`, `month_close::pay_wages`, or `month_close::distribute_profits`, in that order of likelihood.

- [ ] **Step 6: Commit**

```bash
git add src/scheduler.hpp src/scheduler.cpp tests/test_scheduler.cpp CMakeLists.txt
git commit -m "feat: scheduler assembling the full day/month tick"
```

---

### Task 9: Recorder and aggregate metrics

**Files:**
- Create: `src/recorder.hpp`, `src/recorder.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_recorder.cpp`

**Interfaces:**
- Consumes: `World`
- Produces: `struct Metrics` (fields: `month`, `unemployment_rate`, `mean_price`, `mean_wage`, `total_production`, `vacancies`, `gini`, `total_money`), `Metrics compute_metrics(const World&, int month)`, `class Recorder` (abstract, `virtual void record(const Metrics&) = 0`), `class CsvRecorder : public Recorder`

**Metric definitions:**
- `unemployment_rate` = unemployed households / total households
- `mean_price`, `mean_wage` = unweighted means across firms
- `total_production` = `λ × total employed`, the month's output rate
- `vacancies` = count of firms with `has_open_vacancy`
- `gini` = Gini coefficient of household liquidity, computed on the sorted vector as `(2·Σ(i+1)·xᵢ) / (n·Σxᵢ) − (n+1)/n`

- [ ] **Step 1: Write the failing tests**

`tests/test_recorder.cpp`:

```cpp
#include "doctest.h"
#include "recorder.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

TEST_CASE("unemployment rate counts unemployed households") {
    Rng rng(1);
    World w(10, 2, rng);
    for (uint32_t h = 0; h < 4; ++h) network::fire(w, h);
    Metrics m = compute_metrics(w, 3);
    CHECK(m.month == 3);
    CHECK(m.unemployment_rate == doctest::Approx(0.4));
}

TEST_CASE("mean price and wage average across firms") {
    Rng rng(1);
    World w(10, 2, rng);
    w.firms[0].price = 2.0;  w.firms[1].price = 4.0;
    w.firms[0].wage  = 10.0; w.firms[1].wage  = 30.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.mean_price == doctest::Approx(3.0));
    CHECK(m.mean_wage == doctest::Approx(20.0));
}

TEST_CASE("gini is zero for a perfectly equal distribution") {
    Rng rng(1);
    World w(100, 2, rng);
    for (auto& h : w.households) h.liquidity = 50.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.gini == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("gini approaches one when one household holds everything") {
    Rng rng(1);
    World w(100, 2, rng);
    for (auto& h : w.households) h.liquidity = 0.0;
    w.households[0].liquidity = 1000.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.gini > 0.95);
}

TEST_CASE("vacancies are counted") {
    Rng rng(1);
    World w(10, 4, rng);
    w.firms[0].has_open_vacancy = true;
    w.firms[2].has_open_vacancy = true;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.vacancies == 2);
}

TEST_CASE("csv recorder writes a header and one row per record") {
    const std::string path = "test_output.csv";
    std::remove(path.c_str());
    {
        CsvRecorder rec(path);
        Metrics m{};
        m.month = 1; m.unemployment_rate = 0.5; m.mean_price = 2.0;
        m.mean_wage = 20.0; m.total_production = 30.0; m.vacancies = 2;
        m.gini = 0.25; m.total_money = 1000.0;
        rec.record(m);
        m.month = 2;
        rec.record(m);
    }

    std::ifstream in(path);
    REQUIRE(in.good());
    std::string line;
    std::getline(in, line);
    CHECK(line.find("month") != std::string::npos);
    CHECK(line.find("unemployment_rate") != std::string::npos);
    CHECK(line.find("gini") != std::string::npos);

    int rows = 0;
    while (std::getline(in, line)) if (!line.empty()) ++rows;
    CHECK(rows == 2);

    std::remove(path.c_str());
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build -j
```

Expected: FAIL with `recorder.hpp: No such file or directory`.

- [ ] **Step 3: Implement the recorder**

`src/recorder.hpp`:

```cpp
#pragma once

#include <fstream>
#include <string>

class World;

struct Metrics {
    int month = 0;
    double unemployment_rate = 0.0;
    double mean_price = 0.0;
    double mean_wage = 0.0;
    double total_production = 0.0;
    int vacancies = 0;
    double gini = 0.0;
    double total_money = 0.0;
};

Metrics compute_metrics(const World& w, int month);

// The simulation core's only output path. The core never does I/O directly.
class Recorder {
public:
    virtual ~Recorder() = default;
    virtual void record(const Metrics& m) = 0;
};

class CsvRecorder : public Recorder {
public:
    explicit CsvRecorder(const std::string& path);
    void record(const Metrics& m) override;

private:
    std::ofstream out_;
};
```

`src/recorder.cpp`:

```cpp
#include "recorder.hpp"

#include <algorithm>
#include <vector>

#include "params.hpp"
#include "world.hpp"

Metrics compute_metrics(const World& w, int month) {
    Metrics m;
    m.month = month;

    uint32_t unemployed = 0;
    uint32_t employed = 0;
    for (const auto& h : w.households) {
        if (h.employed()) ++employed; else ++unemployed;
    }
    m.unemployment_rate =
        static_cast<double>(unemployed) / static_cast<double>(w.households.size());
    m.total_production = params::kProductivity * static_cast<double>(employed);

    double price_sum = 0.0;
    double wage_sum = 0.0;
    for (const auto& f : w.firms) {
        price_sum += f.price;
        wage_sum += f.wage;
        if (f.has_open_vacancy) ++m.vacancies;
    }
    m.mean_price = price_sum / static_cast<double>(w.firms.size());
    m.mean_wage = wage_sum / static_cast<double>(w.firms.size());

    // Gini of household liquidity, on the sorted distribution.
    std::vector<double> x;
    x.reserve(w.households.size());
    for (const auto& h : w.households) x.push_back(std::max(0.0, h.liquidity));
    std::sort(x.begin(), x.end());

    double sum = 0.0;
    double weighted = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        sum += x[i];
        weighted += static_cast<double>(i + 1) * x[i];
    }
    const double n = static_cast<double>(x.size());
    m.gini = (sum > 0.0) ? (2.0 * weighted) / (n * sum) - (n + 1.0) / n : 0.0;

    m.total_money = w.total_money();
    return m;
}

CsvRecorder::CsvRecorder(const std::string& path) : out_(path) {
    out_ << "month,unemployment_rate,mean_price,mean_wage,"
            "total_production,vacancies,gini,total_money\n";
}

void CsvRecorder::record(const Metrics& m) {
    out_ << m.month << ',' << m.unemployment_rate << ',' << m.mean_price << ','
         << m.mean_wage << ',' << m.total_production << ',' << m.vacancies << ','
         << m.gini << ',' << m.total_money << '\n';
}
```

- [ ] **Step 4: Register in CMakeLists.txt**

Add `src/recorder.cpp` to `atc_core` and `tests/test_recorder.cpp` to `atc_tests`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add src/recorder.hpp src/recorder.cpp tests/test_recorder.cpp CMakeLists.txt
git commit -m "feat: aggregate metrics and CSV recorder"
```

---

### Task 10: CLI entry point and success-criteria validation

**Files:**
- Create: `src/main.cpp`, `scripts/plot.py`
- Modify: `CMakeLists.txt`
- Test: `tests/test_scheduler.cpp` (add the burn-in independence case)

**Interfaces:**
- Consumes: everything
- Produces: the `economy` binary, `./build/atc --seed N --months M --burn-in B --out PATH`

- [ ] **Step 1: Write the failing burn-in independence test**

Append to `tests/test_scheduler.cpp`:

```cpp
TEST_CASE("post-burn-in behaviour does not depend on the initial network") {
    // Two runs differing ONLY in the initial random network (different seeds
    // for construction, same seed for the dynamics) should reach statistically
    // similar unemployment after burn-in. This is a weak but real check that
    // initial conditions wash out.
    auto mean_unemployment_after_burn_in = [](uint64_t init_seed) {
        Rng init_rng(init_seed);
        World w(params::kNumHouseholds, params::kNumFirms, init_rng);
        Rng dyn_rng(4242);
        Scheduler s(w, dyn_rng);
        for (int i = 0; i < 300; ++i) s.run_month();

        double total = 0.0;
        for (int i = 0; i < 100; ++i) {
            s.run_month();
            uint32_t unemployed = 0;
            for (const auto& h : w.households) if (!h.employed()) ++unemployed;
            total += static_cast<double>(unemployed) / w.households.size();
        }
        return total / 100.0;
    };

    double a = mean_unemployment_after_burn_in(11);
    double b = mean_unemployment_after_burn_in(999);
    CHECK(std::abs(a - b) < 0.15);
}
```

Add `#include <cmath>` to the file.

- [ ] **Step 2: Run to verify it compiles and observe the result**

```bash
cmake --build build -j && ./build/atc_tests
```

Expected: compiles. If this test fails, that is a genuine finding about the model, not a build error — record the two values before changing anything.

- [ ] **Step 3: Write main.cpp**

`src/main.cpp`:

```cpp
#include <charconv>
#include <cstdint>
#include <print>
#include <string>
#include <string_view>

#include "params.hpp"
#include "recorder.hpp"
#include "rng.hpp"
#include "scheduler.hpp"
#include "world.hpp"

namespace {

long parse_long(std::string_view s, long fallback) {
    long value = fallback;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    return (ec == std::errc{}) ? value : fallback;
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t seed = 1;
    int months = params::kMeasurementMonths;
    int burn_in = params::kBurnInMonths;
    std::string out_path = "output/run.csv";

    for (int i = 1; i + 1 < argc; i += 2) {
        std::string_view flag(argv[i]);
        std::string_view value(argv[i + 1]);
        if (flag == "--seed")         seed = static_cast<uint64_t>(parse_long(value, 1));
        else if (flag == "--months")  months = static_cast<int>(parse_long(value, months));
        else if (flag == "--burn-in") burn_in = static_cast<int>(parse_long(value, burn_in));
        else if (flag == "--out")     out_path = std::string(value);
    }

    std::println("seed={} burn_in={} months={} out={}", seed, burn_in, months, out_path);

    Rng rng(seed);
    World world(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler scheduler(world, rng);

    const double money_at_start = world.total_money();

    CsvRecorder recorder(out_path);

    for (int m = 0; m < burn_in; ++m) scheduler.run_month();

    for (int m = 0; m < months; ++m) {
        scheduler.run_month();
        recorder.record(compute_metrics(world, m));
    }

    const double money_at_end = world.total_money();
    std::println("money start={:.6f} end={:.6f} drift={:.9f}",
                 money_at_start, money_at_end, money_at_end - money_at_start);

    return 0;
}
```

- [ ] **Step 4: Register the binary in CMakeLists.txt**

```cmake
add_executable(economy src/main.cpp)
target_link_libraries(economy PRIVATE atc_core)
```

- [ ] **Step 5: Build and run the full simulation**

```bash
cmake --build build -j
mkdir -p output
./build/atc --seed 1 --burn-in 1000 --months 6000 --out output/run.csv
```

Expected: completes, and the reported `drift` is smaller than `1e-6`. A non-trivial drift means money is being created or destroyed — stop and fix that before looking at any other output.

- [ ] **Step 6: Write the validation plotting script**

`scripts/plot.py`:

```python
"""Throwaway validation plots for the v1 success criteria.

Nothing in the C++ core depends on this file. Delete it freely.
Requires: pip install matplotlib pandas
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt

path = sys.argv[1] if len(sys.argv) > 1 else "output/run.csv"
df = pd.read_csv(path)

df["wage_inflation"] = df["mean_wage"].pct_change()

fig, axes = plt.subplots(2, 2, figsize=(13, 9))

axes[0][0].plot(df["month"], df["unemployment_rate"], linewidth=0.6)
axes[0][0].set_title("Criterion 1: endogenous unemployment fluctuation")
axes[0][0].set_xlabel("month")
axes[0][0].set_ylabel("unemployment rate")

axes[0][1].scatter(df["unemployment_rate"], df["wage_inflation"], s=1, alpha=0.2)
axes[0][1].set_title("Criterion 2: Phillips curve (expect negative slope)")
axes[0][1].set_xlabel("unemployment rate")
axes[0][1].set_ylabel("wage inflation")

axes[1][0].scatter(df["unemployment_rate"], df["vacancies"], s=1, alpha=0.2)
axes[1][0].set_title("Criterion 3: Beveridge curve (expect negative slope)")
axes[1][0].set_xlabel("unemployment rate")
axes[1][0].set_ylabel("vacancies")

axes[1][1].plot(df["month"], df["gini"], linewidth=0.6)
axes[1][1].set_title("Criterion 4: wealth dispersion")
axes[1][1].set_xlabel("month")
axes[1][1].set_ylabel("Gini of household liquidity")

plt.tight_layout()
plt.savefig("output/validation.png", dpi=130)
print("wrote output/validation.png")

corr_phillips = df["unemployment_rate"].corr(df["wage_inflation"])
corr_beveridge = df["unemployment_rate"].corr(df["vacancies"])
print(f"Phillips correlation:  {corr_phillips:+.3f}  (want negative)")
print(f"Beveridge correlation: {corr_beveridge:+.3f}  (want negative)")
print(f"unemployment: mean={df['unemployment_rate'].mean():.3f} "
      f"sd={df['unemployment_rate'].std():.3f}  (want sd > 0)")
print(f"gini: mean={df['gini'].mean():.3f}  (want > 0)")
```

- [ ] **Step 7: Check the four success criteria**

```bash
pip install --user matplotlib pandas
python3 scripts/plot.py output/run.csv
```

Record the four printed numbers. The spec's criteria are met when:

1. `unemployment sd > 0` and the time series visibly cycles rather than sitting flat.
2. `Phillips correlation` is negative.
3. `Beveridge correlation` is negative.
4. `gini mean > 0` and does not collapse to zero over time.

If any criterion fails, **do not tune parameters first.** In order: re-check money conservation, re-check the `apply_pending_fires` index comparison, re-read the "Known ambiguity" note about household φ in this plan's calibration section, then re-read the corresponding rule in the spec against Lengnick's text.

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp scripts/plot.py tests/test_scheduler.cpp CMakeLists.txt
git commit -m "feat: CLI entry point and success-criteria validation"
```

- [ ] **Step 9: Record the outcome**

Append a short "v1 results" section to the spec at
`docs/superpowers/specs/2026-08-27-agent-based-economy-design.md` giving the
four measured numbers and whether each criterion passed. Commit it. This is
the baseline every future extension will be compared against.

```bash
git add docs/superpowers/specs/2026-08-27-agent-based-economy-design.md
git commit -m "docs: record v1 baseline results"
```

---

## Notes for the implementer

- **Money conservation is the master test.** It is checked in Tasks 6, 7, and 8, and printed by the binary. If it ever drifts, fix that before anything else — every other symptom downstream will be misleading.
- **Do not tune the calibration** until all ten tasks are done and the criteria have been measured once with the published values. The parameters come from a paper that reproduced these results; if they do not reproduce here, the bug is far more likely to be in the implementation.
- **The plotting script is disposable.** Nothing in `src/` may ever include or depend on it.
- **Resist adding features.** Regions, multiple goods, firm-to-firm trade, and labour types all have reasons to wait, recorded in spec §7 and §8. v1 is finished when the four criteria are measured.
