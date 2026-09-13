#include <numeric>
#include <vector>

#include "doctest.h"
#include "daily.hpp"
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
