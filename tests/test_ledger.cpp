#include "doctest.h"
#include "ledger.hpp"
#include "params.hpp"
#include "world.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

// A tiny world keeps these tests about the arithmetic, not the calibration.
static World tiny_world(Rng& rng) { return World(4, 2, rng); }

TEST_CASE("transfer debits the source and credits the sink") {
    Rng rng(1);
    World w = tiny_world(rng);
    w.households[0].liquidity = 100.0;
    w.firms[1].liquidity = 5.0;

    ledger::transfer(w, ledger::household(0), ledger::firm(1), 30.0);

    CHECK(w.households[0].liquidity == doctest::Approx(70.0));
    CHECK(w.firms[1].liquidity == doctest::Approx(35.0));
}

TEST_CASE("transfer conserves total money") {
    Rng rng(2);
    World w = tiny_world(rng);
    double before = w.total_money();
    ledger::transfer(w, ledger::firm(0), ledger::household(3), 17.5);
    CHECK(w.total_money() == doctest::Approx(before));
}

TEST_CASE("transfer works between two accounts of the same kind") {
    Rng rng(3);
    World w = tiny_world(rng);
    w.households[0].liquidity = 10.0;
    w.households[1].liquidity = 10.0;
    ledger::transfer(w, ledger::household(0), ledger::household(1), 4.0);
    CHECK(w.households[0].liquidity == doctest::Approx(6.0));
    CHECK(w.households[1].liquidity == doctest::Approx(14.0));
}

TEST_CASE("a transfer to and from the same account is a no-op") {
    Rng rng(4);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 50.0;
    ledger::transfer(w, ledger::firm(0), ledger::firm(0), 12.0);
    CHECK(w.firms[0].liquidity == doctest::Approx(50.0));
}

TEST_CASE("a zero transfer changes nothing") {
    Rng rng(5);
    World w = tiny_world(rng);
    double h = w.households[0].liquidity;
    double f = w.firms[0].liquidity;
    ledger::transfer(w, ledger::household(0), ledger::firm(0), 0.0);
    CHECK(w.households[0].liquidity == h);
    CHECK(w.firms[0].liquidity == f);
}

TEST_CASE("a negative amount is a caller bug, not a reversed direction") {
    Rng rng(6);
    World w = tiny_world(rng);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0), -1.0),
                    std::invalid_argument);
}

TEST_CASE("a non-finite amount is rejected") {
    Rng rng(7);
    World w = tiny_world(rng);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0),
                                     std::numeric_limits<double>::quiet_NaN()),
                    std::invalid_argument);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0),
                                     std::numeric_limits<double>::infinity()),
                    std::invalid_argument);
}

TEST_CASE("an out-of-range account index is rejected") {
    Rng rng(8);
    World w = tiny_world(rng);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(99), ledger::firm(0), 1.0),
                    std::out_of_range);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::firm(0), ledger::firm(99), 1.0),
                    std::out_of_range);
}
