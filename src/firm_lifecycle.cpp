#include "firm_lifecycle.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "agents.hpp"
#include "balance_sheet.hpp"
#include "goods.hpp"
#include "ledger.hpp"
#include "market.hpp"
#include "network.hpp"
#include "params.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace firm_lifecycle {

namespace {

// Where a dying firm's unclaimed cash goes, and where a founding firm's
// opening capital comes from. One issuer exists today.
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

namespace {

// The economy-wide average wage among living firms, standing in for the wage
// an as-yet-nonexistent firm would pay. No living firm means no wage signal
// to cost a new one against, so entry cannot proceed at all -- that case is
// handled by the caller returning false and declining every good this call.
bool prevailing_wage(const World& w, double& out) {
    double sum = 0.0;
    uint32_t count = 0;
    for (const Firm& f : w.firms) {
        if (!f.alive) continue;
        sum += f.wage;
        ++count;
    }
    if (count == 0) return false;
    out = sum / static_cast<double>(count);
    return true;
}

// Labour at `wage`, scaled by the recipe's labour_per_unit exactly the way
// firm_decisions::marginal_cost scales it by 1 (today's only good has
// labour_per_unit == 1, so the two formulas agree on the legacy economy),
// plus each input at its published reference price.
//
// Returns false -- decline to cost -- the moment an input has no published
// reference price. A firm cannot be founded against a price that does not
// exist yet; declining is the safe answer, not an approximation of one.
bool unit_cost(const World& w, uint32_t good, double wage, double& out) {
    const Good& g = w.goods[good];
    double cost = g.labour_per_unit * wage / (w.params.productivity * params::kDaysPerMonth);
    for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
        if (g.inputs[s] == NO_GOOD) continue;
        if (g.input_qty[s] <= 0.0) continue;
        const market::GoodMarket& m = w.markets[g.inputs[s]];
        if (!m.has_reference) return false;
        cost += g.input_qty[s] * m.reference_price;
    }
    out = cost;
    return true;
}

}  // namespace

void run_entry(World& w, Rng& rng, ledger::Journal* journal) {
    // Off by default, and this check runs before any draw at all: a world
    // that never raises firm_entry_prob takes exactly the RNG draws it always
    // took, so every existing golden run is untouched by this feature merely
    // existing.
    if (w.params.firm_entry_prob <= 0.0) return;

    double wage;
    if (!prevailing_wage(w, wage)) return;  // no living firm to read a wage from

    for (uint32_t g = 0; g < w.goods.size(); ++g) {
        // Producers are "scarce" for a good when fewer of the firms ever
        // assigned to it are still alive than were assigned in the first
        // place -- i.e. a firm that used to make this good has died and left
        // a vacancy. `Firm::good` never changes once set, even in death (see
        // liquidate(), which zeroes a corpse's books but not its good), so
        // this count is stable across the whole run.
        //
        // A good nobody has ever been assigned to (total == 0, e.g. a good
        // added to the graph but never populated) is deliberately NOT
        // treated as scarce: entry here replaces a dead incumbent, it does
        // not seed a producer for a good that never had one.
        uint32_t total = 0;
        uint32_t living = 0;
        for (const Firm& f : w.firms) {
            if (f.good != g) continue;
            ++total;
            if (f.alive) ++living;
        }
        if (living >= total) continue;

        const market::GoodMarket& m = w.markets[g];
        if (!m.has_reference) continue;

        double cost;
        if (!unit_cost(w, g, wage, cost)) continue;
        if (m.reference_price <= cost * w.params.firm_entry_margin) continue;

        if (!rng.chance(w.params.firm_entry_prob)) continue;

        Firm nf;
        nf.alive = true;
        nf.good = g;
        nf.price = m.reference_price;
        nf.wage = wage;
        nf.suppliers.fill(NO_FIRM);
        nf.payable_to.fill(NO_FIRM);
        // Every other array (input stock/demand/unsatisfied, cost bases,
        // payable amounts) and every other scalar (inventory, payables,
        // receivables, demand history, vacancy/fire bookkeeping) is already
        // zero or its Firm default -- see agents.hpp.

        // Appending may reallocate w.firms, so the index -- not a reference
        // -- is what the funding transfer below addresses.
        const uint32_t index = static_cast<uint32_t>(w.firms.size());
        w.firms.push_back(nf);

        // PLACEHOLDER until piece 2, symmetric with liquidate()'s residual
        // cash going to the issuer above: a founder's opening capital should
        // come from shareholders buying equity, and shareholders do not
        // exist yet. Routing it through the issuer keeps invariant 1 exact --
        // the money enters circulation through the same account liquidation
        // returns it to -- and piece 2 replaces both ends with real owners.
        ledger::transfer(w, ledger::issuer(kIssuer), ledger::firm(index),
                         w.params.firm_entry_capital, ledger::Reason::FirmFounding,
                         journal);
    }
}

}  // namespace firm_lifecycle
