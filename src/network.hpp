#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace network {

// Employ `household` at `firm`, detaching it from any previous employer.
void hire(World& w, uint32_t household, uint32_t firm);

// Unemploy `household`. Safe to call on an already-unemployed household.
void fire(World& w, uint32_t household);

// Point shop slot `slot` at `new_firm`, clearing that slot's unmet demand.
void replace_shop(World& w, uint32_t household, uint32_t slot, uint32_t new_firm);

bool has_shop(const Household& h, uint32_t firm);

// Draw a firm the household does not already shop at, with probability
// proportional to firm size in employees. Firms with zero employees are
// still reachable (weight 1) so they are not permanently excluded.
// Returns NO_FIRM if every firm is already connected.
uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng);

}  // namespace network
