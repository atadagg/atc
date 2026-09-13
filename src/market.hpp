#pragma once

#include <cstdint>

class World;

namespace market {

// What a good traded at, aggregated over a month.
//
// This is a MEASUREMENT, not a clearing price. Nothing computes a price at
// which supply equals demand, and nothing guarantees an order fills at the
// published number -- it is the average of trades that actually happened,
// reported with a one-month lag. It is free to spike and free to crash.
struct GoodMarket {
    double traded_value = 0.0;   // accumulating, this month
    double traded_qty = 0.0;     // accumulating, this month
    double reference_price = 0.0;
    bool has_reference = false;  // false until a first month has traded
};

// Record one completed trade. Called from the buying path, never speculatively.
void record_trade(World& w, uint32_t good, double qty, double value);

// Turn this month's accumulation into next month's published price and reset
// the accumulator. A month with no trades leaves the previous price standing.
void publish(World& w);

}  // namespace market
