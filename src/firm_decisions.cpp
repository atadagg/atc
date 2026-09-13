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

void pull_toward_reference(Firm& f, const World& w) {
    const Good& g = w.goods[f.good];
    const market::GoodMarket& m = w.markets[f.good];
    if (m.has_reference && g.commoditization > 0.0) {
        f.price = f.price + g.commoditization * (m.reference_price - f.price);
    }
}

void plan_inputs(World& w) {
    for (Firm& f : w.firms) {
        if (!f.alive) continue;
        const Good& g = w.goods[f.good];

        // Same quantity daily::produce's `by_labour` computes: what labour
        // alone could make this month. Planning against anything else would
        // make the firm systematically over- or under-order relative to what
        // it can actually turn the inputs into.
        double labour_capacity =
            w.params.productivity * static_cast<double>(f.employees.size()) / g.labour_per_unit;

        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            f.input_demand[s] = labour_capacity * params::kDaysPerMonth * g.input_qty[s];
        }
    }
}

void rewire_suppliers(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        Firm& f = w.firms[i];
        if (!f.alive) continue;
        const Good& g = w.goods[f.good];

        // A firm whose good has no material inputs has no supplier slots to
        // fill. This check must precede every use of `rng` below -- it is
        // what keeps the single-good legacy world's draw sequence untouched.
        bool has_inputs = false;
        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] != NO_GOOD) { has_inputs = true; break; }
        }
        if (!has_inputs) continue;

        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            const uint32_t base = s * params::kSuppliersPerInput;
            for (uint32_t k = 0; k < params::kSuppliersPerInput; ++k) {
                const uint32_t slot = base + k;
                const bool empty_slot = f.suppliers[slot] == NO_FIRM;
                const bool rationed = f.input_unsatisfied[slot] > 0.0;
                if (!empty_slot && !rationed) continue;

                uint32_t replacement =
                    network::random_unconnected_producer(w, i, g.inputs[s], rng);
                if (replacement == NO_FIRM) continue;  // no eligible producer left
                f.suppliers[slot] = replacement;
            }
        }

        // Consumed by this month's rewiring decision, exactly as
        // Household::unsatisfied is consumed by shop rewiring and then
        // cleared (CLAUDE.md's ordering note).
        f.input_unsatisfied.fill(0.0);
    }
}

void run_monthly(World& w, Rng& rng) {
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        Firm& f = w.firms[i];
        if (!f.alive) continue;
        adjust_wage(f, w.params, rng);
        adjust_workforce(w, i, rng);
        adjust_price(f, w.params, rng);
        pull_toward_reference(f, w);
    }

    rewire_suppliers(w, rng);
    plan_inputs(w);
}

}  // namespace firm_decisions
