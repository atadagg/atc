#include "doctest.h"

#include "intervention.hpp"
#include "params.hpp"
#include "scheduler.hpp"
#include "world.hpp"

#include <set>

// A run is (seed + intervention log), not seed alone. That is what lets a
// parameter change mid-run without breaking determinism: replaying the same
// inputs replays the same history, including backwards through a scrub.

TEST_CASE("the parameter table is the single source of truth") {
    auto specs = intervention::param_specs();
    REQUIRE(!specs.empty());
    std::set<std::string_view> names;
    for (uint16_t i = 0; i < specs.size(); ++i) {
        CHECK(names.insert(specs[i].name).second);          // unique
        CHECK(intervention::find_param(specs[i].name) == i); // round-trips
    }
    CHECK(intervention::find_param("no such parameter") == intervention::kNoParam);
}

TEST_CASE("parameters read and write through the table") {
    Params p;
    const uint16_t productivity = intervention::find_param("productivity");
    REQUIRE(productivity != intervention::kNoParam);
    auto specs = intervention::param_specs();
    CHECK(specs[productivity].get(p) == doctest::Approx(3.0));
    specs[productivity].set(p, 5.5);
    CHECK(p.productivity == doctest::Approx(5.5));
}

TEST_CASE("an integer parameter survives the double round trip") {
    Params p;
    const uint16_t beta = intervention::find_param("unemployed_firm_visits");
    REQUIRE(beta != intervention::kNoParam);
    intervention::param_specs()[beta].set(p, 9.0);
    CHECK(p.unemployed_firm_visits == 9u);
}

TEST_CASE("the log returns only what is due, in sequence order") {
    intervention::Log log;
    log.set_param(5, "productivity", 4.0);
    log.set_param(3, "min_wage", 2.0);
    log.set_param(5, "min_wage", 7.0);

    CHECK(log.due(4).empty());

    auto at3 = log.due(3);
    REQUIRE(at3.size() == 1);
    CHECK(at3[0].value == doctest::Approx(2.0));

    auto at5 = log.due(5);
    REQUIRE(at5.size() == 2);
    // Sequence, not insertion index: both were added after the month-3 entry,
    // but they must apply in the order they were recorded relative to each other.
    CHECK(at5[0].seq < at5[1].seq);
    CHECK(at5[0].value == doctest::Approx(4.0));
    CHECK(at5[1].value == doctest::Approx(7.0));
}

TEST_CASE("applying a parameter change updates the world") {
    Rng rng(1);
    World w(10, 2, rng);
    intervention::Log log;
    log.set_param(0, "productivity", 8.0);
    intervention::apply(w, log.due(0)[0]);
    CHECK(w.params.productivity == doctest::Approx(8.0));
}

TEST_CASE("issuing money to every household raises circulation, books stay flat") {
    Rng rng(1);
    World w(10, 2, rng);
    const double circulation = w.money_in_circulation();
    const double books = w.books_balance();

    intervention::Log log;
    log.issue_to_all_households(0, /*issuer=*/0, /*each=*/25.0);
    intervention::apply(w, log.due(0)[0]);

    CHECK(w.money_in_circulation() == doctest::Approx(circulation + 250.0));
    CHECK(w.books_balance() == doctest::Approx(books));
    CHECK(w.issuers[0] == doctest::Approx(-250.0));
}

TEST_CASE("an intervention takes effect at its month, not before") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    intervention::Log log;
    log.set_param(3, "productivity", 9.0);

    Scheduler s(w, rng);
    s.set_interventions(&log);

    // Months 0, 1 and 2 run untouched.
    for (int m = 0; m < 3; ++m) {
        CHECK(w.params.productivity == doctest::Approx(3.0));
        s.run_month();
    }
    CHECK(w.params.productivity == doctest::Approx(3.0));

    s.run_month();  // month 3 opens; the change is applied before any agent acts
    CHECK(w.params.productivity == doctest::Approx(9.0));
}

TEST_CASE("interventions apply before any agent acts in that month") {
    // A productivity change at month 0 must be visible in that month's own
    // production, not only from month 1 onward.
    Rng a_rng(4);
    World a(50, 4, a_rng);
    intervention::Log log;
    log.set_param(0, "productivity", 30.0);
    Scheduler sa(a, a_rng);
    sa.set_interventions(&log);
    sa.run_month();

    Rng b_rng(4);
    World b(50, 4, b_rng);
    b.params.productivity = 30.0;  // set directly, no log
    Scheduler sb(b, b_rng);
    sb.run_month();

    for (size_t i = 0; i < a.firms.size(); ++i) {
        CHECK(a.firms[i].inventory == doctest::Approx(b.firms[i].inventory));
    }
}
