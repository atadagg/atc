#include <numeric>
#include <vector>

#include "doctest.h"
#include "daily.hpp"
#include "goods.hpp"
#include "market.hpp"
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
