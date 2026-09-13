#include <numeric>
#include <vector>

#include "doctest.h"
#include "balance_sheet.hpp"
#include "daily.hpp"
#include "firm_decisions.hpp"
#include "goods.hpp"
#include "market.hpp"
#include "month_close.hpp"
#include "network.hpp"
#include "world.hpp"

namespace {

// ore(0, raw) -> bread(1, consumer). Firms 0-1 make ore, firms 2-4 make bread.
World two_stage_world(Rng& rng) {
    World w(50, 5, rng);
    w.goods.resize(2);
    for (Good& g : w.goods) { g.inputs.fill(NO_GOOD); g.input_qty.fill(0.0); }
    w.goods[0].consumer = false;
    w.goods[0].labour_per_unit = 1.0;
    w.goods[1].consumer = true;
    w.goods[1].labour_per_unit = 1.0;
    w.goods[1].inputs[0] = 0;
    w.goods[1].input_qty[0] = 0.5;
    goods::recompute(w.goods, w.params.commoditization_k);
    w.markets.resize(w.goods.size());

    w.firms[0].good = 0; w.firms[1].good = 0;
    for (uint32_t i = 2; i < 5; ++i) w.firms[i].good = 1;

    // Shop links were wired at construction, when every firm made good 0 and
    // good 0 was the consumer good. Good 0 is now iron ore, so re-point every
    // shop slot at a bread producer -- otherwise households shop for ore.
    for (Household& h : w.households) {
        for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
            h.shops[s] = 2u + (s % 3u);
        }
    }
    return w;
}

}  // namespace

TEST_CASE("a firm with no material inputs draws no suppliers and no randomness") {
    Rng rng(5);
    World w(20, 4, rng);  // the default single-good world

    Rng a(99);
    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 0, NO_GOOD}, a);

    // A firm making a good with no inputs must consume no RNG at all --
    // otherwise every existing run's draw sequence shifts and determinism
    // across the single-good special case is lost.
    Rng b(99);
    CHECK(a.uniform(0.0, 1.0) == doctest::Approx(b.uniform(0.0, 1.0)));
}

TEST_CASE("a bread firm buys ore from an ore firm") {
    Rng rng(5);
    World w = two_stage_world(rng);

    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;      // bread firm 2 buys from ore firm 0
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    double ore_before = w.firms[0].inventory;
    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);

    CHECK(w.firms[0].inventory < ore_before);
    CHECK(w.firms[2].input_stock[0] > 0.0);
}

TEST_CASE("input purchases are recorded in the ore market, not the bread market") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    market::publish(w);

    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
    CHECK(w.markets[1].has_reference == false);
}

TEST_CASE("a firm's supplier draw only ever returns producers of that good") {
    Rng rng(5);
    World w = two_stage_world(rng);
    for (int i = 0; i < 50; ++i) {
        uint32_t s = network::random_unconnected_producer(w, 2, 0, rng);
        if (s == NO_FIRM) continue;
        CHECK(w.firms[s].good == 0u);
    }
}

TEST_CASE("household rewiring never points a shop slot at a non-consumer firm") {
    Rng rng(7);
    World w = two_stage_world(rng);
    for (int i = 0; i < 200; ++i) {
        uint32_t a = network::random_unconnected_firm_by_size(w, 0, rng);
        if (a != NO_FIRM) CHECK(w.firms[a].good == 1u);
        uint32_t b = network::random_unconnected_firm(w, 0, rng);
        if (b != NO_FIRM) CHECK(w.firms[b].good == 1u);
    }
}

TEST_CASE("market::publish reconciles markets to goods") {
    Rng rng(3);
    World w(10, 2, rng);
    w.goods.resize(3);
    // markets deliberately left at size 1
    market::publish(w);
    CHECK(w.markets.size() == w.goods.size());
    CHECK(w.markets[2].has_reference == false);
}

