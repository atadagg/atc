#include "firm_decisions.hpp"

#include <algorithm>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace firm_decisions {

double marginal_cost(const Firm& f) {
    return f.wage / (params::kProductivity * params::kDaysPerMonth);
}

InventoryBand inventory_band(const Firm& f) {
    return InventoryBand{
        params::kInventoryLowerFactor * f.demand_last_month,
        params::kInventoryUpperFactor * f.demand_last_month,
    };
}

void adjust_wage(Firm& f, Rng& rng) {
    if (f.vacancy_went_unfilled) {
        double mu = rng.uniform(0.0, params::kWageAdjustMax);
        f.wage *= (1.0 + mu);
    } else if (f.months_fully_staffed >= params::kMonthsBeforeWageCut) {
        double mu = rng.uniform(0.0, params::kWageAdjustMax);
        f.wage *= (1.0 - mu);
        f.months_fully_staffed = 0;
    }
}

void adjust_workforce(World& w, uint32_t firm, Rng& rng) {
    Firm& f = w.firms[firm];
    const InventoryBand band = inventory_band(f);

    f.has_open_vacancy = false;
    f.pending_fire = NO_HOUSEHOLD;

    if (f.inventory < band.lower) {
        f.has_open_vacancy = true;
    } else if (f.inventory > band.upper && !f.employees.empty()) {
        uint32_t slot = rng.index(static_cast<uint32_t>(f.employees.size()));
        f.pending_fire = f.employees[slot];
    }
}

void adjust_price(Firm& f, Rng& rng) {
    const InventoryBand band = inventory_band(f);

    double mc = marginal_cost(f);
    double price_floor = params::kPriceLowerFactor * mc;
    double price_cap = params::kPriceUpperFactor * mc;

    double proposed = f.price;
    if (f.inventory < band.lower) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        // Raise only while below the cap. A price already at or above the cap is
        // left alone: clamping it down here would turn a raise into a large cut.
        proposed = (f.price < price_cap)
                       ? std::min(f.price * (1.0 + nu), price_cap)
                       : f.price;
    } else if (f.inventory > band.upper) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        // Symmetric: lower only while above the floor.
        proposed = (f.price > price_floor)
                       ? std::max(f.price * (1.0 - nu), price_floor)
                       : f.price;
    } else {
        return;  // inventory is comfortable; price is not reconsidered
    }

    // Calvo-style stickiness: the change is computed but only sometimes applied.
    if (rng.chance(params::kPriceChangeProb)) {
        f.price = proposed;
    }
}

void run_monthly(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        adjust_wage(w.firms[i], rng);
        adjust_workforce(w, i, rng);
        adjust_price(w.firms[i], rng);
    }
}

}  // namespace firm_decisions
