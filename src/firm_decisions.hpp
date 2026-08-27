#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace firm_decisions {

// Cost of producing one unit: one worker costs `wage` per month and yields
// lambda * days_per_month units, so marginal cost is independent of headcount.
double marginal_cost(const Firm& f);

void adjust_wage(Firm& f, Rng& rng);
void adjust_workforce(World& w, uint32_t firm, Rng& rng);
void adjust_price(Firm& f, Rng& rng);

// Runs the three rules above for every firm, in index order.
void run_monthly(World& w, Rng& rng);

}  // namespace firm_decisions
