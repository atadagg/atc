#include "doctest.h"
#include "golden_hash.hpp"
#include "scheduler.hpp"
#include "world.hpp"



// The byte-exactness gate for the production-economy work.
//
// Every task in the supply-chain plan must leave this hash untouched. A
// changed hash means the generalisation altered behaviour it was supposed to
// preserve. DO NOT update the constant to make a failing run pass -- that
// converts the one test that can catch a silent behavioural regression into a
// test that records whatever the code happens to do.
TEST_CASE("golden run: seed 1, 120 months, byte-identical") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler sched(w, rng);
    for (int m = 0; m < 120; ++m) sched.run_month();

    CHECK(golden::run_hash(w) == 15776243924562223517ull);
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
