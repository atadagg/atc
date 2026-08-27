#include "firm_decisions.hpp"

#include <algorithm>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace firm_decisions {

double marginal_cost(const Firm& f) {
    return f.wage / (params::kProductivity * params::kDaysPerMonth);
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
    double lower = params::kInventoryLowerFactor * f.demand_last_month;
    double upper = params::kInventoryUpperFactor * f.demand_last_month;

    f.has_open_vacancy = false;
    f.pending_fire = NO_HOUSEHOLD;

    if (f.inventory < lower) {
        f.has_open_vacancy = true;
    } else if (f.inventory > upper && !f.employees.empty()) {
        uint32_t slot = rng.index(static_cast<uint32_t>(f.employees.size()));
        f.pending_fire = f.employees[slot];
    }
}

void adjust_price(Firm& f, Rng& rng) {
    double lower_inv = params::kInventoryLowerFactor * f.demand_last_month;
    double upper_inv = params::kInventoryUpperFactor * f.demand_last_month;

    double mc = marginal_cost(f);
    double price_floor = params::kPriceLowerFactor * mc;
    double price_cap = params::kPriceUpperFactor * mc;

    double proposed = f.price;
    if (f.inventory < lower_inv) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        proposed = std::min(f.price * (1.0 + nu), price_cap);
    } else if (f.inventory > upper_inv) {
        double nu = rng.uniform(0.0, params::kPriceAdjustMax);
        proposed = std::max(f.price * (1.0 - nu), price_floor);
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
