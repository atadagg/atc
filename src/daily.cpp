#include "daily.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

#include "ledger.hpp"
#include "market.hpp"
#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace daily {

namespace {

// The buyer's cash account, its supplier slots, and its unmet-demand slots.
// A household shops for the consumer good; a firm buys one input good. Both
// hold a fixed-size set of links and a parallel set of shortfall counters,
// which is why one routine can serve both.
struct BuyerView {
    const double* liquidity;
    const uint32_t* links;
    double* unsatisfied;
    uint32_t link_count;
    double daily_target;
    ledger::Account account;
};

// The widest set of links any buyer holds: a household's shops, or a firm's
// suppliers for one input.
inline constexpr uint32_t kMaxBuyerLinks =
    params::kShopsPerHousehold > params::kSuppliersPerInput ? params::kShopsPerHousehold
                                                            : params::kSuppliersPerInput;

BuyerView view_of(World& w, Buyer b) {
    if (b.kind == BuyerKind::Household) {
        Household& h = w.households[b.index];
        return BuyerView{&h.liquidity,
                         h.shops.data(),
                         h.unsatisfied.data(),
                         params::kShopsPerHousehold,
                         h.planned_consumption / params::kDaysPerMonth,
                         ledger::household(b.index)};
    }
    // Firms do not buy yet. A zero link count returns before any randomness is
    // drawn, so the firm path consumes no RNG at all.
    return BuyerView{&w.firms[b.index].liquidity, nullptr, nullptr, 0, 0.0,
                     ledger::firm(b.index)};
}

}  // namespace

void buy(World& w, Buyer b, Rng& rng, ledger::Journal* journal) {
    BuyerView view = view_of(w, b);
    if (view.link_count == 0) return;

    double target = view.daily_target;
    if (target <= 0.0) return;

    double remaining = target;
    const double stop_at = target * (1.0 - w.params.demand_satisfied_fraction);

    // Visit each of the buyer's link slots at most once per day, in a random
    // order (spec §5.2: retries go to *other* connected sellers, not repeated
    // draws with replacement from the same slot).
    std::array<uint32_t, kMaxBuyerLinks> order{};
    for (uint32_t s = 0; s < view.link_count; ++s) order[s] = s;
    for (std::size_t i = view.link_count - 1; i > 0; --i) {
        uint32_t j = rng.index(static_cast<uint32_t>(i + 1));
        std::swap(order[i], order[j]);
    }

    // The paper's n is a *behavioural* parameter but the visit order lives in
    // a fixed-size array (one slot per link), so a raised n cannot visit
    // sellers the buyer does not have.
    const uint32_t visits = std::min(w.params.max_shop_visits_per_day, view.link_count);
    for (uint32_t visit = 0; visit < visits; ++visit) {
        if (remaining <= stop_at) break;

        uint32_t slot = order[visit];
        uint32_t firm_idx = view.links[slot];
        Firm& f = w.firms[firm_idx];

        const double wanted = remaining;

        // The firm learns what was wanted, whether or not it can supply it.
        f.demand_this_month += wanted;

        if (f.price <= 0.0) continue;

        double affordable = *view.liquidity / f.price;
        double bought = std::min({wanted, f.inventory, affordable});

        if (bought > 0.0) {
            // Clamp the cost to available liquidity so floating-point division
            // can never push the buyer slightly negative.
            double cost = std::min(bought * f.price, *view.liquidity);
            ledger::transfer(w, view.account, ledger::firm(firm_idx), cost,
                             ledger::Reason::Purchase, journal);
            market::record_trade(w, f.good, bought, cost);
            f.inventory -= bought;
            remaining -= bought;
        }

        // Whatever this seller could not supply is its share of the shortfall.
        // Only stock-outs count as rationing — running out of money is the
        // buyer's own constraint, not the seller's failure.
        double stock_shortfall = wanted - std::min(wanted, f.inventory + bought);
        if (stock_shortfall > 0.0) {
            view.unsatisfied[slot] += stock_shortfall;
        }

        if (*view.liquidity <= 0.0) break;
    }
    // Any demand still unmet at the end of the day simply vanishes.
}

void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal) {
    buy(w, Buyer{BuyerKind::Household, household, 0}, rng, journal);
}

void produce(World& w) {
    for (auto& f : w.firms) {
        f.inventory += w.params.productivity * static_cast<double>(f.employees.size());
    }
}

void run_day(World& w, Rng& rng, ledger::Journal* journal) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) shop(w, h, rng, journal);
    produce(w);
}

}  // namespace daily
