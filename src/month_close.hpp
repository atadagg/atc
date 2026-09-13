#pragma once

#include "ledger.hpp"

class World;
class Rng;

namespace month_close {

// Pay every outstanding supplier bill. Firms are settled in a randomised
// order: with a fixed order, low-indexed firms would be paid first every
// month for the life of the run, which is a systematic advantage nobody
// chose. This is why month_close::run now takes an Rng& -- CLAUDE.md listed
// that as a deferred item and this is where it comes due.
//
// A firm short of cash pays what it can, pro-rata across its creditors; the
// remainder stays outstanding.
//
// Consumes no randomness at all when no firm owes anything, which is the
// whole of the single-good world -- otherwise every existing run's draw
// sequence would shift.
void settle_trade_credit(World& w, Rng& rng, ledger::Journal* journal = nullptr);

void pay_wages(World& w, ledger::Journal* journal = nullptr);
void distribute_profits(World& w, ledger::Journal* journal = nullptr);
void update_reservation_wages(World& w);
void apply_pending_fires(World& w);
void roll_demand(World& w);

// The full end-of-month sequence, in order.
void run(World& w, Rng& rng, ledger::Journal* journal = nullptr);

}  // namespace month_close
