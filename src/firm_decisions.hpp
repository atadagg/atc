#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace firm_decisions {

// Cost of producing one unit: one worker costs `wage` per month and yields
// lambda * days_per_month units, so marginal cost is independent of headcount.
double marginal_cost(const Firm& f);

// The inventory comfort band computed from last month's demand:
// [kInventoryLowerFactor * demand_last_month, kInventoryUpperFactor *
// demand_last_month]. Shared by adjust_workforce and adjust_price so the two
// rules can never silently drift apart (spec §5.1).
struct InventoryBand {
    double lower;
    double upper;
};
InventoryBand inventory_band(const Firm& f);

void adjust_wage(Firm& f, Rng& rng);
void adjust_workforce(World& w, uint32_t firm, Rng& rng);
void adjust_price(Firm& f, Rng& rng);

// Runs the three rules above for every firm, in index order.
void run_monthly(World& w, Rng& rng);

}  // namespace firm_decisions
