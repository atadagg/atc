#include "doctest.h"
#include "golden_hash.hpp"
#include "scheduler.hpp"
#include "world.hpp"



// The byte-exactness gate for the production-economy work.
//
// A changed hash means behaviour changed. DO NOT update the constant to make a
// failing run pass -- that converts the one test that can catch a silent
// behavioural regression into a test that records whatever the code happens to
// do. The constant moves only when a change was *meant* to move it, in a
// commit that says so and that changes nothing else.
//
// It has moved once. The ownership commit replaced the profit rule: profits
// used to be paid in proportion to household liquidity and are now paid per
// share. That reprices every month close in every world including this one, so
// the old constant could not survive and preserving it was never the goal.
// Everything after that commit is gated against the new one.
TEST_CASE("golden run: seed 1, 120 months, byte-identical") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler sched(w, rng);
    for (int m = 0; m < 120; ++m) sched.run_month();

    CHECK(golden::run_hash(w) == 9841378507470298354ull);
}

TEST_CASE("golden run: same seed twice agrees") {
    auto run = []() {
        Rng rng(7);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler sched(w, rng);
        for (int m = 0; m < 60; ++m) sched.run_month();
        return golden::run_hash(w);
    };
    CHECK(run() == run());
}
