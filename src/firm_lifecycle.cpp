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

    // A dead firm cannot collect. Its debtors' arrears are cancelled rather
    // than left standing: a payable pointing at a corpse would be settled
    // next month into an account nothing ever empties again (money leaking
    // out of circulation), and in the meantime it would count as a liability
    // against a creditor that no longer exists. Cancelling here is the only
    // reading that keeps both the money stock and the solvency test honest.
    for (uint32_t other = 0; other < w.firms.size(); ++other) {
        if (other == firm) continue;
        Firm& d = w.firms[other];
        if (d.payables <= 0.0) continue;
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