TEST_CASE("a firm's monthly input want does not decay across the month") {
    Rng rng(11);
    World w = two_stage_world(rng);

    // Both ore firms deeply stocked and cheap; the bread firm rich. Nothing
    // but the daily slice itself can limit what firm 2 accumulates.
    for (uint32_t f : {0u, 1u}) {
        w.firms[f].inventory = 1e6;
        w.firms[f].price = 1.0;
    }
    w.firms[2].suppliers[0] = 0;
    w.firms[2].suppliers[1] = 1;
    w.firms[2].liquidity = 1e6;

    const double want = 21.0;
    w.firms[2].input_demand[0] = want;

    for (int day = 0; day < params::kDaysPerMonth; ++day) {
        daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    }

    // input_demand is the firm's analogue of planned_consumption: it stands
    // for the whole month and the daily slice is recomputed from it. Drawing
    // it down instead made the slice decay geometrically and the firm filled
    // only about 64% of its want.
    CHECK(w.firms[2].input_demand[0] == doctest::Approx(want));
    CHECK(w.firms[2].input_stock[0] == doctest::Approx(want));
}

TEST_CASE("run_day moves inputs from ore firms to bread firms") {
    Rng rng(13);
    World w = two_stage_world(rng);

    for (uint32_t f : {0u, 1u}) {
        w.firms[f].inventory = 1000.0;
        w.firms[f].price = 1.0;
    }
    for (uint32_t f = 2; f < 5; ++f) {
        w.firms[f].suppliers[0] = 0;
        w.firms[f].suppliers[1] = 1;
        w.firms[f].liquidity = 5000.0;
        w.firms[f].input_demand[0] = 21.0;
        w.firms[f].inventory = 1000.0;  // so households do not stock them out
        // No workforce: this test is about the day MOVING ore into the bread
        // firms' input_stock, not about production consuming it again in the
        // same day now that produce() is input-limited.
        w.firms[f].employees.clear();
    }

    const double ore_before = w.firms[0].inventory + w.firms[1].inventory;
    const double owed_before = w.firms[0].receivables + w.firms[1].receivables;
    // The fixture hands out cash by direct assignment, so the books do not sum
    // to zero here; what must hold is that the day MOVES money without
    // creating any.
    const double books_before = w.books_balance();

    daily::run_day(w, rng);

    double stocked = 0.0;
    for (uint32_t f = 2; f < 5; ++f) stocked += w.firms[f].input_stock[0];

    CHECK(stocked > 0.0);
    // Inputs are bought on trade credit, so the ore firms' cash does not move
    // during the day -- what grows is what they are owed.
    CHECK(w.firms[0].receivables + w.firms[1].receivables > owed_before);
    // Production adds to the ore firms' inventory too, so compare the sold
    // quantity against what the bread firms took in.
    CHECK(ore_before - (w.firms[0].inventory + w.firms[1].inventory) +
              w.params.productivity * static_cast<double>(w.firms[0].employees.size() +
                                                          w.firms[1].employees.size()) ==
          doctest::Approx(stocked));
    CHECK(w.books_balance() == doctest::Approx(books_before));
}

TEST_CASE("output is limited by the scarcest input, not by labour alone") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};       // labour capacity = 3 * productivity = 9
    bread.input_stock[0] = 2.0;        // enough ore for 2 / 0.5 = 4 units

    daily::produce(w);

    CHECK(bread.inventory == doctest::Approx(4.0));
    CHECK(bread.input_stock[0] == doctest::Approx(0.0));
}

TEST_CASE("an input stockout upstream stops output downstream") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};
    bread.input_stock[0] = 0.0;

    daily::produce(w);

    CHECK(bread.inventory == doctest::Approx(0.0));
}

TEST_CASE("a firm with no material inputs still produces on labour alone") {
    Rng rng(5);
    World w = two_stage_world(rng);

    Firm& ore = w.firms[0];
    ore.employees = {10, 11};

    daily::produce(w);

    CHECK(ore.inventory == doctest::Approx(2.0 * w.params.productivity));
}

