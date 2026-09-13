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

// One household's shopping for one day.
void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal = nullptr);

// All firms produce lambda * headcount into inventory.
void produce(World& w);

// Shopping (randomized household order) followed by production.
void run_day(World& w, Rng& rng, ledger::Journal* journal = nullptr);

}  // namespace daily
