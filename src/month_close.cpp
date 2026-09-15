#include "month_close.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "firm_lifecycle.hpp"
#include "ledger.hpp"
#include "network.hpp"
#include "rng.hpp"
#include "params.hpp"
#include "world.hpp"

namespace month_close {
namespace {

// A distribution pays out a pot in N pieces. Finite arithmetic means the N
// pieces need not sum to exactly the pot -- in either direction -- so each
// payment is capped by what the firm can actually part with: what it holds
// above the balance it must be left with.
//
// In the ordinary case the cap never binds; the shares sum to the pot by
// construction. It binds only on a rounding crumb, and it is what stops two
// distinct failures: a negative final remainder (rejected as an invalid
// amount) and, for a firm whose retained balance is zero, an outright
// overdraw of the account.
double capped_payment(double want, double available) {
    if (available <= 0.0) return 0.0;
    return std::min(want, available);
}

// Absorbing a crumb is correct; absorbing a real discrepancy is not. After a
// distribution the firm must be sitting on its target balance, and anything
// further out than float noise is a bug in the shares rather than in the last
// bit of a double.
void check_landed_on_target(double balance, double target, const char* where) {
    const double tolerance = 1e-9 * std::max(1.0, std::abs(target));
    if (std::abs(balance - target) > tolerance) {
        throw std::logic_error(std::string(where) +
                               ": distribution did not land on its target balance");
    }
}

}  // namespace

void settle_trade_credit(World& w, Rng& rng, ledger::Journal* journal) {
    // Gather the debtors first. If nobody owes anything the function returns
    // without touching `rng` -- see the header. This is load-bearing for
    // determinism, not an optimisation.
    std::vector<uint32_t> debtors;
    for (uint32_t fi = 0; fi < w.firms.size(); ++fi) {
        if (!w.firms[fi].alive) continue;
        if (w.firms[fi].payables > 0.0) debtors.push_back(fi);
    }
    if (debtors.empty()) return;

    rng.shuffle(debtors);

    for (uint32_t fi : debtors) {
        Firm& f = w.firms[fi];

        double owed = 0.0;
        for (std::size_t s = 0; s < f.payable_amount.size(); ++s) {
            if (f.payable_to[s] == NO_FIRM) continue;
            if (f.payable_amount[s] > 0.0) owed += f.payable_amount[s];
        }
        if (owed <= 0.0) {
            f.payables = 0.0;
            continue;
        }

        // Short of cash, the firm pays the same fraction of every bill. The
        // remainder is NOT forgiven: it stays on the books as the arrears
        // that the insolvency test reads.
        const double fraction = f.liquidity >= owed ? 1.0 : f.liquidity / owed;

        double still_owed = 0.0;
        for (std::size_t s = 0; s < f.payable_amount.size(); ++s) {
            const uint32_t creditor = f.payable_to[s];
            if (creditor == NO_FIRM) continue;
            double bill = f.payable_amount[s];
            if (bill <= 0.0) {
                f.payable_to[s] = NO_FIRM;
                f.payable_amount[s] = 0.0;
                continue;
            }

            // Capped at what the account actually holds: rounding in
            // `fraction` must never reach `transfer` as an overdraft.
            double pay = std::min(bill * fraction, f.liquidity);
            if (pay > 0.0) {
                ledger::transfer(w, ledger::firm(fi), ledger::firm(creditor), pay,
                                 ledger::Reason::TradeCreditSettlement, journal);
                Firm& c = w.firms[creditor];
                c.receivables -= pay;
                if (c.receivables < 0.0) c.receivables = 0.0;
            }

            double remainder = bill - pay;
            if (remainder <= 1e-12) {
                f.payable_to[s] = NO_FIRM;
                f.payable_amount[s] = 0.0;
            } else {
                f.payable_amount[s] = remainder;
                still_owed += remainder;
            }
        }

        f.payables = still_owed;
    }
}

void pay_wages(World& w, ledger::Journal* journal) {
    for (uint32_t fi = 0; fi < w.firms.size(); ++fi) {
        Firm& f = w.firms[fi];
        if (!f.alive) continue;
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
        // it should be left holding, and every payment is capped at what is
        // actually available. Without the remainder the firm keeps a
        // machine-epsilon residue and the solvency check rejects a payment
        // that happens every month in a healthy run (measured: 5,838 times per
        // reference run); without the cap the remainder itself can come out
        // negative.
        for (size_t i = 0; i < f.employees.size(); ++i) {
            const bool last = (i + 1 == f.employees.size());
            const double want = last ? (f.liquidity - target_balance) : per_worker;
            ledger::transfer(w, ledger::firm(fi), ledger::household(f.employees[i]),
                             capped_payment(want, f.liquidity - target_balance),
                             ledger::Reason::Wage, journal);
        }
        check_landed_on_target(f.liquidity, target_balance, "pay_wages");
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
        if (!f.alive) continue;
        double wage_bill = f.wage * static_cast<double>(f.employees.size());
        double buffer = w.params.buffer_factor * wage_bill;
        const double retain = buffer + f.payables;
        double profit = f.liquidity - retain;
        if (profit <= 0.0) continue;

        // As in pay_wages: the last recipient takes the exact remainder, so
        // the firm is left holding precisely its buffer. An empty firm has a
        // zero wage bill, hence a zero buffer, hence pays out everything --
        // which is the case where separately computed shares are most likely
        // to overshoot the pot (measured: 11,115 times per reference run).
        //
        // The 999 proportional shares can also overshoot in the *other*
        // direction, leaving the firm a hair below its buffer and this
        // remainder negative. A household whose entitlement is exactly zero
        // (no liquidity of its own) is maximally exposed to that, because it
        // has no share of its own to absorb the accumulated error. Seeds 5 and
        // 7 aborted this way before `capped_payment` was added.
        const double even_share = profit / static_cast<double>(w.households.size());
        for (uint32_t i = 0; i < w.households.size(); ++i) {
            const bool last = (i + 1 == w.households.size());
            double want;
            if (last) {
                want = f.liquidity - retain;
            } else if (total_household_liquidity <= 0.0) {
                // Degenerate case: split evenly rather than destroying money.
                want = even_share;
            } else {
                want = profit * (initial_liquidity[i] / total_household_liquidity);
            }
            ledger::transfer(w, ledger::firm(fi), ledger::household(i),
                             capped_payment(want, f.liquidity - retain),
                             ledger::Reason::Profit, journal);
        }
        check_landed_on_target(f.liquidity, retain, "distribute_profits");
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

void run(World& w, Rng& rng, ledger::Journal* journal) {
    // Suppliers before workers: a payroll squeeze should be a consequence of
    // a supply bill, not the other way round.
    settle_trade_credit(w, rng, journal);
    pay_wages(w, journal);
    distribute_profits(w, journal);
    update_reservation_wages(w);
    apply_pending_fires(w);
    // Position matters. `output_cost_basis` books labour the firm has not yet
    // paid and there is no matching accrued-wages liability, so equity is
    // overstated mid-month and steps down when wages leave. After
    // `settle_trade_credit` and `pay_wages` is the point of least distortion:
    // the supply bills and the payroll have both landed.
    firm_lifecycle::run_deaths(w, rng, journal);
    // Entry after exit, so a good's vacancy this month reflects everything
    // that died in it. run_entry may append to (and reallocate) w.firms; no
    // reference or pointer into w.firms is held across this call anywhere in
    // this function -- roll_demand below takes its own fresh iterator rather
    // than reusing one from an earlier step.
    firm_lifecycle::run_entry(w, rng, journal);
    roll_demand(w);
}

}  // namespace month_close
