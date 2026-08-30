#include "doctest.h"

#include "daily.hpp"
#include "month_close.hpp"
#include "params.hpp"
#include "world.hpp"

// Behavioural parameters are runtime state on the World, not compile-time
// constants, so a run can change them and a scenario can vary them. Structural
// parameters -- population sizes, kShopsPerHousehold, kDaysPerMonth -- stay
// `constexpr`, because they define memory layout.

TEST_CASE("a fresh world carries the published calibration") {
    Rng rng(1);
    World w(10, 2, rng);
    CHECK(w.params.productivity == doctest::Approx(3.0));       // lambda
    CHECK(w.params.wage_adjust_max == doctest::Approx(0.019));  // delta
    CHECK(w.params.price_change_prob == doctest::Approx(0.75)); // theta
    CHECK(w.params.buffer_factor == doctest::Approx(0.1));      // chi
    CHECK(w.params.unemployed_firm_visits == 5);                // beta
}

TEST_CASE("parameters are per-world, not global") {
    Rng rng(1);
    World a(10, 2, rng);
    World b(10, 2, rng);
    a.params.productivity = 99.0;
    CHECK(b.params.productivity == doctest::Approx(3.0));
}

TEST_CASE("productivity drives how much a firm produces") {
    Rng rng(1);
    World w(10, 2, rng);
    for (auto& f : w.firms) f.inventory = 0.0;
    w.params.productivity = 7.0;
    daily::produce(w);
    for (const auto& f : w.firms) {
        CHECK(f.inventory ==
              doctest::Approx(7.0 * static_cast<double>(f.employees.size())));
    }
}

TEST_CASE("the wage floor is a parameter, not a constant") {
    Rng rng(1);
    World w(10, 2, rng);
    w.params.min_wage = 25.0;
    Firm& f = w.firms[0];
    REQUIRE(!f.employees.empty());
    f.wage = 40.0;
    f.liquidity = 0.0;  // cannot afford any payroll at all
    month_close::pay_wages(w);
    // The emergency cut is bounded below by both the max-cut rule and the
    // floor; with a 25.0 floor it cannot fall to the 50%-cut value of 20.0.
    CHECK(f.wage == doctest::Approx(25.0));
}

TEST_CASE("structural parameters stay compile-time constants") {
    // These are array bounds and layout facts, not knobs. If one of them ever
    // becomes runtime state, `Household::shops` stops being a fixed array.
    static_assert(params::kShopsPerHousehold == 7);
    static_assert(params::kDaysPerMonth == 21);
    static_assert(params::kNumHouseholds == 1000);
    static_assert(params::kNumFirms == 100);
    CHECK(true);
}
