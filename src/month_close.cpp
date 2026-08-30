#include "month_close.hpp"

#include <algorithm>
#include <vector>

#include "ledger.hpp"
#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace month_close {

void pay_wages(World& w, ledger::Journal* journal) {
    for (uint32_t fi = 0; fi < w.firms.size(); ++fi) {
        Firm& f = w.firms[fi];
        double headcount = static_cast<double>(f.employees.size());
        if (headcount == 0.0) continue;

        double bill = f.wage * headcount;
        double per_worker = f.wage;
        // What the firm should be holding once the payroll is paid. Computed
        // up front so the last worker can be paid the exact remainder rather
        // than a separately computed share -- see the distribution-tail note
        // below.
        double target_balance = f.liquidity - bill;

        // Lengnick 2013 §2.4: rather than allow bankruptcy, employees accept
        // a wage cut just large enough to keep the firm operating. This is
        // what keeps the firm count constant with no entry or exit.
        if (bill > f.liquidity) {
            // The firm still pays out everything it holds -- that part is the
            // paper's rule and is what conserves money here.
            per_worker = f.liquidity / headcount;
            target_balance = 0.0;  // the firm pays out everything it holds

            // The posted wage, however, is floored. Cutting it all the way to
            // what this month happened to afford drives a persistently
            // unprofitable firm to zero geometrically, and zero is absorbing:
            // adjust_wage only ever multiplies, so 0 * (1 + mu) == 0 forever,
            // and marginal_cost 0 collapses the price band to [0, 0]. The
            // shortfall is arrears the firm owes its workers, not a licence to
            // post a wage it can never climb back from.
            double bounded = std::max(f.wage * (1.0 - w.params.max_emergency_wage_cut),
                                      w.params.min_wage);
            f.wage = std::max(per_worker, bounded);
        }

        // Paid one worker at a time, through the chokepoint. `headcount`
        // debits of `per_worker` need not sum to exactly `bill`, so the last
        // worker is paid the difference between what the firm holds and what
        // it should be left holding. Without this the firm keeps a
        // machine-epsilon residue -- and when that residue is negative the
        // solvency check rejects a payment that happens every month in a
        // healthy run (measured: 5,838 times per reference run).
        for (size_t i = 0; i < f.employees.size(); ++i) {
            const bool last = (i + 1 == f.employees.size());
            const double pay = last ? (f.liquidity - target_balance) : per_worker;
            ledger::transfer(w, ledger::firm(fi), ledger::household(f.employees[i]), pay,
                             ledger::Reason::Wage, journal);
        }
    }
}

void distribute_profits(World& w, ledger::Journal* journal) {
    // Ownership proxy: profit shares are proportional to household liquidity,
    // so the wealthy receive more. This is the model's inequality engine.
    // Capture liquidity before any distributions to avoid over-distribution.
    std::vector<double> initial_liquidity(w.households.size());
    double total_household_liquidity = 0.0;
    for (size_t i = 0; i < w.households.size(); ++i) {
        initial_liquidity[i] = w.households[i].liquidity;
        total_household_liquidity += initial_liquidity[i];
    }

    for (uint32_t fi = 0; fi < w.firms.size(); ++fi) {
        Firm& f = w.firms[fi];
        double wage_bill = f.wage * static_cast<double>(f.employees.size());
        double buffer = w.params.buffer_factor * wage_bill;
        double profit = f.liquidity - buffer;
        if (profit <= 0.0) continue;

        // As in pay_wages: the last recipient takes the exact remainder, so
        // the firm is left holding precisely its buffer. An empty firm has a
        // zero wage bill, hence a zero buffer, hence pays out everything --
        // which is the case where separately computed shares are most likely
        // to overshoot the pot (measured: 11,115 times per reference run).
        const double even_share = profit / static_cast<double>(w.households.size());
        for (uint32_t i = 0; i < w.households.size(); ++i) {
            const bool last = (i + 1 == w.households.size());
            double share;
            if (last) {
                share = f.liquidity - buffer;
            } else if (total_household_liquidity <= 0.0) {
                // Degenerate case: split evenly rather than destroying money.
                share = even_share;
            } else {
                share = profit * (initial_liquidity[i] / total_household_liquidity);
            }
            ledger::transfer(w, ledger::firm(fi), ledger::household(i), share,
                             ledger::Reason::Profit, journal);
        }
    }
}

void update_reservation_wages(World& w) {
    for (auto& h : w.households) {
        if (!h.employed()) {
            h.reservation_wage *= (1.0 - w.params.unemployed_reservation_cut);
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

void run(World& w, ledger::Journal* journal) {
    pay_wages(w, journal);
    distribute_profits(w, journal);
    update_reservation_wages(w);
    apply_pending_fires(w);
    roll_demand(w);
}

}  // namespace month_close
