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
    w.firms[0].liquidity = 50.0;
    double before = w.books_balance();
    ledger::transfer(w, ledger::firm(0), ledger::household(3), 17.5);
    CHECK(w.books_balance() == doctest::Approx(before));
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

// --- journaling -----------------------------------------------------------
//
// The chokepoint only guarantees conservation. Journaling is what turns
// balances into a book: every movement recorded with its source, sink,
// reason and place in time. It is opt-in because the reference run makes
// millions of transfers.

TEST_CASE("a transfer without a journal records nothing") {
    Rng rng(10);
    World w = tiny_world(rng);
    ledger::VectorJournal j;
    ledger::transfer(w, ledger::household(0), ledger::firm(0), 1.0);
    CHECK(j.entries.empty());
}

TEST_CASE("a journaled transfer records source, sink, amount and reason") {
    Rng rng(11);
    World w = tiny_world(rng);
    ledger::VectorJournal j;
    ledger::transfer(w, ledger::household(2), ledger::firm(1), 7.5,
                     ledger::Reason::Purchase, &j);

    REQUIRE(j.entries.size() == 1);
    const ledger::Entry& e = j.entries[0];
    CHECK(e.from.kind == ledger::Kind::Household);
    CHECK(e.from.index == 2);
    CHECK(e.to.kind == ledger::Kind::Firm);
    CHECK(e.to.index == 1);
    CHECK(e.amount == doctest::Approx(7.5));
    CHECK(e.reason == ledger::Reason::Purchase);
}

TEST_CASE("entries carry the time the journal was last told about") {
    Rng rng(12);
    World w = tiny_world(rng);
    ledger::VectorJournal j;

    j.set_time(4, 9);
    ledger::transfer(w, ledger::household(0), ledger::firm(0), 1.0,
                     ledger::Reason::Purchase, &j);
    j.set_time(4, ledger::kMonthClose);
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 1.0,
                     ledger::Reason::Wage, &j);

    REQUIRE(j.entries.size() == 2);
    CHECK(j.entries[0].month == 4);
    CHECK(j.entries[0].day == 9);
    CHECK(j.entries[1].month == 4);
    CHECK(j.entries[1].day == ledger::kMonthClose);
}

TEST_CASE("seq gives every entry a total order across the whole run") {
    Rng rng(13);
    World w = tiny_world(rng);
    ledger::VectorJournal j;
    for (int i = 0; i < 5; ++i) {
        j.set_time(i, 0);
        ledger::transfer(w, ledger::household(0), ledger::firm(0), 0.5,
                         ledger::Reason::Purchase, &j);
    }
    REQUIRE(j.entries.size() == 5);
    for (size_t i = 1; i < j.entries.size(); ++i) {
        CHECK(j.entries[i].seq > j.entries[i - 1].seq);
    }
}

TEST_CASE("a rejected transfer records nothing") {
    Rng rng(14);
    World w = tiny_world(rng);
    ledger::VectorJournal j;
    CHECK_THROWS(ledger::transfer(w, ledger::household(0), ledger::firm(0), -1.0,
                                  ledger::Reason::Purchase, &j));
    CHECK_THROWS(ledger::transfer(w, ledger::household(99), ledger::firm(0), 1.0,
                                  ledger::Reason::Purchase, &j));
    CHECK(j.entries.empty());
}

// --- issuers and solvency -------------------------------------------------
//
// The engine's rule is that every event balances, NOT that the total never
// changes. Money is created by issuance -- a balanced movement out of an
// account that is allowed to go negative -- and an issuer's negative balance
// is the money it has issued, not a deficit.

TEST_CASE("only issuer kinds may issue") {
    static_assert(!ledger::may_issue(ledger::Kind::Firm));
    static_assert(!ledger::may_issue(ledger::Kind::Household));
    static_assert(ledger::may_issue(ledger::Kind::Issuer));
    CHECK(true);
}

TEST_CASE("a non-issuer cannot be pushed below zero") {
    Rng rng(20);
    World w = tiny_world(rng);
    w.households[0].liquidity = 10.0;
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0), 10.5),
                    ledger::Overdraft);
    // The rejected transfer moved nothing.
    CHECK(w.households[0].liquidity == doctest::Approx(10.0));
}

TEST_CASE("a non-issuer may be emptied exactly") {
    Rng rng(21);
    World w = tiny_world(rng);
    w.households[0].liquidity = 10.0;
    ledger::transfer(w, ledger::household(0), ledger::firm(0), 10.0);
    CHECK(w.households[0].liquidity == 0.0);
}

TEST_CASE("an issuer may go negative -- that is the money it has issued") {
    Rng rng(22);
    World w = tiny_world(rng);
    w.issuers.resize(1, 0.0);

    ledger::transfer(w, ledger::issuer(0), ledger::household(0), 500.0);

    CHECK(w.issuers[0] == doctest::Approx(-500.0));
}

TEST_CASE("issuance raises circulation but leaves the books balanced") {
    Rng rng(23);
    World w = tiny_world(rng);
    w.issuers.resize(1, 0.0);

    const double circulation_before = w.money_in_circulation();
    const double books_before = w.books_balance();

    ledger::transfer(w, ledger::issuer(0), ledger::household(1), 250.0);

    CHECK(w.money_in_circulation() == doctest::Approx(circulation_before + 250.0));
    CHECK(w.books_balance() == doctest::Approx(books_before));
}

TEST_CASE("circulation excludes issuers, books include them") {
    Rng rng(24);
    World w = tiny_world(rng);
    w.issuers.resize(1, -400.0);
    CHECK(w.books_balance() ==
          doctest::Approx(w.money_in_circulation() - 400.0));
}
