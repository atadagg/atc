#pragma once

#include <cstdint>

class World;
class Rng;

namespace ledger {
class Journal;
}

// Firm exit. A firm whose equity has gone negative (balance_sheet::insolvent)
// stops trading: its workers are released, its residual cash is handed to its
// creditors, and whatever it could not pay becomes those creditors' write-off.
//
// That write-off is the point. A supplier can be solvent only because of a
// receivable, and lose solvency the moment the receivable evaporates. So
// deaths are resolved to a FIXED POINT rather than in one pass -- the second
// pass is the contagion travelling one link up the chain, the third is it
// travelling two.
namespace firm_lifecycle {

// Winds up one firm. Conserves money exactly: every movement goes through
// ledger::transfer, and what no creditor claims goes to issuer 0 rather than
// being deleted. Idempotent-safe: a firm already dead is left alone.
void liquidate(World& w, uint32_t firm, ledger::Journal* journal = nullptr);

// Kills every insolvent firm, then every firm made insolvent by those deaths,
// until a pass kills nobody. Bounded by w.firms.size() passes, so termination
// does not depend on the economics.
//
// Consumes no randomness when nothing dies; `rng` is taken because
// liquidation will need it once shareholders exist (see the PLACEHOLDER note
// in the .cpp).
void run_deaths(World& w, Rng& rng, ledger::Journal* journal = nullptr);

// Firm entry: the other half of the lifecycle. A good whose reference price
// pays well above what it costs to make attracts a new entrant, up to
// `Params::firm_entry_max_per_good` LIVING producers of that good and
// `Params::firm_entry_max_per_month` new firms across all goods combined.
//
// The gate is a living headcount, not "fewer than were ever assigned to this
// good" -- `Firm::good` survives death, so counting corpses either blocks a
// good from ever attracting its first producer (0 assigned == 0 dead) or, for
// a good that has lost one, never closes at all, since founding a
// replacement raises the corpse-inclusive count too. Both were measured, not
// merely suspected.
//
// Returns immediately -- before touching `rng` at all -- when
// `Params::firm_entry_prob` is <= 0.0. That is what keeps the legacy
// single-good economy's draw sequence, and therefore every existing golden
// run, untouched by this feature's mere existence.
//
// Appending to `w.firms` may reallocate the vector. Callers must not hold a
// reference or pointer into `w.firms` across this call -- see the audit note
// in month_close::run.
void run_entry(World& w, Rng& rng, ledger::Journal* journal = nullptr);

}  // namespace firm_lifecycle
