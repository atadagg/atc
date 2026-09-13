#include "firm_lifecycle.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "agents.hpp"
#include "balance_sheet.hpp"
#include "ledger.hpp"
#include "network.hpp"
#include "params.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace firm_lifecycle {

namespace {

// Where a dying firm's unclaimed cash goes. One issuer exists today.
constexpr uint32_t kIssuer = 0;

}  // namespace

void liquidate(World& w, uint32_t firm, ledger::Journal* journal) {
    Firm& f = w.firms[firm];
    if (!f.alive) return;

    // Release the workforce first, so the payroll a dead firm can no longer
    // meet does not exist. `network::fire` mutates f.employees, so iterate a
    // copy.
    const std::vector<uint32_t> staff = f.employees;
    for (uint32_t h : staff) {
        if (w.households[h].employer == firm) network::fire(w, h);
    }
    f.employees.clear();
    f.has_open_vacancy = false;
    f.vacancy_went_unfilled = false;
    f.months_fully_staffed = 0;
    f.pending_fire = NO_HOUSEHOLD;

    // Money owed TO a dying firm is one of its ASSETS, so it is COLLECTED
    // before anything is paid out, not forgiven. Forgiving it would make a
    // firm's death leave its debtors better off -- contagion running
    // backwards, the opposite of what this module exists to produce.
    //
    // Each debtor pays what it can: capped at its own cash, so no non-issuer
    // is pushed below zero. A debtor drained here may itself become
    // insolvent; that is the cascade travelling DOWNSTREAM, and the next pass
    // of `run_deaths` picks it up.
    for (uint32_t other = 0; other < w.firms.size(); ++other) {
        if (other == firm) continue;
        Firm& d = w.firms[other];
        if (!d.alive || d.payables <= 0.0) continue;
        for (std::size_t s = 0; s < d.payable_to.size(); ++s) {
            if (d.payable_to[s] != firm) continue;
            const double pay = std::min(d.payable_amount[s], d.liquidity);
            if (pay > 0.0) {
                ledger::transfer(w, ledger::firm(other), ledger::firm(firm), pay,
                                 ledger::Reason::FirmLiquidation, journal);
                d.payable_amount[s] -= pay;
                d.payables -= pay;
                if (d.payables < 0.0) d.payables = 0.0;
            }
        }
    }

    // Residual cash to creditors, pro-rata by what each is owed. Whatever a
    // creditor is not paid stays unpaid: its receivable is removed from its
    // books in full, so the shortfall lands on its equity as a write-off.
    // That write-off is what can kill the creditor in the next pass.
    double owed = 0.0;
    for (std::size_t s = 0; s < f.payable_amount.size(); ++s) {
        if (f.payable_to[s] == NO_FIRM) continue;
        if (f.payable_amount[s] > 0.0) owed += f.payable_amount[s];
    }

    if (owed > 0.0) {
        const double pot = f.liquidity;
        for (std::size_t s = 0; s < f.payable_amount.size(); ++s) {
            const uint32_t creditor = f.payable_to[s];
            const double bill = f.payable_amount[s];
            f.payable_to[s] = NO_FIRM;
            f.payable_amount[s] = 0.0;
            if (creditor == NO_FIRM || bill <= 0.0) continue;

            // Capped at what the account actually holds: rounding in the
            // pro-rata share must never reach `transfer` as an overdraft.
            const double pay = std::min(pot * (bill / owed), f.liquidity);
            if (pay > 0.0) {
                ledger::transfer(w, ledger::firm(firm), ledger::firm(creditor), pay,
                                 ledger::Reason::FirmLiquidation, journal);
            }
            Firm& c = w.firms[creditor];
            c.receivables -= bill;
            if (c.receivables < 0.0) c.receivables = 0.0;
        }
    } else {
        f.payable_to.fill(NO_FIRM);
        f.payable_amount.fill(0.0);
    }

    if (f.liquidity > 0.0) {
        // PLACEHOLDER until piece 2. A dying firm's residual cash should go to
        // its shareholders, and shareholders do not exist yet. Routing it to
        // the issuer keeps invariant 1 exact -- the money leaves circulation
        // rather than vanishing -- and Task 12's founding draws from the same
        // account, so entry and exit are symmetric. Piece 2 replaces both ends
        // with real owners.
        ledger::transfer(w, ledger::firm(firm), ledger::issuer(kIssuer), f.liquidity,
                         ledger::Reason::FirmLiquidation, journal);
    }

    // Only what could NOT be collected is cancelled -- a debtor that was out
    // of cash above. This runs last so that everything collectable has
    // already entered the pot and been paid on to the creditors.
    //
    // It is load-bearing, not tidying: after it, no living firm holds a
    // `payable_to` pointing at a corpse, so no later settlement can transfer
    // cash into a dead firm's account, where nothing would ever empty it
    // again. It also stops a liability to a creditor that no longer exists
    // from counting against the debtor's solvency.
    for (uint32_t other = 0; other < w.firms.size(); ++other) {
        if (other == firm) continue;
        Firm& d = w.firms[other];
        for (std::size_t s = 0; s < d.payable_to.size(); ++s) {
            if (d.payable_to[s] != firm) continue;
            d.payables -= d.payable_amount[s];
            d.payable_to[s] = NO_FIRM;
            d.payable_amount[s] = 0.0;
        }
        if (d.payables < 0.0) d.payables = 0.0;
    }

    // The books are closed: no stock, no claims either way. Equity is
    // computed, so zeroing the terms is what makes a dead firm's sheet empty.
    f.inventory = 0.0;
    f.output_cost_basis = 0.0;
    f.input_stock.fill(0.0);
    f.input_cost_basis.fill(0.0);
    f.input_demand.fill(0.0);
    f.input_unsatisfied.fill(0.0);
    f.suppliers.fill(NO_FIRM);
    f.payables = 0.0;
    f.receivables = 0.0;
    f.demand_this_month = 0.0;
    f.demand_last_month = 0.0;

    f.alive = false;
}

void run_deaths(World& w, Rng& rng, ledger::Journal* journal) {
    (void)rng;  // see the header: liquidation becomes random once owners exist

    // Fixed point. A death writes off a supplier's receivable, which can make
    // THAT supplier insolvent, which writes off one further up. Each pass
    // resolves one link of the chain, and a pass that kills nobody means the
    // chain has run out. Every pass kills at least one firm or is the last,
    // so w.firms.size() passes is a hard bound on a world of that many firms
    // -- termination does not rest on the economics converging.
    const std::size_t bound = w.firms.size();
    for (std::size_t pass = 0; pass < bound; ++pass) {
        bool killed = false;
        for (uint32_t fi = 0; fi < w.firms.size(); ++fi) {
            if (!w.firms[fi].alive) continue;
            if (!balance_sheet::of(w, fi).insolvent()) continue;
            liquidate(w, fi, journal);
            killed = true;
        }
        if (!killed) return;
    }
}

}  // namespace firm_lifecycle
