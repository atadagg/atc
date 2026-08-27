#include "household_decisions.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace household_decisions {

double average_shop_price(const World& w, uint32_t household) {
    const Household& h = w.households[household];
    double sum = 0.0;
    for (uint32_t f : h.shops) sum += w.firms[f].price;
    return sum / static_cast<double>(params::kShopsPerHousehold);
}

void search_cheaper_shop(World& w, uint32_t household, Rng& rng) {
    if (!rng.chance(params::kProbPriceSearch)) return;

    Household& h = w.households[household];
    uint32_t slot = rng.index(params::kShopsPerHousehold);
    uint32_t incumbent = h.shops[slot];

    uint32_t challenger = network::random_unconnected_firm_by_size(w, household, rng);
    if (challenger == NO_FIRM) return;

    double incumbent_price = w.firms[incumbent].price;
    double challenger_price = w.firms[challenger].price;

    // Switch only if the challenger is at least xi cheaper, proportionally.
    if (challenger_price <= incumbent_price * (1.0 - params::kPriceSwitchThreshold)) {
        network::replace_shop(w, household, slot, challenger);
    }
}

void search_reliable_shop(World& w, uint32_t household, Rng& rng) {
    if (!rng.chance(params::kProbQuantSearch)) return;

    Household& h = w.households[household];
    double total_shortfall = 0.0;
    for (double s : h.unsatisfied) total_shortfall += s;
    if (total_shortfall <= 0.0) return;

    // Pick the shop to drop with probability proportional to its shortfall.
    double draw = rng.uniform(0.0, total_shortfall);
    uint32_t slot = 0;
    for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
        draw -= h.unsatisfied[s];
        if (draw <= 0.0) { slot = s; break; }
    }

    uint32_t replacement = network::random_unconnected_firm_by_size(w, household, rng);
    if (replacement == NO_FIRM) return;
    network::replace_shop(w, household, slot, replacement);
}

void search_job(World& w, uint32_t household, Rng& rng) {
    Household& h = w.households[household];

    uint32_t visits = 0;
    if (!h.employed()) {
        visits = params::kUnemployedFirmVisits;
    } else if (w.firms[h.employer].wage < h.reservation_wage) {
        visits = 1;  // underpaid: always searching
    } else if (rng.chance(params::kEmployedSearchProb)) {
        visits = 1;  // satisfied: occasional look around
    }

    for (uint32_t v = 0; v < visits; ++v) {
        uint32_t candidate = rng.index(static_cast<uint32_t>(w.firms.size()));
        if (!w.firms[candidate].has_open_vacancy) continue;
        if (candidate == h.employer) continue;

        double current_income = h.employed() ? w.firms[h.employer].wage : 0.0;
        if (w.firms[candidate].wage > current_income) {
            network::hire(w, household, candidate);
            w.firms[candidate].has_open_vacancy = false;
            return;  // one job per month
        }
    }
}

void set_consumption_budget(World& w, uint32_t household) {
    Household& h = w.households[household];
    double avg_price = average_shop_price(w, household);
    if (avg_price <= 0.0) { h.planned_consumption = 0.0; return; }

    double real_wealth = h.liquidity / avg_price;
    double desired = std::pow(real_wealth, params::kConsumptionExponent);

    // The min enforces the budget constraint: for real_wealth < 1 the power
    // function would otherwise exceed available wealth.
    h.planned_consumption = std::min(desired, real_wealth);
}

void run_monthly(World& w, Rng& rng) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) {
        search_cheaper_shop(w, h, rng);
        search_reliable_shop(w, h, rng);
        search_job(w, h, rng);
        set_consumption_budget(w, h);

        // Shortfalls are consumed by this month's rewiring decision.
        w.households[h].unsatisfied.fill(0.0);
    }
}

}  // namespace household_decisions
