#include "doctest.h"
#include "ledger.hpp"
#include "params.hpp"
#include "world.hpp"

#include <stdexcept>

// A tiny world keeps these tests about the accounting, not the calibration.
static World tiny_world(Rng& rng) { return World(4, 2, rng); }

static double reason_total(const ledger::MonthFlows& f, ledger::Reason r) {
    return f.by_reason[ledger::reason_index(r)];
}

TEST_CASE("SummaryJournal totals movements by reason") {
    Rng rng(1);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;
    w.households[0].liquidity = 100.0;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 10.0,
                     ledger::Reason::Wage, &j);
    ledger::transfer(w, ledger::firm(0), ledger::household(1), 5.0,
                     ledger::Reason::Wage, &j);
    ledger::transfer(w, ledger::household(0), ledger::firm(1), 3.0,
                     ledger::Reason::Purchase, &j);

    CHECK(reason_total(j.flows(), ledger::Reason::Wage) == doctest::Approx(15.0));
    CHECK(reason_total(j.flows(), ledger::Reason::Purchase) == doctest::Approx(3.0));
    CHECK(reason_total(j.flows(), ledger::Reason::Profit) == doctest::Approx(0.0));
}

TEST_CASE("SummaryJournal totals movements by sector") {
    Rng rng(2);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;
    w.households[0].liquidity = 100.0;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 40.0,
                     ledger::Reason::Wage, &j);
    ledger::transfer(w, ledger::household(0), ledger::firm(1), 25.0,
                     ledger::Reason::Purchase, &j);

    CHECK(j.flows().household_in() == doctest::Approx(40.0));
    CHECK(j.flows().household_out() == doctest::Approx(25.0));
    CHECK(j.flows().firm_in() == doctest::Approx(25.0));
    CHECK(j.flows().firm_out() == doctest::Approx(40.0));
}

TEST_CASE("a movement within one sector counts on both sides and nets to zero") {
    Rng rng(3);
    World w = tiny_world(rng);
    w.households[0].liquidity = 50.0;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::household(0), ledger::household(1), 20.0,
                     ledger::Reason::Unspecified, &j);

    CHECK(j.flows().household_in() == doctest::Approx(20.0));
    CHECK(j.flows().household_out() == doctest::Approx(20.0));
    CHECK(j.flows().household_in() - j.flows().household_out() == doctest::Approx(0.0));
}

TEST_CASE("issuance is counted as money entering circulation") {
    Rng rng(4);
    World w = tiny_world(rng);
    REQUIRE(w.issuers.size() >= 1);

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::issuer(0), ledger::household(0), 500.0,
                     ledger::Reason::Issuance, &j);

    CHECK(j.flows().issued() == doctest::Approx(500.0));
    CHECK(j.flows().household_in() == doctest::Approx(500.0));
    CHECK(reason_total(j.flows(), ledger::Reason::Issuance) == doctest::Approx(500.0));
}

TEST_CASE("money returned to an issuer is netted out of the issued total") {
    Rng rng(5);
    World w = tiny_world(rng);
    w.households[0].liquidity = 100.0;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::issuer(0), ledger::household(0), 500.0,
                     ledger::Reason::Issuance, &j);
    ledger::transfer(w, ledger::household(0), ledger::issuer(0), 200.0,
                     ledger::Reason::Issuance, &j);

    CHECK(j.flows().issued() == doctest::Approx(300.0));
}

TEST_CASE("a rejected transfer is not counted") {
    Rng rng(6);
    World w = tiny_world(rng);
    w.households[0].liquidity = 10.0;

    ledger::SummaryJournal j;
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0), 99.0,
                                     ledger::Reason::Purchase, &j),
                    ledger::Overdraft);
    CHECK_THROWS_AS(ledger::transfer(w, ledger::household(0), ledger::firm(0), -1.0,
                                     ledger::Reason::Purchase, &j),
                    std::invalid_argument);

    CHECK(reason_total(j.flows(), ledger::Reason::Purchase) == doctest::Approx(0.0));
    CHECK(j.flows().household_out() == doctest::Approx(0.0));
}

