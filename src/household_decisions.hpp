#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace household_decisions {

double average_shop_price(const World& w, uint32_t household);

void search_cheaper_shop(World& w, uint32_t household, Rng& rng);
void search_reliable_shop(World& w, uint32_t household, Rng& rng);
void search_job(World& w, uint32_t household, Rng& rng);
void set_consumption_budget(World& w, uint32_t household);

// Runs all four for every household, in a randomized order.
void run_monthly(World& w, Rng& rng);

}  // namespace household_decisions
