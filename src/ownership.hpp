#pragma once

#include <cstdint>

#include "ledger.hpp"
#include "params.hpp"

class World;
class Rng;
struct Firm;

// Shares, and the payments that flow along them.
//
// This module exists to sever a triple duty `liquidity` used to carry: the
// wallet, the ownership stake profits were weighted by, and the appetite. When
// profit shares are proportional to cash, buying groceries sells your stake
// and whoever consumes least owns most -- a loop that compounds monthly with
// nothing opposing it. Shares are the thing that does not move when you spend.
//
// Holding only. Nobody trades a share yet: valuing one needs an answer to
// "what makes a household buy, and at what price" in this model's own
// backward-looking vocabulary, and that is a separate problem.
namespace ownership {

// Every firm is dealt this many shares. A pure unit of account: what matters
// is each holder's fraction, so the absolute number only has to be the same
// everywhere for "shares" to be comparable across firms.
inline constexpr double kSharesPerFirm = 100.0;

// Shares outstanding in a firm. A sum over the register, never stored.
double total_shares(const Firm& f);

// What one household holds in one firm. Zero if it holds nothing.
double shares_held(const Firm& f, uint32_t household);

// Deals `f`'s register: whether it is a public company, who holds it, and how
// much each holds. Drawn from households [0, n_households). Called once per
// firm, at construction or at founding -- never again.
//
// Always consumes the same draws for the same firm regardless of outcome, so
// that it cannot desynchronise a run.
void deal(Firm& f, uint32_t n_households, const Params& p, Rng& rng);

// Pays `pot` out of the firm's account to its holders, in proportion to their
// shares. The firm must hold at least `pot`.
//
// The last holder takes the exact remainder, so the firm lands on precisely
// `liquidity - pot` rather than on the sum of separately rounded shares --
// the same discipline pay_wages uses. Throws std::logic_error if it does not.
//
// Returns what was paid. A firm with no holders pays nothing and keeps `pot`:
// there is nobody to receive it, and destroying it would break invariant 1.
double pay_holders(World& w, uint32_t firm, double pot, ledger::Reason reason,
                   ledger::Journal* journal);

}  // namespace ownership
