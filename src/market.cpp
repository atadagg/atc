#include "market.hpp"

#include "world.hpp"

namespace market {

void record_trade(World& w, uint32_t good, double qty, double value) {
    if (good >= w.markets.size()) return;
    if (qty <= 0.0) return;
    w.markets[good].traded_qty += qty;
    w.markets[good].traded_value += value;
}

void publish(World& w) {
    // Markets are index-parallel to goods. Reconcile here rather than trusting
    // every caller that adds a good to remember to resize. Growing keeps every
    // existing entry and gives each new good a market with no reference price
    // yet. Shrinking DISCARDS the trailing entries and their published prices
    // -- goods are never removed today, and if they ever are, the prices of the
    // removed goods are exactly what should go with them.
    if (w.markets.size() != w.goods.size()) w.markets.resize(w.goods.size());

    for (GoodMarket& m : w.markets) {
        if (m.traded_qty > 0.0) {
            m.reference_price = m.traded_value / m.traded_qty;
            m.has_reference = true;
        }
        // A month with no trades leaves the last price standing. Snapping to
        // zero would tell every buyer the good had become free.
        m.traded_qty = 0.0;
        m.traded_value = 0.0;
    }
}

}  // namespace market
