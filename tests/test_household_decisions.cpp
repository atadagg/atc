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
    double expected = std::pow(real_wealth, w.params.consumption_exponent);
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
