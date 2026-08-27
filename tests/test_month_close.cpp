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
