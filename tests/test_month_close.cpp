#include "doctest.h"
#include "month_close.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"
#include "firm_decisions.hpp"

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

TEST_CASE("profits are distributed in proportion to household liquidity") {
    Rng rng(1);
    World w(2, 1, rng);
    w.firms[0].wage = 0.0;
    w.firms[0].liquidity = 300.0;
    w.households[0].liquidity = 100.0;
    w.households[1].liquidity = 200.0;

    double before = w.books_balance();
    month_close::distribute_profits(w);

    // Buffer is chi * wage bill = 0, so all 300 is distributed 1:2.
    CHECK(w.households[0].liquidity == doctest::Approx(200.0));
    CHECK(w.households[1].liquidity == doctest::Approx(400.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(0.0));
    CHECK(w.books_balance() == doctest::Approx(before));
}

TEST_CASE("profit distribution across multiple firms does not create money") {
    // Regression test for a previously-fixed bug: distribute_profits used to
    // recompute each household's share against a liquidity total captured
    // once before the whole loop, while the numerator (household liquidity)
    // kept growing as earlier firms' profits were credited. From the second
    // firm onward the per-household ratios summed to more than 1, so each
    // subsequent firm distributed more than its own profit -- pure money
    // creation. The current implementation snapshots every household's
    // liquidity (and the total) once, before any firm distributes, and uses
    // that fixed snapshot for every firm's split.
    //
    // 3 firms, 3 households, distinct liquidities and distinct distributable
    // profits so that a stale-total bug would visibly diverge from the
    // correct fixed-proportion split by the third firm.
    Rng rng(1);
    World w(3, 3, rng);

    w.households[0].liquidity = 100.0;
    w.households[1].liquidity = 200.0;
    w.households[2].liquidity = 300.0;

    // Zero wage => zero buffer => the firm's entire liquidity is its
    // distributable profit, regardless of employee headcount.
    for (auto& f : w.firms) f.wage = 0.0;
    w.firms[0].liquidity = 30.0;
    w.firms[1].liquidity = 60.0;
    w.firms[2].liquidity = 90.0;

    double before = w.books_balance();
    CHECK(before == doctest::Approx(100.0 + 200.0 + 300.0 + 30.0 + 60.0 + 90.0));

    month_close::distribute_profits(w);

    // Correct behaviour: every firm splits its profit in the SAME fixed
    // proportions (100:200:300 of the initial 600 total), independent of
    // distribution order. Total profit distributed is 30+60+90=180.
    CHECK(w.households[0].liquidity == doctest::Approx(100.0 + 180.0 * (100.0 / 600.0)));
    CHECK(w.households[1].liquidity == doctest::Approx(200.0 + 180.0 * (200.0 / 600.0)));
    CHECK(w.households[2].liquidity == doctest::Approx(300.0 + 180.0 * (300.0 / 600.0)));

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
    month_close::run(w);
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
