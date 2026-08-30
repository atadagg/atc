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

TEST_CASE("random_unconnected_firm never returns a connected firm") {
    Rng rng(1);
    World w(50, 20, rng);
    for (int trial = 0; trial < 500; ++trial) {
        uint32_t f = network::random_unconnected_firm(w, 0, rng);
        if (f == NO_FIRM) continue;
        CHECK_FALSE(network::has_shop(w.households[0], f));
        CHECK(f < 20u);
    }
}

TEST_CASE("random_unconnected_firm returns NO_FIRM when all are connected") {
    Rng rng(1);
    World w(10, params::kShopsPerHousehold, rng);
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    CHECK(network::random_unconnected_firm(w, 0, rng) == NO_FIRM);
}

// This is the property that separates it from random_unconnected_firm_by_size,
// and the reason it exists. The rationed-household rewiring path (spec §4,
// P_quant) applies no price test at all, so a size weighting there is
// unopposed: a household turned away by a large firm is routed to another
// large firm ~= size times as often, and a firm that has lost its staff can
// never regain the customers that would justify rehiring. That feedback loop
// left 66 of 100 firms permanently empty and pinned at kMinWage.
TEST_CASE("random_unconnected_firm ignores firm size") {
    Rng rng(1);
    World w(200, 10, rng);
    // Disconnect household 0 from everything, then make firm 9 huge and the
    // rest empty. Employment is irrelevant to the draw, so equal odds hold.
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        network::replace_shop(w, 0, s, s);
    }
    for (auto& f : w.firms) f.employees.clear();
    for (uint32_t h = 1; h <= 150; ++h) w.firms[9].employees.push_back(h);

    // Firms 7, 8 and 9 are the only unconnected ones; 9 has 150 employees.
    int counts[10] = {};
    const int kTrials = 30000;
    for (int t = 0; t < kTrials; ++t) {
        uint32_t f = network::random_unconnected_firm(w, 0, rng);
        REQUIRE(f != NO_FIRM);
        ++counts[f];
    }
    const double expected = kTrials / 3.0;
    for (uint32_t f : {7u, 8u, 9u}) {
        CHECK(counts[f] > expected * 0.9);
        CHECK(counts[f] < expected * 1.1);
    }
}