TEST_CASE("an input purchase creates a payable, not an immediate payment") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    double buyer_cash = w.firms[2].liquidity;
    double seller_cash = w.firms[0].liquidity;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);

    CHECK(w.firms[2].liquidity == doctest::Approx(buyer_cash));
    CHECK(w.firms[0].liquidity == doctest::Approx(seller_cash));
    CHECK(w.firms[2].payables > 0.0);
    CHECK(w.firms[0].receivables == doctest::Approx(w.firms[2].payables));
}

TEST_CASE("settlement moves the money and clears both sides") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[2].liquidity = 1000.0;
    w.firms[2].payables = 40.0;
    w.firms[2].payable_to[0] = 0;
    w.firms[2].payable_amount[0] = 40.0;
    w.firms[0].receivables = 40.0;

    month_close::settle_trade_credit(w, rng);

    CHECK(w.firms[2].liquidity == doctest::Approx(960.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(40.0));
    CHECK(w.firms[2].payables == doctest::Approx(0.0));
    CHECK(w.firms[0].receivables == doctest::Approx(0.0));
}

TEST_CASE("the books balance with trade credit outstanding") {
    Rng rng(5);
    World w = two_stage_world(rng);
    double before = w.books_balance();

    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].liquidity = 1000.0;
    w.firms[2].input_demand[0] = 10.0;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    CHECK(w.books_balance() == doctest::Approx(before + 1000.0));

    month_close::settle_trade_credit(w, rng);
    CHECK(w.books_balance() == doctest::Approx(before + 1000.0));
}

TEST_CASE("a firm with no cash still buys in full and is billed in full") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 100.0;
    w.firms[0].price = 2.0;
    w.firms[2].suppliers[0] = 0;
    w.firms[2].suppliers[1] = NO_FIRM;
    w.firms[2].suppliers[2] = NO_FIRM;
    w.firms[2].liquidity = 0.0;  // broke, but trade credit does not care
    w.firms[2].input_demand[0] = 21.0;  // one unit a day

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);

    // The full daily want, not a cash-rationed slice.
    CHECK(w.firms[2].input_stock[0] == doctest::Approx(1.0));
    // Billed for exactly what arrived -- the household cost clamp must not
    // apply here, or a zero-cash firm would be invoiced zero.
    CHECK(w.firms[2].payables == doctest::Approx(2.0));
    CHECK(w.firms[0].receivables == doctest::Approx(2.0));
    CHECK(w.firms[2].liquidity == doctest::Approx(0.0));
}

TEST_CASE("a household is still capped by its cash") {
    Rng rng(5);
    World w = two_stage_world(rng);
    for (uint32_t i = 2; i < 5; ++i) {
        w.firms[i].inventory = 1000.0;
        w.firms[i].price = 2.0;
    }
    Household& h = w.households[0];
    h.liquidity = 1.0;
    h.planned_consumption = 21.0 * 100.0;  // wants far more than it can pay for

    daily::shop(w, 0, rng);

    CHECK(h.liquidity >= 0.0);
    CHECK(h.liquidity == doctest::Approx(0.0));
}

TEST_CASE("output cost basis carries input cost plus accrued labour") {
    Rng rng(5);
    World w = two_stage_world(rng);
    Firm& bread = w.firms[2];
    // Labour is a Leontief input too, so the firm keeps its workers; a zero
    // wage is what isolates the material cost.
    bread.wage = 0.0;
    bread.inventory = 0.0;
    bread.output_cost_basis = 0.0;
    bread.input_stock[0] = 100.0;
    bread.input_cost_basis[0] = 2.0;  // ore cost 2/unit

    daily::produce(w);

    // 0.5 ore per bread at 2 each = 1.0 of material per unit, no labour.
    CHECK(bread.inventory > 0.0);
    CHECK(bread.output_cost_basis == doctest::Approx(1.0));

    // With workers, a day's share of payroll rides along.
    World w2 = two_stage_world(rng);
    Firm& b2 = w2.firms[2];
    b2.inventory = 0.0;
    b2.output_cost_basis = 0.0;
    b2.input_stock[0] = 1000.0;
    b2.input_cost_basis[0] = 2.0;
    b2.wage = 21.0;
    const double made = w2.params.productivity * static_cast<double>(b2.employees.size());
    REQUIRE(made > 0.0);
    const double payroll = 21.0 * static_cast<double>(b2.employees.size()) / 21.0;

    daily::produce(w2);

    CHECK(b2.output_cost_basis == doctest::Approx(1.0 + payroll / made));
}

