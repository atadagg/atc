#include "doctest.h"
#include "market.hpp"
#include "world.hpp"

#include "daily.hpp"

TEST_CASE("a fresh market has no reference price") {
    Rng rng(1);
    World w(10, 5, rng);
    REQUIRE(w.markets.size() == w.goods.size());
    CHECK(w.markets[0].has_reference == false);
}

TEST_CASE("the reference price is the volume-weighted mean of real trades") {
    Rng rng(1);
    World w(10, 5, rng);

    market::record_trade(w, 0, 10.0, 20.0);  // 10 units at 2.0
    market::record_trade(w, 0, 30.0, 30.0);  // 30 units at 1.0
    market::publish(w);

    // (20 + 30) / (10 + 30) = 1.25 -- weighted by volume, so the big cheap
    // trade dominates. A plain mean of 2.0 and 1.0 would say 1.5.
    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(1.25));
}

TEST_CASE("publishing clears the accumulator for the next month") {
    Rng rng(1);
    World w(10, 5, rng);

    market::record_trade(w, 0, 10.0, 20.0);
    market::publish(w);
    market::publish(w);

    // No trades in the second month: the last published price stands rather
    // than snapping to zero. A quiet month is not a free good.
    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
}

TEST_CASE("household purchases feed the consumer good's market") {
    Rng rng(1);
    World w(20, 5, rng);
    for (Firm& f : w.firms) { f.inventory = 1000.0; f.price = 2.0; }
    for (Household& h : w.households) h.planned_consumption = 21.0;

    daily::run_day(w, rng);
    market::publish(w);

    CHECK(w.markets[0].has_reference == true);
    CHECK(w.markets[0].reference_price == doctest::Approx(2.0));
}
