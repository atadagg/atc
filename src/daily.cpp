#include "daily.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace daily {

void shop(World& w, uint32_t household, Rng& rng) {
    Household& h = w.households[household];

    double target = h.planned_consumption / params::kDaysPerMonth;
    if (target <= 0.0) return;

    double remaining = target;
    const double stop_at = target * (1.0 - params::kDemandSatisfiedFraction);

    for (uint32_t visit = 0; visit < params::kMaxShopVisitsPerDay; ++visit) {
        if (remaining <= stop_at) break;

        uint32_t slot = rng.index(params::kShopsPerHousehold);
        uint32_t firm_idx = h.shops[slot];
        Firm& f = w.firms[firm_idx];

        const double wanted = remaining;

        // The firm learns what was wanted, whether or not it can supply it.
        f.demand_this_month += wanted;

        if (f.price <= 0.0) continue;

        double affordable = h.liquidity / f.price;
        double bought = std::min({wanted, f.inventory, affordable});

        if (bought > 0.0) {
            // Clamp the cost to available liquidity so floating-point division
            // can never push the household slightly negative.
            double cost = std::min(bought * f.price, h.liquidity);
            h.liquidity -= cost;
            f.liquidity += cost;
            f.inventory -= bought;
            remaining -= bought;
        }

        // Whatever this shop could not supply is its share of the shortfall.
        // Only stock-outs count as rationing — running out of money is the
        // household's own constraint, not the firm's failure.
        double stock_shortfall = wanted - std::min(wanted, f.inventory + bought);
        if (stock_shortfall > 0.0) {
            h.unsatisfied[slot] += stock_shortfall;
        }

        if (h.liquidity <= 0.0) break;
    }
    // Any demand still unmet at the end of the day simply vanishes.
}

void produce(World& w) {
    for (auto& f : w.firms) {
        f.inventory += params::kProductivity * static_cast<double>(f.employees.size());
    }
}

void run_day(World& w, Rng& rng) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) shop(w, h, rng);
    produce(w);
}

}  // namespace daily
