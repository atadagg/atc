#include "doctest.h"
#include "rng.hpp"
#include "agents.hpp"
#include "balance_sheet.hpp"
#include "firm_lifecycle.hpp"
#include "network.hpp"
#include "world.hpp"

TEST_CASE("an insolvent firm dies and releases its workers") {
    Rng rng(2);
    World w(30, 4, rng);
    network::hire(w, 0, 1);
    network::hire(w, 1, 1);
    w.firms[1].payables = 500.0;   // hopelessly underwater

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[1].employees.empty());
    CHECK(w.households[0].employed() == false);
    CHECK(w.households[1].employed() == false);
}

TEST_CASE("a dead firm's unpaid bill is written off by its supplier") {
    Rng rng(2);
    World w(30, 4, rng);

    w.firms[1].payables = 500.0;
    w.firms[1].payable_to[0] = 2;
    w.firms[1].payable_amount[0] = 500.0;
    w.firms[2].receivables = 500.0;

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[2].receivables == doctest::Approx(0.0));
}

TEST_CASE("a write-off can push an exposed supplier under: the cascade") {
    Rng rng(2);
    World w(30, 4, rng);

    // Firm 1 is doomed and owes firm 2 everything firm 2 is worth.
    w.firms[1].payables = 500.0;
    w.firms[1].payable_to[0] = 2;
    w.firms[1].payable_amount[0] = 500.0;

    // Firm 2 is solvent ONLY because of that receivable.
    w.firms[2].liquidity = 10.0;
    w.firms[2].inventory = 0.0;
    w.firms[2].receivables = 500.0;
    w.firms[2].payables = 400.0;
    REQUIRE(balance_sheet::of(w, 2).insolvent() == false);

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);
    CHECK(w.firms[2].alive == false);  // killed by the write-off, not by its own trading
}

TEST_CASE("liquidation conserves money") {
    Rng rng(2);
    World w(30, 4, rng);
    double before = w.books_balance();
    w.firms[1].liquidity = 100.0;
    w.firms[1].payables = 500.0;

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.books_balance() == doctest::Approx(before + 100.0));
}

TEST_CASE("in a world with no debt nothing dies") {
    Rng rng(2);
    World w(30, 4, rng);
    firm_lifecycle::run_deaths(w, rng);
    for (const Firm& f : w.firms) CHECK(f.alive == true);
}
