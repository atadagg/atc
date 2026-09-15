#include "doctest.h"
#include "firm_decisions.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"
#include "goods.hpp"
#include "market.hpp"

// The published calibration, as defaults. These tests exercise the rules, not
// the parameter plumbing -- test_params.cpp covers that.
static const Params kP;

TEST_CASE("wage rises when a vacancy went unfilled") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = true;
    firm_decisions::adjust_wage(f, kP, rng);
    CHECK(f.wage > 100.0);
    CHECK(f.wage <= 100.0 * (1.0 + kP.wage_adjust_max));
}

TEST_CASE("wage falls after gamma consecutive fully-staffed months") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = kP.months_before_wage_cut;
    firm_decisions::adjust_wage(f, kP, rng);
    CHECK(f.wage < 100.0);
    CHECK(f.wage >= 100.0 * (1.0 - kP.wage_adjust_max));
    CHECK(f.months_fully_staffed == 0u);
}

TEST_CASE("wage is unchanged when neither condition holds") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = 3;
    firm_decisions::adjust_wage(f, kP, rng);
    CHECK(f.wage == doctest::Approx(100.0));
}

TEST_CASE("low inventory opens a vacancy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    f.inventory = 10.0;  // below 0.25 * 100
    f.has_open_vacancy = false;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("high inventory marks one employee for firing next month") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    REQUIRE(f.employees.size() > 0);
    size_t before = f.employees.size();
    f.demand_last_month = 100.0;
    f.inventory = 200.0;  // above 1.0 * 100
    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK(f.pending_fire != NO_HOUSEHOLD);
    CHECK(f.employees.size() == before);  // firing is lagged, not immediate
    CHECK_FALSE(f.has_open_vacancy);
}

TEST_CASE("inventory inside the bands changes nothing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    f.inventory = 50.0;  // between 25 and 100
    f.has_open_vacancy = false;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK_FALSE(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("a firm with no employees does not mark anyone for firing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    while (!f.employees.empty()) network::fire(w, f.employees.front());
    f.demand_last_month = 100.0;
    f.inventory = 200.0;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("marginal cost is wage per unit of monthly output per worker") {
    Firm f;
    f.wage = 63.0;
    double expected = 63.0 / (kP.productivity * params::kDaysPerMonth);
    CHECK(firm_decisions::marginal_cost(f, kP) == doctest::Approx(expected));
}

TEST_CASE("price rises toward the cap when inventory is low") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1.0;
    double mc = firm_decisions::marginal_cost(f, kP);
    f.price = mc;  // well below the upper bound

    bool ever_rose = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, kP, rng);
        if (f.price > before) ever_rose = true;
        CHECK(f.price <= mc * kP.price_upper_factor + 1e-9);
    }
    CHECK(ever_rose);
}

TEST_CASE("price falls toward the floor when inventory is high") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1000.0;
    double mc = firm_decisions::marginal_cost(f, kP);
    f.price = mc * kP.price_upper_factor;

    bool ever_fell = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, kP, rng);
        if (f.price < before) ever_fell = true;
        CHECK(f.price >= mc * kP.price_lower_factor - 1e-9);
    }
    CHECK(ever_fell);
}

TEST_CASE("price is untouched when inventory is inside the bands") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 50.0;
    f.price = 5.0;
    for (int i = 0; i < 50; ++i) firm_decisions::adjust_price(f, kP, rng);
    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("raise branch never lowers price even when price exceeds the cap") {
    Rng rng(1);
    Firm f;
    f.wage = 30.0;  // realistic initial wage
    f.demand_last_month = 100.0;
    f.inventory = 1.0;  // below lower band, triggers raise branch
    f.price = 1.0;  // realistic initial price, which is above its own cap
    double mc = firm_decisions::marginal_cost(f, kP);
    double price_cap = kP.price_upper_factor * mc;
    CHECK(f.price > price_cap);  // verify price starts above cap

    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, kP, rng);
        // The raise branch should never cut the price; it should stay >= before value
        CHECK(f.price >= before - 1e-9);
    }
}

TEST_CASE("lower branch never raises price even when price falls below the floor") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1000.0;  // above upper band, triggers lower branch
    double mc = firm_decisions::marginal_cost(f, kP);
    double price_floor = kP.price_lower_factor * mc;
    f.price = price_floor * 0.5;  // set price well below the floor
    CHECK(f.price < price_floor);  // verify price starts below floor

    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, kP, rng);
        // The lower branch should never raise the price; it should stay <= before value
        CHECK(f.price <= before + 1e-9);
    }
}

