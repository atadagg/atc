#include "doctest.h"
#include "world.hpp"
#include "params.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

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

TEST_CASE("small world (fewer firms than shops) completes and is valid") {
    // Regression test for infinite loop when n_firms < kShopsPerHousehold.
    // With 2 firms and 7 shops per household, shops must repeat; the
    // constructor must still complete (not hang).
    Rng rng(1);
    World w(10, 2, rng);
    CHECK(w.households.size() == 10);
    CHECK(w.firms.size() == 2);
    for (const auto& h : w.households) {
        for (uint32_t s : h.shops) {
            CHECK(s < 2);  // All shop indices are valid (0 or 1)
        }
    }
}

TEST_CASE("zero firms throws instead of dividing by zero") {
    // assert() compiles out under NDEBUG (the default Release build), so
    // this precondition violation must be a real, always-active throw.
    Rng rng(1);
    CHECK_THROWS_AS(World(10, 0, rng), std::invalid_argument);
}

TEST_CASE("zero households throws") {
    Rng rng(1);
    CHECK_THROWS_AS(World(0, 10, rng), std::invalid_argument);
}

TEST_CASE("degenerate single-firm world is valid") {
    // Test the edge case of a single firm. All households must be employed
    // by firm 0, and all shop slots must reference firm 0.
    Rng rng(1);
    World w(4, 1, rng);
    CHECK(w.households.size() == 4);
    CHECK(w.firms.size() == 1);
    for (uint32_t h = 0; h < 4; ++h) {
        CHECK(w.households[h].employer == 0);
        for (uint32_t s : w.households[h].shops) {
            CHECK(s == 0);
        }
    }
    CHECK(w.firms[0].employees.size() == 4);
}
