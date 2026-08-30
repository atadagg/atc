#include "doctest.h"

#include "intervention.hpp"
#include "model_steering.hpp"
#include "world.hpp"

TEST_CASE("a steering row exists for every behavioural parameter") {
    Rng rng(1);
    World w(10, 2, rng);
    auto rows = steering_rows(w);
    CHECK(rows.size() == intervention::param_specs().size());
}

TEST_CASE("steering rows carry the world's current values, not the defaults") {
    Rng rng(1);
    World w(10, 2, rng);
    w.params.productivity = 6.25;
    bool found = false;
    for (const auto& r : steering_rows(w)) {
        if (r.name == "productivity") {
            CHECK(r.value == doctest::Approx(6.25));
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("every steering row has a usable range containing its default") {
    Rng rng(1);
    World w(10, 2, rng);
    for (const auto& r : steering_rows(w)) {
        CAPTURE(r.name);
        CHECK(r.lo < r.hi);
        CHECK(r.value >= r.lo);
        CHECK(r.value <= r.hi);
    }
}

TEST_CASE("a logged intervention describes itself for the audit trail") {
    intervention::Log log;
    log.set_param(12, "min_wage", 4.5);
    log.issue_to_all_households(20, 0, 50.0);
    log.issue(30, 0, 7, 125.0);

    auto e = log.all();
    REQUIRE(e.size() == 3);
    CHECK(describe(e[0]) == "month 12: set min_wage = 4.5");
    CHECK(describe(e[1]) == "month 20: issuer 0 gives every household 50");
    CHECK(describe(e[2]) == "month 30: issuer 0 gives household 7 125");
}

TEST_CASE("a parameter outside its range is rejected") {
    intervention::Log log;
    // A probability above 1 is meaningless, not merely unusual.
    CHECK_THROWS_AS(log.set_param(0, "price_change_prob", 1.5), std::out_of_range);
    CHECK_THROWS_AS(log.set_param(0, "price_change_prob", -0.1), std::out_of_range);
    log.set_param(0, "price_change_prob", 1.0);  // the boundary is allowed
    CHECK(log.all().size() == 1);
}
