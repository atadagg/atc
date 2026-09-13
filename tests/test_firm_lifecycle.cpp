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

TEST_CASE("entry is off by default, so existing runs are untouched") {
    Rng rng(4);
    World w(30, 4, rng);
    CHECK(w.params.firm_entry_prob == doctest::Approx(0.0));

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.firms.size() == before);
}

TEST_CASE("a profitable good with a dead producer attracts entry") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.params.firm_entry_margin = 1.2;

    w.markets[0].reference_price = 100.0;   // far above any plausible cost
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;               // a vacancy in the market

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);

    CHECK(w.firms.size() > before);
    const Firm& founded = w.firms.back();
    CHECK(founded.alive == true);
    CHECK(founded.good == 0u);
    CHECK(founded.liquidity == doctest::Approx(w.params.firm_entry_capital));
}

TEST_CASE("founding conserves money") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.markets[0].reference_price = 100.0;
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;

    double before = w.books_balance();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("no entry when the price does not cover cost") {
    Rng rng(4);
    World w(30, 4, rng);
    w.params.firm_entry_prob = 1.0;
    w.markets[0].reference_price = 0.001;
    w.markets[0].has_reference = true;
    w.firms[0].alive = false;

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);
    CHECK(w.firms.size() == before);
}

TEST_CASE("a never-produced good with a reference price can attract a first producer") {
    Rng rng(4);
    World w(30, 4, rng);  // every firm makes good 0

    // A second good nobody has ever been assigned to -- the count of firms
    // ever assigned to it is 0, and 0 living producers is not scarce under
    // the old (dropped) "fewer than were ever assigned" rule, which would
    // have refused this forever. Under the cap rule, 0 living is simply below
    // the cap.
    Good g;
    g.inputs.fill(NO_GOOD);
    g.input_qty.fill(0.0);
    g.labour_per_unit = 1.0;
    w.goods.push_back(g);
    w.markets.resize(w.goods.size());
    w.markets[1].reference_price = 100.0;
    w.markets[1].has_reference = true;

    w.params.firm_entry_prob = 1.0;

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);

    CHECK(w.firms.size() > before);
    CHECK(w.firms.back().good == 1u);
}

TEST_CASE("a profitable sector with no deaths grows to the cap and then stops") {
    Rng rng(4);
    World w(30, 4, rng);  // 4 firms, all alive, all making good 0
    w.params.firm_entry_prob = 1.0;
    w.params.firm_entry_max_per_good = 6.0;
    w.params.firm_entry_max_per_month = 10.0;  // not the constraint under test
    w.markets[0].reference_price = 100.0;
    w.markets[0].has_reference = true;

    // Nobody dies; entry alone must grow the sector from 4 living producers
    // to the cap of 6, then stop drawing new ones even though the good stays
    // just as profitable every month after.
    for (int month = 0; month < 10; ++month) {
        firm_lifecycle::run_entry(w, rng);
    }

    uint32_t living = 0;
    for (const Firm& f : w.firms) {
        if (f.alive) ++living;
    }
    CHECK(living == 6u);
}

TEST_CASE("entry does not exceed the per-month cap across goods") {
    Rng rng(4);
    World w(30, 6, rng);
    w.goods.resize(3);
    for (Good& g : w.goods) {
        g.inputs.fill(NO_GOOD);
        g.input_qty.fill(0.0);
        g.labour_per_unit = 1.0;
    }
    w.markets.resize(w.goods.size());
    for (uint32_t g = 0; g < 3; ++g) {
        w.markets[g].reference_price = 100.0;
        w.markets[g].has_reference = true;
    }
    // One firm per good: every good is well below its per-good cap, so all
    // three would attract an entrant this month if nothing else stopped them.
    for (uint32_t i = 0; i < 6; ++i) w.firms[i].good = i % 3;

    w.params.firm_entry_prob = 1.0;
    w.params.firm_entry_max_per_good = 8.0;
    w.params.firm_entry_max_per_month = 2.0;

    std::size_t before = w.firms.size();
    firm_lifecycle::run_entry(w, rng);

    CHECK(w.firms.size() == before + 2);
}
