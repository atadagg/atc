#pragma once

#include <cstdint>

#include "rng.hpp"

class World;

namespace daily {

// One household's shopping for one day.
void shop(World& w, uint32_t household, Rng& rng);

// All firms produce lambda * headcount into inventory.
void produce(World& w);

// Shopping (randomized household order) followed by production.
void run_day(World& w, Rng& rng);

}  // namespace daily