TEST_CASE("buying inputs on credit and producing does not read as insolvent") {
    Rng rng(5);
    World w = two_stage_world(rng);
    w.firms[0].inventory = 1000.0;
    w.firms[0].price = 2.0;
    Firm& bread = w.firms[2];
    bread.suppliers[0] = 0;
    bread.suppliers[1] = NO_FIRM;
    bread.suppliers[2] = NO_FIRM;
    bread.liquidity = 0.0;  // the phantom-insolvency setup: no cash at all
    bread.inventory = 0.0;
    bread.output_cost_basis = 0.0;
    bread.wage = 0.0;  // isolate the material chain from accrued wages
    bread.input_demand[0] = 21.0 * 10.0;

    daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
    REQUIRE(bread.payables > 0.0);
    daily::produce(w);
    REQUIRE(bread.inventory > 0.0);

    // The input value moved into finished goods; it did not vanish. Equity is
    // still zero, not minus the supplier bill.
    balance_sheet::Sheet s = balance_sheet::of(w, 2);
    CHECK(s.equity() == doctest::Approx(0.0));
    CHECK_FALSE(s.insolvent());
}

TEST_CASE("planned input demand follows planned output") {
    Rng rng(6);
    World w = two_stage_world(rng);
    Firm& bread = w.firms[2];
    bread.employees = {0, 1, 2};   // 3 * productivity units of output capacity

    firm_decisions::plan_inputs(w);

    // 9 units of bread at 0.5 ore each = 4.5 ore for the month.
    CHECK(bread.input_demand[0] ==
          doctest::Approx(3.0 * w.params.productivity * 0.5 * params::kDaysPerMonth));
}

TEST_CASE("a firm rationed by a supplier rewires away from it") {
    Rng rng(6);
    World w = two_stage_world(rng);
    Firm& bread = w.firms[2];
    bread.suppliers[0] = 0;
    bread.input_unsatisfied[0] = 500.0;   // firm 0 kept turning it away

    firm_decisions::rewire_suppliers(w, rng);

    CHECK(bread.suppliers[0] != 0u);
    CHECK(bread.input_unsatisfied[0] == doctest::Approx(0.0));
}

TEST_CASE("a firm with no material inputs draws nothing when rewiring") {
    Rng rng(6);
    World w(20, 4, rng);   // single-good world

    Rng a(77);
    firm_decisions::rewire_suppliers(w, a);
    Rng b(77);
    CHECK(a.uniform(0.0, 1.0) == doctest::Approx(b.uniform(0.0, 1.0)));
}

TEST_CASE("a commodity buyer compares more sellers than a bespoke one") {
    Rng rng(8);
    World w = two_stage_world(rng);

    // Both goods, same buyer state, different commoditization.
    CHECK(daily::sellers_compared(w, 0) > daily::sellers_compared(w, 1));

    // The consumer good sits at commoditization 0 and must compare exactly
    // what the existing rule compares -- no more, no less, or the single-good
    // world changes.
    CHECK(daily::sellers_compared(w, 1) == 1u);
}

TEST_CASE("a commodity link is abandoned more readily than a bespoke one") {
    Rng rng(8);
    World w = two_stage_world(rng);

    // Stickiness is the inverse of commoditization: ore (upstream, fungible)
    // is switched away from freely; the consumer good is not.
    CHECK(daily::switch_probability(w, 0) > daily::switch_probability(w, 1));
    CHECK(daily::switch_probability(w, 1) == doctest::Approx(0.0));
}

