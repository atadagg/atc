#pragma once

#include <cstdint>

#include "ledger.hpp"
#include "rng.hpp"

class World;

namespace daily {

// Who is buying. A tagged index (invariant 3), not a pointer and not a
// subclass (invariant 4): variation arrives as a field and an enum.
enum class BuyerKind : uint8_t { Household, Firm };

struct Buyer {
    BuyerKind kind;
    uint32_t index;
    uint32_t good;  // what it is trying to buy
};

// One buyer's purchasing for one day. A firm buying iron and a household
// buying bread run this same routine; what differs between them is the good's
// commoditization number, not the code.
void buy(World& w, Buyer b, Rng& rng, ledger::Journal* journal = nullptr);

// How many sellers a buyer of this good compares before committing, and how
// readily it abandons a link it already has. Both are the SAME
// commoditization number that drives the price pull -- one dial, three
// channels, so "how commodity-like is this good" has a single answer rather
// than three that can drift apart.
//
// Both are the identity at commoditization 0: one seller compared, zero extra
// switching. That is what keeps the consumer good behaving exactly as it did
// before goods existed.
uint32_t sellers_compared(const World& w, uint32_t good);
double switch_probability(const World& w, uint32_t good);

// One household's shopping for one day.
void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal = nullptr);

// All firms produce lambda * headcount into inventory.
void produce(World& w);

// Shopping (randomized household order) followed by production.
void run_day(World& w, Rng& rng, ledger::Journal* journal = nullptr);

}  // namespace daily
