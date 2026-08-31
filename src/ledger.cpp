#include "ledger.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace ledger {
namespace {

double& liquidity_of(World& w, Account a) {
    switch (a.kind) {
        case Kind::Firm:
            if (a.index >= w.firms.size()) {
                throw std::out_of_range("ledger::transfer: no such firm");
            }
            return w.firms[a.index].liquidity;
        case Kind::Household:
            if (a.index >= w.households.size()) {
                throw std::out_of_range("ledger::transfer: no such household");
            }
            return w.households[a.index].liquidity;
        case Kind::Issuer:
            break;
    }
    if (a.index >= w.issuers.size()) {
        throw std::out_of_range("ledger::transfer: no such issuer");
    }
    return w.issuers[a.index];
}

}  // namespace

void transfer(World& w, Account from, Account to, double amount, Reason reason,
              Journal* journal) {
    if (!std::isfinite(amount) || amount < 0.0) {
        throw std::invalid_argument("ledger::transfer: amount must be finite and non-negative");
    }
    double& src = liquidity_of(w, from);
    double& dst = liquidity_of(w, to);
    if (!may_issue(from.kind) && src < amount) {
        throw Overdraft("ledger::transfer: a non-issuer cannot be overdrawn");
    }
    src -= amount;
    dst += amount;
    if (journal) journal->record(from, to, amount, reason);
}

double flow_discrepancy(const MonthFlows& f, double household_liquidity_before,
                        double household_liquidity_after) {
    const double net_receipts = f.household_in() - f.household_out();
    const double change_in_stock = household_liquidity_after - household_liquidity_before;
    return net_receipts - change_in_stock;
}

void check_flows(const MonthFlows& f, double household_liquidity_before,
                 double household_liquidity_after, double scale) {
    const double d = flow_discrepancy(f, household_liquidity_before, household_liquidity_after);
    // Relative to the stock, floored at 1 so a tiny or zero `scale` cannot make
    // the tolerance vanish and turn every rounding crumb into an abort.
    const double tolerance = 1e-9 * std::max(1.0, std::abs(scale));
    if (std::abs(d) > tolerance) {
        throw FlowMismatch(std::format(
            "ledger::check_flows: month {} flows do not account for the change in the "
            "household stock: discrepancy {}, tolerance {}",
            f.month, d, tolerance));
    }
}

}  // namespace ledger
