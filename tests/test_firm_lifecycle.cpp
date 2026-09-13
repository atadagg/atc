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

TEST_CASE("a dying firm collects what it is owed before paying its creditors") {
    Rng rng(2);
    World w(30, 4, rng);
    const double before = w.books_balance();

    // Firm 1 is dying. Firm 3 owes it 900 and can pay; firm 1 owes firm 2
    // 1000 and cannot.
    w.firms[3].liquidity = 950.0;
    w.firms[3].payables = 900.0;
    w.firms[3].payable_to[0] = 1;
    w.firms[3].payable_amount[0] = 900.0;

    w.firms[1].receivables = 900.0;
    w.firms[1].payables = 1000.0;
    w.firms[1].payable_to[0] = 2;
    w.firms[1].payable_amount[0] = 1000.0;
    w.firms[2].receivables = 1000.0;

    firm_lifecycle::run_deaths(w, rng);

    CHECK(w.firms[1].alive == false);

    // The debtor paid what it owed: no windfall, no cancelled debt.
    CHECK(w.firms[3].alive == true);
    CHECK(w.firms[3].liquidity == doctest::Approx(50.0));
    CHECK(w.firms[3].payables == doctest::Approx(0.0));
    CHECK(balance_sheet::of(w, 3).equity() == doctest::Approx(50.0));

    // The creditor recovered the 900 that was collected, and wrote off only
    // the uncollectable 100.
    CHECK(w.firms[2].liquidity == doctest::Approx(900.0));
    CHECK(w.firms[2].receivables == doctest::Approx(0.0));

    // Nothing was created or destroyed, and nothing overdrew.
    CHECK(w.books_balance() == doctest::Approx(before + 950.0));
}
