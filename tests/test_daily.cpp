#include <numeric>
#include <vector>

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
          doctest::Approx(w.params.productivity * static_cast<double>(n)));
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

    double money_before = w.books_balance();
    daily::shop(w, 0, rng);

    CHECK(w.households[0].liquidity == doctest::Approx(98.0));
    CHECK(w.books_balance() == doctest::Approx(money_before));

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

    double before = w.books_balance();
    daily::run_day(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before));
}

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