TEST_CASE("at commoditization 0 the single-good world is untouched") {
    Rng rng(8);
    World w(20, 4, rng);
    CHECK(daily::sellers_compared(w, 0) == 1u);
    CHECK(daily::switch_probability(w, 0) == doctest::Approx(0.0));
}

// ---------------------------------------------------------------------------
// Task 14b: the firm side gets the household side's TWO draws, for the same
// reason -- an unopposed size weighting makes a new entrant undiscoverable.
// ---------------------------------------------------------------------------

namespace {

// ore(0) -> bread(1), with ore firm 0 a large incumbent and ore firm 1 a
// zero-employee entrant. Firms 2-4 make bread. Every supplier slot is empty.
World entrant_world(Rng& rng) {
    World w = two_stage_world(rng);
    for (Firm& f : w.firms) f.employees.clear();
    for (Household& h : w.households) h.employer = NO_FIRM;
    for (uint32_t i = 0; i < 40; ++i) w.firms[0].employees.push_back(i);  // incumbent
    // firm 1 stays at zero employees: the entrant.
    for (uint32_t i = 0; i < 5; ++i) w.firms[i].price = 1.0;
    for (uint32_t i = 2; i < 5; ++i) {
        w.firms[i].suppliers.fill(NO_FIRM);
        w.firms[i].input_unsatisfied.fill(0.0);
    }
    return w;
}

}  // namespace

TEST_CASE("the unweighted producer draw is flat; the weighted one is not") {
    Rng setup(11);
    World w = entrant_world(setup);
    REQUIRE(w.firms[0].employees.size() == 40u);
    REQUIRE(w.firms[1].employees.empty());

    // The buyer holds no ore supplier, so both ore firms are eligible on
    // every draw and a flat draw must split 50/50.
    const int kDraws = 40000;
    Rng flat_rng(3);
    int flat_entrant = 0;
    for (int i = 0; i < kDraws; ++i) {
        if (network::random_unconnected_producer_unweighted(w, 2, 0, flat_rng) == 1u) {
            ++flat_entrant;
        }
    }
    const double flat_share = static_cast<double>(flat_entrant) / kDraws;
    CHECK(flat_share > 0.47);
    CHECK(flat_share < 0.53);

    // The weighted draw, on the same world, must still favour the incumbent
    // by roughly its employee count -- it is kept for the price-tested path.
    Rng weighted_rng(3);
    int weighted_entrant = 0;
    for (int i = 0; i < kDraws; ++i) {
        if (network::random_unconnected_producer(w, 2, 0, weighted_rng) == 1u) {
            ++weighted_entrant;
        }
    }
    const double weighted_share = static_cast<double>(weighted_entrant) / kDraws;
    CHECK(weighted_share < 0.05);  // 1 / (41 + 1) ~= 0.024

    // The whole point: unambiguously different draws, not two spellings of one.
    CHECK(flat_share > weighted_share * 5.0);
}

