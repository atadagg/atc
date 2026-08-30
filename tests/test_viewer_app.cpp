#include "doctest.h"

#include "app.hpp"
#include "model_steering.hpp"

// App holds no graphics types, so the steering flow -- the thing a slider
// actually does -- is testable without a window. The panels remain untested;
// they only draw.

TEST_CASE("a steered parameter takes effect at the next month, not immediately") {
    App app(1);
    app.paused = false;
    app.advance();  // month 0 runs

    const int scheduled = app.sim.month();
    app.log.set_param(scheduled, "productivity", 6.0);

    // Scheduled, not applied: the world is still using the old value.
    CHECK(app.sim.world().params.productivity == doctest::Approx(3.0));

    app.advance();
    CHECK(app.sim.world().params.productivity == doctest::Approx(6.0));
}

TEST_CASE("steering survives a scrub back through the change") {
    App app(1);
    app.paused = false;
    app.months_per_frame = 10;
    app.advance();  // months 0-9

    app.log.set_param(app.sim.month(), "min_wage", 15.0);
    app.advance();  // months 10-19, with the change applied at month 10
    REQUIRE(app.sim.world().params.min_wage == doctest::Approx(15.0));

    const double wage = app.sim.world().firms[0].wage;
    const double liquidity = app.sim.world().households[0].liquidity;

    app.seek_to(5);
    CHECK(app.sim.world().params.min_wage == doctest::Approx(1.0));

    app.seek_to(20);
    CHECK(app.sim.world().params.min_wage == doctest::Approx(15.0));
    CHECK(app.sim.world().firms[0].wage == wage);
    CHECK(app.sim.world().households[0].liquidity == liquidity);
}

TEST_CASE("issuing money from the UI expands the supply and balances the books") {
    App app(1);
    app.paused = false;
    app.advance();

    const double books = app.sim.world().books_balance();
    app.log.issue_to_all_households(app.sim.month(), 0, 20.0);
    app.advance();

    CHECK(app.sim.world().money_in_circulation() ==
          doctest::Approx(100000.0 + 20.0 * params::kNumHouseholds));
    CHECK(app.sim.world().books_balance() == doctest::Approx(books));
}

TEST_CASE("seeking never extends how far the run has reached") {
    App app(1);
    app.paused = false;
    app.months_per_frame = 12;
    app.advance();
    CHECK(app.max_month == 12);
    app.seek_to(3);
    CHECK(app.max_month == 12);
}
