#include "doctest.h"
#include "firm_decisions.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("wage rises when a vacancy went unfilled") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = true;
    firm_decisions::adjust_wage(f, rng);
    CHECK(f.wage > 100.0);
    CHECK(f.wage <= 100.0 * (1.0 + params::kWageAdjustMax));
}

TEST_CASE("wage falls after gamma consecutive fully-staffed months") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = params::kMonthsBeforeWageCut;
    firm_decisions::adjust_wage(f, rng);
    CHECK(f.wage < 100.0);
    CHECK(f.wage >= 100.0 * (1.0 - params::kWageAdjustMax));
    CHECK(f.months_fully_staffed == 0u);
}

TEST_CASE("wage is unchanged when neither condition holds") {
    Rng rng(1);
    Firm f;
    f.wage = 100.0;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = 3;
    firm_decisions::adjust_wage(f, rng);
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
    double expected = 63.0 / (params::kProductivity * params::kDaysPerMonth);
    CHECK(firm_decisions::marginal_cost(f) == doctest::Approx(expected));
}

TEST_CASE("price rises toward the cap when inventory is low") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1.0;
    double mc = firm_decisions::marginal_cost(f);
    f.price = mc;  // well below the upper bound

    bool ever_rose = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
        if (f.price > before) ever_rose = true;
        CHECK(f.price <= mc * params::kPriceUpperFactor + 1e-9);
    }
    CHECK(ever_rose);
}

TEST_CASE("price falls toward the floor when inventory is high") {
    Rng rng(1);
    Firm f;
    f.wage = 63.0;
    f.demand_last_month = 100.0;
    f.inventory = 1000.0;
    double mc = firm_decisions::marginal_cost(f);
    f.price = mc * params::kPriceUpperFactor;

    bool ever_fell = false;
    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
        if (f.price < before) ever_fell = true;
        CHECK(f.price >= mc * params::kPriceLowerFactor - 1e-9);
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
    for (int i = 0; i < 50; ++i) firm_decisions::adjust_price(f, rng);
    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("raise branch never lowers price even when price exceeds the cap") {
    Rng rng(1);
    Firm f;
    f.wage = 30.0;  // realistic initial wage
    f.demand_last_month = 100.0;
    f.inventory = 1.0;  // below lower band, triggers raise branch
    f.price = 1.0;  // realistic initial price, which is above its own cap
    double mc = firm_decisions::marginal_cost(f);
    double price_cap = params::kPriceUpperFactor * mc;
    CHECK(f.price > price_cap);  // verify price starts above cap

    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
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
    double mc = firm_decisions::marginal_cost(f);
    double price_floor = params::kPriceLowerFactor * mc;
    f.price = price_floor * 0.5;  // set price well below the floor
    CHECK(f.price < price_floor);  // verify price starts below floor

    for (int i = 0; i < 200; ++i) {
        double before = f.price;
        firm_decisions::adjust_price(f, rng);
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

    firm_decisions::adjust_price(f, rng);

    CHECK(f.price == doctest::Approx(5.0));
}

TEST_CASE("inventory exactly at lower band edge does not open vacancy") {
    Rng rng(1);
    World w(10, 2, rng);
    Firm& f = w.firms[0];
    f.demand_last_month = 100.0;
    double lower = params::kInventoryLowerFactor * 100.0;
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
    double upper = params::kInventoryUpperFactor * 100.0;
    f.inventory = upper;  // exactly at the edge
    f.pending_fire = NO_HOUSEHOLD;

    firm_decisions::adjust_workforce(w, 0, rng);

    CHECK(f.pending_fire == NO_HOUSEHOLD);
}
