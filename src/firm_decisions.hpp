#pragma once

#include <cstdint>

#include "agents.hpp"
#include "params.hpp"
#include "rng.hpp"

class World;

namespace firm_decisions {

// Cost of producing one unit: one worker costs `wage` per month and yields
// lambda * days_per_month units, so marginal cost is independent of headcount.
double marginal_cost(const Firm& f, const Params& p);

// The inventory comfort band computed from last month's demand:
// [inventory_lower_factor * demand_last_month, inventory_upper_factor *
// demand_last_month]. Shared by adjust_workforce and adjust_price so the two
// rules can never silently drift apart (spec §5.1).
struct InventoryBand {
    double lower;
    double upper;
};
InventoryBand inventory_band(const Firm& f, const Params& p);

void adjust_wage(Firm& f, const Params& p, Rng& rng);
void adjust_workforce(World& w, uint32_t firm, Rng& rng);
void adjust_price(Firm& f, const Params& p, Rng& rng);

// The commoditization pull. The firm's own gated rule above (raise only
// while below the cap, lower only while above the floor -- see CLAUDE.md, it
// is gated rather than clamped) decides what the firm WANTS. This blends
// that toward what the good actually trades at, weighted by how far the good
// sits from the consumer.
//
// At 0 this is the identity and today's economy is untouched. At 1 the firm
// is a pure price taker: iron ore sells at the iron ore price whatever its
// producer would prefer. Nothing here is an auctioneer -- the reference is
// last month's realised average, and a firm at the reference price can still
// fail to sell.
void pull_toward_reference(Firm& f, const World& w);

// Runs the three rules above for every firm, in index order.
void run_monthly(World& w, Rng& rng);

}  // namespace firm_decisions
