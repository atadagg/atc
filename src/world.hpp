#pragma once

#include <vector>

#include "agents.hpp"
#include "params.hpp"
#include "rng.hpp"

// Owns all agent state. The single source of truth for the simulation.
class World {
public:
    // Throws std::invalid_argument if n_households == 0 or n_firms == 0.
    // Shop distinctness is preserved when n_firms >= kShopsPerHousehold (7);
    // with fewer firms, shops may repeat (test-only configuration).
    World(uint32_t n_households, uint32_t n_firms, Rng& rng);

    std::vector<Firm> firms;
    std::vector<Household> households;

    // Behavioural parameters for this run. Runtime state rather than
    // `constexpr` so they can change mid-run; carried by `SimSnapshot`
    // because it copies the whole World, which is what keeps a backward
    // scrub correct across a parameter change. See params.hpp.
    Params params;

    // Master invariant: this must not change across a tick.
    double total_money() const;
};