TEST_CASE("the stickiness path does not switch to a more expensive challenger") {
    Rng setup(12);
    World w = entrant_world(setup);
    REQUIRE(daily::switch_probability(w, 0) > 0.0);  // ore is switchable at all

    // Bread firm 2 buys all three ore slots from firm 0 at 1.0. The only
    // possible challenger, firm 1, is five times dearer. No slot is rationed,
    // so the only live path is stickiness -- which must apply a price test.
    w.firms[0].price = 1.0;
    w.firms[1].price = 5.0;
    w.firms[2].suppliers.fill(0u);
    w.firms[2].input_unsatisfied.fill(0.0);

    Rng rng(4);
    for (int month = 0; month < 500; ++month) {
        for (uint32_t i = 2; i < 5; ++i) {
            w.firms[i].suppliers.fill(0u);
            w.firms[i].input_unsatisfied.fill(0.0);
        }
        firm_decisions::rewire_suppliers(w, rng);
        for (uint32_t k = 0; k < params::kSuppliersPerInput; ++k) {
            REQUIRE(w.firms[2].suppliers[k] == 0u);
        }
    }

    // Converse: the same path DOES switch when the challenger is genuinely
    // cheaper, so the test above is not passing for want of any switching.
    w.firms[1].price = 0.5;
    bool switched = false;
    for (int month = 0; month < 500 && !switched; ++month) {
        for (uint32_t i = 2; i < 5; ++i) {
            w.firms[i].suppliers.fill(0u);
            w.firms[i].input_unsatisfied.fill(0.0);
        }
        firm_decisions::rewire_suppliers(w, rng);
        for (uint32_t k = 0; k < params::kSuppliersPerInput; ++k) {
            if (w.firms[2].suppliers[k] == 1u) switched = true;
        }
    }
    CHECK(switched);
}

TEST_CASE("the rationed path rewires unconditionally, price notwithstanding") {
    Rng setup(13);
    World w = entrant_world(setup);

    // Every slot held by firm 0, every slot rationed, and the only
    // alternative -- the zero-employee entrant -- is far dearer. Rationing
    // applies no price test, so the buyer must leave anyway.
    w.firms[0].price = 1.0;
    w.firms[1].price = 100.0;

    for (int trial = 0; trial < 200; ++trial) {
        for (uint32_t i = 2; i < 5; ++i) {
            w.firms[i].suppliers.fill(0u);
            w.firms[i].input_unsatisfied.fill(1.0);
        }
        Rng rng(static_cast<uint64_t>(trial) + 100u);
        firm_decisions::rewire_suppliers(w, rng);
        // Slot 0 is rationed and must move; the only eligible producer is 1.
        REQUIRE(w.firms[2].suppliers[0] == 1u);
    }

    // And the shortfall is consumed, as Household::unsatisfied is.
    for (uint32_t k = 0; k < params::kSuppliersPerInput; ++k) {
        CHECK(w.firms[2].input_unsatisfied[k] == doctest::Approx(0.0));
    }
}

TEST_CASE("a cheap stocked-out seller is still visited and still records demand") {
    Rng setup(14);
    World w = entrant_world(setup);
    REQUIRE(daily::sellers_compared(w, 0) > 1u);  // ore is a compared good

    // Ore firm 1 is the cheap entrant with EMPTY shelves; ore firm 0 is dear
    // and fully stocked. Demand in this model is what was WANTED, not bought,
    // so the empty-shelved entrant must still hear the want -- that is the
    // only way it ever opens a vacancy.
    w.firms[0].price = 10.0;
    w.firms[0].inventory = 1.0e6;
    w.firms[1].price = 1.0;
    w.firms[1].inventory = 0.0;

    Firm& buyer = w.firms[2];
    buyer.suppliers.fill(1u);
    buyer.suppliers[0] = 0u;  // one dear stocked slot, two cheap empty ones
    buyer.input_demand[0] = 210.0;
    buyer.liquidity = 1.0e9;

    Rng rng(9);
    int days_heard = 0;
    const int kDays = 300;
    for (int d = 0; d < kDays; ++d) {
        w.firms[1].demand_this_month = 0.0;
        w.firms[1].inventory = 0.0;
        w.firms[0].inventory = 1.0e6;
        buyer.input_unsatisfied.fill(0.0);
        buyer.payable_to.fill(NO_FIRM);
        buyer.payable_amount.fill(0.0);
        daily::buy(w, daily::Buyer{daily::BuyerKind::Firm, 2, 0}, rng);
        if (w.firms[1].demand_this_month > 0.0) ++days_heard;
    }
    // Sorting empty shelves to the back made this exactly zero: the buyer
    // filled up at the dear stocked seller and broke out of the visit loop.
    CHECK(days_heard > kDays / 2);
}
