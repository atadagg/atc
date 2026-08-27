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