TEST_CASE("zero demand_last_month does not open vacancy or mark firing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 0.0;
    f.inventory = 0.0;
    f.has_open_vacancy = false;

    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK_FALSE(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("zero demand_last_month does not trigger price adjustment") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 0.0;
    f.inventory = 0.0;
    f.price = 5.0;

    firm_decisions::adjust_price(f, kP, rng);

    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("inventory exactly at lower band edge does not open vacancy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    double lower = kP.inventory_lower_factor * 100.0;
    f.inventory = lower;  // exactly at the edge
    f.has_open_vacancy = false;

    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK_FALSE(f.has_open_vacancy);
    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("inventory exactly at upper band edge does not mark firing") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    double upper = kP.inventory_upper_factor * 100.0;
    f.inventory = upper;  // exactly at the edge
    f.pending_fire = NO_HOUSEHOLD;

    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK(f.pending_fire == NO_HOUSEHOLD);
}

TEST_CASE("inventory exactly at lower band edge does not adjust price") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    double lower = kP.inventory_lower_factor * 100.0;
    f.inventory = lower;  // exactly at the edge: not < lower, so no raise
    f.price = 5.0;

    firm_decisions::adjust_price(f, kP, rng);

    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("inventory exactly at upper band edge does not adjust price") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    double upper = kP.inventory_upper_factor * 100.0;
    f.inventory = upper;  // exactly at the edge: not > upper, so no cut
    f.price = 5.0;

    firm_decisions::adjust_price(f, kP, rng);

    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("at commoditization 0 the price rule is untouched by the reference") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 0.0;
    w.markets[0].reference_price = 99.0;
    w.markets[0].has_reference = true;

    for (Firm& f : w.firms) f.price = 1.0;
    World before = w;

    for (Firm& f : w.firms) {
        firm_decisions::adjust_price(f, w.params, rng);
        firm_decisions::pull_toward_reference(f, w);
    }

    Rng rng2(3);
    World w2 = before;
    w2.markets[0].has_reference = false;  // no reference at all
    for (Firm& f : w2.firms) {
        firm_decisions::adjust_price(f, w2.params, rng2);
        firm_decisions::pull_toward_reference(f, w2);
    }

    for (std::size_t i = 0; i < w.firms.size(); ++i) {
        CHECK(w.firms[i].price == doctest::Approx(w2.firms[i].price));
    }
}

TEST_CASE("at commoditization 1 the firm is a price taker") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 1.0;
    w.markets[0].reference_price = 5.0;
    w.markets[0].has_reference = true;
    for (Firm& f : w.firms) f.price = 1.0;

    for (Firm& f : w.firms) {
        firm_decisions::adjust_price(f, w.params, rng);
        firm_decisions::pull_toward_reference(f, w);
    }

    for (const Firm& f : w.firms) CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("partial commoditization moves the price part of the way") {
    Rng rng(3);
    World w(20, 5, rng);
    w.goods[0].commoditization = 0.5;
    w.markets[0].reference_price = 3.0;
    w.markets[0].has_reference = true;
    for (Firm& f : w.firms) f.price = 1.0;

    for (Firm& f : w.firms) {
        firm_decisions::adjust_price(f, w.params, rng);
        firm_decisions::pull_toward_reference(f, w);
    }

    // Whatever the firm's own rule decided, the result sits strictly between
    // that and the reference -- the pull is a blend, never a jump.
    for (const Firm& f : w.firms) {
        CHECK(f.price > 1.0);
        CHECK(f.price < 3.0);
    }
}

// Regression: adjust_workforce opened a vacancy on low inventory alone. That
// is right when labour is the only constraint -- the whole single-good world
// -- but wrong under Leontief production, where a firm short of MATERIALS
// gains nothing from another worker and simply carries the payroll. Measured
// at chain depth 3: consumer-good firms held 789 workers to produce what ~240
// could make, were input-limited 91% of firm-days, and cut wages to the floor
// paying for idle labour.
TEST_CASE("a firm rationed on inputs does not hire") {
    Rng rng(1);
    World w(10, 2, rng);

    // Two goods: good 0 feeds good 1.
    w.goods.assign(2, Good{});
    for (auto& g : w.goods) { g.inputs.fill(NO_GOOD); g.input_qty.fill(0.0); g.labour_per_unit = 1.0; }
    w.goods[1].consumer = true;
    w.goods[1].inputs[0] = 0;
    w.goods[1].input_qty[0] = 0.5;
    goods::recompute(w.goods, w.params.commoditization_k);
    w.markets.assign(w.goods.size(), market::GoodMarket{});

    Firm& f = w.firms[0];
    f.good = 1;
    f.inventory = 0.0;              // starved of stock: would hire on its own
    f.demand_last_month = 1000.0;
    f.input_unsatisfied.fill(0.0);

    // Not rationed: low inventory means hire.
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK(f.has_open_vacancy);

    // Rationed on its input last month: another worker has nothing to work on.
    f.input_unsatisfied[0] = 25.0;
    firm_decisions::adjust_workforce(w, 0, rng);
    CHECK_FALSE(f.has_open_vacancy);
}
