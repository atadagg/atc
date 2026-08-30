#include "daily.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

#include "ledger.hpp"
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

    // Visit each of the household's 7 shop slots at most once per day, in a
    // random order (spec §5.2: retries go to *other* connected shops, not
    // repeated draws with replacement from the same slot).
    std::array<uint32_t, params::kShopsPerHousehold> order{};
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) order[s] = s;
    for (std::size_t i = order.size() - 1; i > 0; --i) {
        uint32_t j = rng.index(static_cast<uint32_t>(i + 1));
        std::swap(order[i], order[j]);
    }

    for (uint32_t visit = 0; visit < params::kMaxShopVisitsPerDay; ++visit) {
        if (remaining <= stop_at) break;

        uint32_t slot = order[visit];
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
            ledger::transfer(w, ledger::household(household), ledger::firm(firm_idx), cost);
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
