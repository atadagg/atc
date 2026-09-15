#include "ownership.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "agents.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace ownership {

double total_shares(const Firm& f) {
    double total = 0.0;
    for (std::size_t k = 0; k < f.holders.size(); ++k) {
        if (f.holders[k] == NO_HOUSEHOLD) continue;
        total += f.holder_shares[k];
    }
    return total;
}

double shares_held(const Firm& f, uint32_t household) {
    for (std::size_t k = 0; k < f.holders.size(); ++k) {
        if (f.holders[k] == household) return f.holder_shares[k];
    }
    return 0.0;
}

void deal(Firm& f, uint32_t n_households, const Params& p, Rng& rng) {
    f.holders.fill(NO_HOUSEHOLD);
    f.holder_shares.fill(0.0);

    // Draw order is fixed: the public/private coin first, then the size of
    // the register, then the holders. Every firm consumes the same shape of
    // draw sequence whatever it is dealt.
    f.public_company = rng.chance(p.public_firm_fraction);

    uint32_t count;
    if (f.public_company) {
        count = params::kMinPublicHolders +
                rng.index(params::kMaxHolders - params::kMinPublicHolders + 1);
    } else {
        count = 1u + rng.index(params::kMaxPrivateHolders);
    }
    // A world with fewer households than register slots is a test
    // configuration, not a real one; it still must not deal the same
    // household twice.
    count = std::min(count, n_households);

    for (uint32_t k = 0; k < count; ++k) {
        uint32_t candidate;
        bool duplicate;
        do {
            candidate = rng.index(n_households);
            duplicate = false;
            for (uint32_t prior = 0; prior < k; ++prior) {
                if (f.holders[prior] == candidate) { duplicate = true; break; }
            }
        } while (duplicate);
        f.holders[k] = candidate;
    }

    // Holder k of n takes weight skew^k. The holders were drawn at random, so
    // rank is not a property of the household -- being dealt first is luck.
    // What the rule fixes is the *shape* of the distribution, which is the
    // thing `share_skew` is a dial on.
    double weight = 1.0;
    double total_weight = 0.0;
    for (uint32_t k = 0; k < count; ++k) {
        f.holder_shares[k] = weight;
        total_weight += weight;
        weight *= p.share_skew;
    }
    if (total_weight <= 0.0) return;

    // Normalised to kSharesPerFirm so that "shares" is a comparable unit
    // across firms, and a per-share profit is a number worth reading.
    for (uint32_t k = 0; k < count; ++k) {
        f.holder_shares[k] = kSharesPerFirm * (f.holder_shares[k] / total_weight);
    }
}

double pay_holders(World& w, uint32_t firm, double pot, ledger::Reason reason,
                   ledger::Journal* journal) {
    if (pot <= 0.0) return 0.0;

    Firm& f = w.firms[firm];
    const double total = total_shares(f);
    if (total <= 0.0) return 0.0;

    const double target = f.liquidity - pot;
    if (target < 0.0) {
        throw std::logic_error("ownership::pay_holders: firm cannot cover the pot");
    }

    // Index of the last occupied slot: it takes the exact remainder, so the
    // firm lands on `target` rather than on a sum of separately rounded
    // shares. Finite arithmetic means those shares may overshoot the pot in
    // either direction; each payment is capped by what the firm can actually
    // part with, which is what stops a negative final remainder reaching
    // `transfer` as an invalid amount, and an outright overdraw of an account
    // whose target balance is zero.
    std::size_t last = 0;
    for (std::size_t k = 0; k < f.holders.size(); ++k) {
        if (f.holders[k] != NO_HOUSEHOLD) last = k;
    }

    double paid = 0.0;
    for (std::size_t k = 0; k < f.holders.size(); ++k) {
        if (f.holders[k] == NO_HOUSEHOLD) continue;
        const double available = f.liquidity - target;
        if (available <= 0.0) break;
        const double want = (k == last) ? available : pot * (f.holder_shares[k] / total);
        const double amount = std::min(want, available);
        if (amount <= 0.0) continue;
        ledger::transfer(w, ledger::firm(firm), ledger::household(f.holders[k]), amount,
                         reason, journal);
        paid += amount;
    }

    // Absorbing a crumb is correct; absorbing a real discrepancy is not.
    // Anything further out than float noise is a bug in the shares.
    const double tolerance = 1e-9 * std::max(1.0, std::abs(target));
    if (std::abs(f.liquidity - target) > tolerance) {
        throw std::logic_error("ownership::pay_holders: payout did not land on its target");
    }
    return paid;
}

}  // namespace ownership
