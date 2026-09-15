#include "doctest.h"
#include "scheduler.hpp"
#include "month_close.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"
#include "firm_decisions.hpp"
#include "balance_sheet.hpp"

TEST_CASE("wages move money from firms to their employees") {
    Rng rng(1);
    World w(10, 2, rng);
    for (auto& f : w.firms) { f.wage = 10.0; f.liquidity = 1000.0; }
    for (auto& h : w.households) h.liquidity = 0.0;

    double before = w.books_balance();
    month_close::pay_wages(w);

    CHECK(w.books_balance() == doctest::Approx(before));
    for (const auto& h : w.households) {
        if (h.employed()) CHECK(h.liquidity == doctest::Approx(10.0));
    }
}

TEST_CASE("an unaffordable wage bill triggers a wage cut rather than bankruptcy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.wage = 1000.0;
    f.liquidity = 10.0;  // nowhere near the wage bill
    size_t n = f.employees.size();
    REQUIRE(n > 0);

    double before = w.books_balance();
    month_close::pay_wages(w);

    CHECK(f.wage < 1000.0);
    CHECK(f.liquidity >= -1e-9);
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("profits are distributed per share, not per unit of cash") {
    Rng rng(1);
    World w(2, 1, rng);
    w.firms[0].wage = 0.0;
    w.firms[0].liquidity = 300.0;
    // The household holding a third of the firm is the RICHER of the two, so
    // a liquidity-weighted rule and a per-share rule disagree in an obvious
    // direction: under the old rule household 1 took 200 of the 300.
    w.households[0].liquidity = 100.0;
    w.households[1].liquidity = 200.0;
    w.firms[0].holders.fill(NO_HOUSEHOLD);
    w.firms[0].holder_shares.fill(0.0);
    w.firms[0].holders[0] = 0;
    w.firms[0].holder_shares[0] = 200.0 / 3.0;
    w.firms[0].holders[1] = 1;
    w.firms[0].holder_shares[1] = 100.0 / 3.0;

    double before = w.books_balance();
    month_close::distribute_profits(w);

    // Buffer is chi * wage bill = 0, so all 300 is distributed 2:1.
    CHECK(w.households[0].liquidity == doctest::Approx(300.0));
    CHECK(w.households[1].liquidity == doctest::Approx(300.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(0.0));
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("profit distribution across multiple firms does not create money") {
    // Regression test for a previously-fixed bug: distribute_profits used to
    // weight each household's share by its liquidity, recomputed against a
    // total captured once before the whole loop, while the numerator kept
    // growing as earlier firms' profits were credited. From the second firm
    // onward the ratios summed to more than 1 and each subsequent firm paid
    // out more than its own profit -- pure money creation.
    //
    // Shares cannot reproduce that bug by construction, because they do not
    // move when money does. The test stays because the property it guards --
    // total distributed equals total profit, whatever the order -- is the one
    // that matters, and it now also pins down that a household's payout is
    // independent of how rich it is.
    Rng rng(1);
    World w(3, 3, rng);

    w.households[0].liquidity = 100.0;
    w.households[1].liquidity = 200.0;
    w.households[2].liquidity = 300.0;

    // Every firm is held 50/50 by households 0 and 1. Household 2 is the
    // richest and owns nothing, so under the old rule it took half the pot
    // and under this one it takes none.
    for (auto& f : w.firms) {
        f.wage = 0.0;  // zero buffer, so liquidity is all distributable
        f.holders.fill(NO_HOUSEHOLD);
        f.holder_shares.fill(0.0);
        f.holders[0] = 0;
        f.holder_shares[0] = 50.0;
        f.holders[1] = 1;
        f.holder_shares[1] = 50.0;
    }
    w.firms[0].liquidity = 30.0;
    w.firms[1].liquidity = 60.0;
    w.firms[2].liquidity = 90.0;

    double before = w.books_balance();
    CHECK(before == doctest::Approx(100.0 + 200.0 + 300.0 + 30.0 + 60.0 + 90.0));

    month_close::distribute_profits(w);

    // Total profit distributed is 30+60+90=180, split evenly by shares.
    CHECK(w.households[0].liquidity == doctest::Approx(100.0 + 90.0));
    CHECK(w.households[1].liquidity == doctest::Approx(200.0 + 90.0));
    CHECK(w.households[2].liquidity == doctest::Approx(300.0));

    for (const auto& f : w.firms) CHECK(f.liquidity == doctest::Approx(0.0));
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("a firm retains its buffer before distributing profit") {
    Rng rng(1);
    World w(4, 1, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    f.liquidity = 1000.0;
    double wage_bill = f.wage * static_cast<double>(f.employees.size());
    double expected_buffer = w.params.buffer_factor * wage_bill;

    month_close::distribute_profits(w);
    CHECK(f.liquidity == doctest::Approx(expected_buffer));
}

TEST_CASE("a firm below its buffer distributes nothing") {
    Rng rng(1);
    World w(4, 1, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    f.liquidity = 1.0;
    double before = w.books_balance();

    month_close::distribute_profits(w);

    CHECK(f.liquidity == doctest::Approx(1.0));
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("reservation wage rises to match a higher income") {
    Rng rng(1);
    World w(4, 1, rng);
    w.firms[0].wage = 80.0;
    w.households[0].reservation_wage = 50.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(80.0));
}

TEST_CASE("reservation wage does not fall when the employer cuts pay") {
    Rng rng(1);
    World w(4, 1, rng);
    w.firms[0].wage = 20.0;
    w.households[0].reservation_wage = 50.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(50.0));
}

TEST_CASE("an unemployed household cuts its reservation wage by ten percent") {
    Rng rng(1);
    World w(4, 2, rng);
    network::fire(w, 0);
    w.households[0].reservation_wage = 100.0;
    month_close::update_reservation_wages(w);
    CHECK(w.households[0].reservation_wage == doctest::Approx(90.0));
}

TEST_CASE("pending fires are applied and cleared") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(!f.employees.empty());
    uint32_t victim = f.employees.front();
    f.pending_fire = victim;

    month_close::apply_pending_fires(w);

    CHECK(w.households[victim].employer == NO_FIRM);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("demand rolls over and resets") {
    Rng rng(1);
    World w(10, 2, rng);
    w.firms[0].demand_this_month = 42.0;
    w.firms[0].demand_last_month = 7.0;
    month_close::roll_demand(w);
    CHECK(w.firms[0].demand_last_month == doctest::Approx(42.0));
    CHECK(w.firms[0].demand_this_month == doctest::Approx(0.0));
}

TEST_CASE("a full month close conserves money") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (auto& f : w.firms) { f.liquidity = 500.0; f.wage = 30.0; }
    double before = w.books_balance();
    month_close::run(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("an emergency wage cut is bounded and never reaches zero") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    f.liquidity = 0.0;  // sold nothing all month: the worst possible case
    REQUIRE(!f.employees.empty());

    month_close::pay_wages(w);

    // Unbounded, the cut lands exactly on zero, and zero is absorbing under
    // the multiplicative updates in firm_decisions::adjust_wage.
    CHECK(f.wage > 0.0);
    CHECK(f.wage >= w.params.min_wage);
    CHECK(f.wage >= 100.0 * (1.0 - w.params.max_emergency_wage_cut));
}

TEST_CASE("repeated emergency cuts cannot drive a wage to zero") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.wage = 100.0;
    REQUIRE(!f.employees.empty());

    for (int month = 0; month < 200; ++month) {
        f.liquidity = 0.0;  // never sells anything, ever
        month_close::pay_wages(w);
    }

    CHECK(f.wage >= w.params.min_wage);
    // A zero wage collapses the price band to [0, 0], which permanently
    // disables the price-raise gate in firm_decisions::adjust_price.
    CHECK(firm_decisions::marginal_cost(f, w.params) > 0.0);
}

// --- distribution tails ---------------------------------------------------
//
// A firm splitting a pot among N recipients cannot pay N separately computed
// shares: they need not sum to the pot, and the firm is left holding a
// machine-epsilon residue -- 16,953 of them per reference run, measured. The
// last recipient takes the exact remaining balance instead.

TEST_CASE("a firm that cannot afford its payroll ends at exactly zero") {
    Rng rng(1);
    World w(20, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(f.employees.size() >= 2);
    f.wage = 1000.0;          // far beyond what it holds
    f.liquidity = 1.0 / 3.0;  // a pot that does not divide evenly
    month_close::pay_wages(w);
    CHECK(f.liquidity == 0.0);
}

TEST_CASE("a firm that can afford its payroll keeps exactly the difference") {
    Rng rng(1);
    World w(20, 2, rng);
    Firm& f = w.firms[0];
    const double headcount = static_cast<double>(f.employees.size());
    REQUIRE(headcount >= 2);
    f.wage = 1.0 / 7.0;
    f.liquidity = 100.0;
    const double expected = 100.0 - f.wage * headcount;
    month_close::pay_wages(w);
    CHECK(f.liquidity == doctest::Approx(expected));
    CHECK(f.liquidity >= 0.0);
}

TEST_CASE("an empty firm distributes its entire balance and ends at zero") {
    Rng rng(1);
    World w(20, 2, rng);
    Firm& f = w.firms[0];
    // No employees means a zero wage bill, hence a zero retained buffer,
    // hence a full payout -- the case where shares can overshoot the pot.
    while (!f.employees.empty()) network::fire(w, f.employees.front());
    REQUIRE(f.employees.empty());
    f.liquidity = 1.0 / 3.0;
    month_close::distribute_profits(w);
    CHECK(f.liquidity == 0.0);
}

TEST_CASE("a staffed firm retains exactly its buffer") {
    Rng rng(1);
    World w(20, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(!f.employees.empty());
    f.wage = 1.0 / 7.0;
    f.liquidity = 500.0;
    const double buffer =
        w.params.buffer_factor * f.wage * static_cast<double>(f.employees.size());
    month_close::distribute_profits(w);
    CHECK(f.liquidity == doctest::Approx(buffer));
    CHECK(f.liquidity >= 0.0);
}

// Regression: seeds 5 and 7 aborted mid-run because the proportional shares in
// distribute_profits can overshoot the pot by a rounding crumb, leaving the
// firm fractionally BELOW its buffer and the last recipient's remainder
// negative. The tail fix absorbed overshoot in one direction only.
//
// Found by a 20-seed ensemble. Seeds 1-4 are clean, which is why 7000 months
// of seed 1 never showed it -- one seed is not a sample.
TEST_CASE("a long run does not abort on a negative distribution remainder"
          * doctest::timeout(120)) {
    for (uint64_t seed : {uint64_t{5}, uint64_t{7}}) {
        CAPTURE(seed);
        Rng rng(seed);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler s(w, rng);
        REQUIRE_NOTHROW([&] {
            for (int m = 0; m < 400; ++m) s.run_month();
        }());
    }
}

// Regression: distribute_profits computed `profit = liquidity - buffer` with
// no reference to what the firm owed, so a firm holding cash earmarked for a
// supplier bill paid that cash out as profit and was insolvent by the time
// firm_lifecycle::run_deaths looked at it -- an insolvency the distribution
// itself created. Safe by accident before trade credit existed (payables were
// always zero, so equity was a sum of non-negative terms; see
// balance_sheet.hpp), fatal the moment firms bought from firms: it emptied
// the middle of a supply chain within a dozen months.
TEST_CASE("a firm does not distribute cash it owes its suppliers") {
    Rng rng(1);
    World w(10, 2, rng);
    for (auto& h : w.households) h.liquidity = 1.0;

    Firm& f = w.firms[0];
    f.wage = 0.0;            // no wage bill, so the buffer is zero
    f.liquidity = 500.0;
    f.payables = 400.0;      // owed to a supplier, settling next month close
    f.inventory = 0.0;
    f.output_cost_basis = 0.0;

    const double before = w.books_balance();
    month_close::distribute_profits(w);

    CHECK(w.books_balance() == doctest::Approx(before));
    // The debt stays funded: only the 100 above it is distributable.
    CHECK(f.liquidity == doctest::Approx(400.0));
    CHECK(balance_sheet::of(w, 0).equity() == doctest::Approx(0.0));
    CHECK_FALSE(balance_sheet::of(w, 0).insolvent());
}
