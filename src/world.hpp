#pragma once

#include <vector>

#include "agents.hpp"
#include "rng.hpp"

// Owns all agent state. The single source of truth for the simulation.
class World {
public:
    World(uint32_t n_households, uint32_t n_firms, Rng& rng);

    std::vector<Firm> firms;
    std::vector<Household> households;

    // Master invariant: this must not change across a tick.
    double total_money() const;
};
