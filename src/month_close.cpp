#include "month_close.hpp"

#include <algorithm>
#include <vector>

#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace month_close {

void pay_wages(World& w) {
    for (auto& f : w.firms) {
        double headcount = static_cast<double>(f.employees.size());
        if (headcount == 0.0) continue;

        double bill = f.wage * headcount;

        // Lengnick 2013 §2.4: rather than allow bankruptcy, employees accept
        // a wage cut just large enough to keep the firm operating. This is
        // what keeps the firm count constant with no entry or exit.
        if (bill > f.liquidity) {
            f.wage = std::max(0.0, f.liquidity / headcount);
            bill = f.wage * headcount;
        }

        f.liquidity -= bill;
        for (uint32_t h : f.employees) w.households[h].liquidity += f.wage;
    }
}

void distribute_profits(World& w) {
    // Ownership proxy: profit shares are proportional to household liquidity,
    // so the wealthy receive more. This is the model's inequality engine.
    // Capture liquidity before any distributions to avoid over-distribution.
    std::vector<double> initial_liquidity(w.households.size());
    double total_household_liquidity = 0.0;
    for (size_t i = 0; i < w.households.size(); ++i) {
        initial_liquidity[i] = w.households[i].liquidity;
        total_household_liquidity += initial_liquidity[i];
    }

    for (auto& f : w.firms) {
        double wage_bill = f.wage * static_cast<double>(f.employees.size());
        double buffer = params::kBufferFactor * wage_bill;
        double profit = f.liquidity - buffer;
        if (profit <= 0.0) continue;

        f.liquidity -= profit;

        if (total_household_liquidity <= 0.0) {
            // Degenerate case: split evenly rather than destroying money.
            double share = profit / static_cast<double>(w.households.size());
            for (auto& h : w.households) h.liquidity += share;
        } else {
            for (size_t i = 0; i < w.households.size(); ++i) {
                w.households[i].liquidity += profit * (initial_liquidity[i] / total_household_liquidity);
            }
        }
    }
}

void update_reservation_wages(World& w) {
    for (auto& h : w.households) {
        if (!h.employed()) {
            h.reservation_wage *= (1.0 - params::kUnemployedReservationCut);
            continue;
        }
        double income = w.firms[h.employer].wage;
        if (income > h.reservation_wage) {
            h.reservation_wage = income;
        }
        // If income < reservation wage the claim is NOT lowered; the household
        // searches harder instead (see household_decisions::search_job).
    }
}

void apply_pending_fires(World& w) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(w.firms.size()); ++i) {
        Firm& f = w.firms[i];
        if (f.pending_fire == NO_HOUSEHOLD) continue;
        uint32_t victim = f.pending_fire;
        f.pending_fire = NO_HOUSEHOLD;
        // Only fire if still employed here — they may have quit in the interim.
        if (w.households[victim].employer == i) {
            network::fire(w, victim);
        }
    }
}

void roll_demand(World& w) {
    for (auto& f : w.firms) {
        f.demand_last_month = f.demand_this_month;
        f.demand_this_month = 0.0;
    }
}

void run(World& w) {
    pay_wages(w);
    distribute_profits(w);
    update_reservation_wages(w);
    apply_pending_fires(w);
    roll_demand(w);
}

}  // namespace month_close
