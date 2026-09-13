#include "firm_decisions.hpp"

#include <algorithm>

#include "goods.hpp"
#include "market.hpp"
#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace firm_decisions {

double marginal_cost(const Firm& f, const Params& p) {
    return f.wage / (p.productivity * params::kDaysPerMonth);
}

InventoryBand inventory_band(const Firm& f, const Params& p) {
    return InventoryBand{
        p.inventory_lower_factor * f.demand_last_month,
        p.inventory_upper_factor * f.demand_last_month,
    };
}

void adjust_wage(Firm& f, const Params& p, Rng& rng) {
    if (f.vacancy_went_unfilled) {
        double mu = rng.uniform(0.0, p.wage_adjust_max);
        f.wage *= (1.0 + mu);
    } else if (f.months_fully_staffed >= p.months_before_wage_cut) {
        double mu = rng.uniform(0.0, p.wage_adjust_max);
        f.wage *= (1.0 - mu);
        f.months_fully_staffed = 0;
    }
}

void adjust_workforce(World& w, uint32_t firm, Rng& rng) {
    Firm& f = w.firms[firm];
    const InventoryBand band = inventory_band(f, w.params);

    f.has_open_vacancy = false;
    f.pending_fire = NO_HOUSEHOLD;

    if (f.inventory < band.lower) {
        f.has_open_vacancy = true;
    } else if (f.inventory > band.upper && !f.employees.empty()) {
        uint32_t slot = rng.index(static_cast<uint32_t>(f.employees.size()));
        f.pending_fire = f.employees[slot];
    }
}

void adjust_price(Firm& f, const Params& p, Rng& rng) {
    const InventoryBand band = inventory_band(f, p);

    double mc = marginal_cost(f, p);
    double price_floor = p.price_lower_factor * mc;
    double price_cap = p.price_upper_factor * mc;

    double proposed = f.price;
    if (f.inventory < band.lower) {
        double nu = rng.uniform(0.0, p.price_adjust_max);
        // Raise only while below the cap. A price already at or above the cap is
        // left alone: clamping it down here would turn a raise into a large cut.
        proposed = (f.price < price_cap)
                       ? std::min(f.price * (1.0 + nu), price_cap)
                       : f.price;
    } else if (f.inventory > band.upper) {
        double nu = rng.uniform(0.0, p.price_adjust_max);
        // Symmetric: lower only while above the floor.
        proposed = (f.price > price_floor)
                       ? std::max(f.price * (1.0 - nu), price_floor)
                       : f.price;
    } else {
        return;  // inventory is comfortable; price is not reconsidered
    }

    // Calvo-style stickiness: the change is computed but only sometimes applied.
    if (rng.chance(p.price_change_prob)) {
        f.price = proposed;
    }
}

namespace {

// The commoditization pull. The firm's own gated rule above (raise only
// while below the cap, lower only while above the floor -- see CLAUDE.md, it
// is gated rather than clamped) decides what the firm WANTS. This blends
// that toward what the good actually trades at, weighted by how far the good
// sits from the consumer.
//
// At 0 this is the identity and today's economy is untouched. At 1 the firm
// is a pure price taker: iron ore sells at the iron ore price whatever its
// producer would prefer. Nothing here is an auctioneer -- the reference is
// last month's realised average, and a firm at the reference price can still
// fail to sell.
void pull_toward_reference(Firm& f, const Good& g, const market::GoodMarket& m) {
    if (m.has_reference && g.commoditization > 0.0) {
        f.price = f.price + g.commoditization * (m.reference_price - f.price);
    }
}

}  // namespace

void adjust_price(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        Firm& f = w.firms[i];
        adjust_price(f, w.params, rng);
        pull_toward_reference(f, w.goods[f.good], w.markets[f.good]);
    }
}

void run_monthly(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        Firm& f = w.firms[i];
        adjust_wage(f, w.params, rng);
        adjust_workforce(w, i, rng);
        adjust_price(f, w.params, rng);
        pull_toward_reference(f, w.goods[f.good], w.markets[f.good]);
    }
}

}  // namespace firm_decisions
