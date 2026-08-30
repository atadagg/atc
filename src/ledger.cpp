#include "ledger.hpp"

#include <cmath>
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

}  // namespace ledger
