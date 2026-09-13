#pragma once

#include <array>
#include <cstdint>

#include "params.hpp"

inline constexpr uint32_t NO_GOOD = 0xFFFFFFFFu;

// A recipe. Fixed-capacity members only: Good lives in a vector on World,
// which SimSnapshot copies by plain assignment (invariants 3 and 4).
//
// `remove` and `commoditization` are DERIVED from the graph, not authored.
// goods.cpp recomputes both whenever the graph changes. Never set by hand.
struct Good {
    std::array<uint32_t, params::kMaxInputs> inputs{};    // good indices, NO_GOOD padded
    std::array<double, params::kMaxInputs> input_qty{};   // per unit of output
    double labour_per_unit = 1.0;
    bool consumer = false;

    // Longest path to a consumer good. Consumer goods are 0; ore feeding a
    // four-deep chain is 3. Counted from the consumer end, not the raw end,
    // because distance from the consumer is the distance over which a
    // producer's choices stop being visible to anyone -- and because it makes
    // today's single good land at 0, so the current economy needs no
    // exception carved for it.
    uint8_t remove = 0;

    // In [0,1], rising with `remove`. Drives price-taking, how many sellers a
    // buyer compares, and supplier stickiness. At 0 every existing rule runs
    // unchanged.
    double commoditization = 0.0;
};

#include <vector>

namespace goods {

// Throws std::invalid_argument if the recipe graph contains a cycle.
// The graph must be a DAG: a good cannot be an input to making itself.
void assert_acyclic(const std::vector<Good>& goods);

// Recomputes `remove` and `commoditization` for every good. Call after any
// change to the graph. Throws std::invalid_argument on a cycle, or if no good
// is marked `consumer` (there is then nothing to measure distance to).
void recompute(std::vector<Good>& goods, double k);

}  // namespace goods