TEST_CASE("begin_month resets the totals and stamps the month") {
    Rng rng(7);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;

    ledger::SummaryJournal j;
    j.begin_month(3);
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 10.0,
                     ledger::Reason::Wage, &j);
    CHECK(j.flows().month == 3);
    CHECK(reason_total(j.flows(), ledger::Reason::Wage) == doctest::Approx(10.0));

    j.begin_month(4);
    CHECK(j.flows().month == 4);
    CHECK(reason_total(j.flows(), ledger::Reason::Wage) == doctest::Approx(0.0));
    CHECK(j.flows().household_in() == doctest::Approx(0.0));
    CHECK(j.flows().issued() == doctest::Approx(0.0));
}

TEST_CASE("SummaryJournal forwards every entry to the journal it wraps") {
    Rng rng(8);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;

    ledger::VectorJournal inner;
    ledger::SummaryJournal j(&inner);
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 10.0,
                     ledger::Reason::Wage, &j);
    ledger::transfer(w, ledger::firm(0), ledger::household(1), 4.0,
                     ledger::Reason::Wage, &j);

    REQUIRE(inner.entries.size() == 2);
    CHECK(inner.entries[0].amount == doctest::Approx(10.0));
    CHECK(inner.entries[1].reason == ledger::Reason::Wage);
    CHECK(reason_total(j.flows(), ledger::Reason::Wage) == doctest::Approx(14.0));
}

// The check that earns its keep. Per-reason totals balance trivially, because
// `transfer` is the only writer. What is not trivial is that the flows the
// journal *saw* account for the change in the stock: a module that moved money
// without journaling it shows up here and nowhere else.
TEST_CASE("flows reconcile against the change in the household stock") {
    Rng rng(9);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;
    w.households[0].liquidity = 50.0;

    double before = 0.0;
    for (const auto& h : w.households) before += h.liquidity;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 30.0,
                     ledger::Reason::Wage, &j);
    ledger::transfer(w, ledger::household(0), ledger::firm(1), 12.0,
                     ledger::Reason::Purchase, &j);
    ledger::transfer(w, ledger::issuer(0), ledger::household(2), 7.0,
                     ledger::Reason::Issuance, &j);

    double after = 0.0;
    for (const auto& h : w.households) after += h.liquidity;

    CHECK(ledger::flow_discrepancy(j.flows(), before, after) == doctest::Approx(0.0));
    CHECK_NOTHROW(ledger::check_flows(j.flows(), before, after, w.books_balance()));
}

TEST_CASE("a movement that bypassed the journal is caught") {
    Rng rng(10);
    World w = tiny_world(rng);
    w.firms[0].liquidity = 100.0;

    double before = 0.0;
    for (const auto& h : w.households) before += h.liquidity;

    ledger::SummaryJournal j;
    ledger::transfer(w, ledger::firm(0), ledger::household(0), 30.0,
                     ledger::Reason::Wage, &j);
    // Exactly what `transfer` exists to prevent, done here on purpose: money
    // moved without the journal seeing it.
    w.households[1].liquidity += 500.0;

    double after = 0.0;
    for (const auto& h : w.households) after += h.liquidity;

    CHECK(ledger::flow_discrepancy(j.flows(), before, after) == doctest::Approx(-500.0));
    CHECK_THROWS_AS(ledger::check_flows(j.flows(), before, after, 1000.0),
                    ledger::FlowMismatch);
}

TEST_CASE("a machine-epsilon crumb is absorbed, not thrown") {
    Rng rng(11);
    World w = tiny_world(rng);

    ledger::SummaryJournal j;
    // Two magnitudes, one tolerance: a rounding remainder against a stock of
    // 100,000 is not a bug, and the reference run carries one every month.
    CHECK_NOTHROW(ledger::check_flows(j.flows(), 1000.0, 1000.0 + 3e-9, 100000.0));
}
