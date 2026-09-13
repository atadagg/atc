#include "goods.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace goods {
namespace {

// Depth-first cycle detection. 0 = unvisited, 1 = on the current path,
// 2 = finished.
void visit(const std::vector<Good>& g, uint32_t i, std::vector<uint8_t>& state) {
    if (state[i] == 1) throw std::invalid_argument("goods: recipe graph has a cycle");
    if (state[i] == 2) return;
    state[i] = 1;
    for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
        uint32_t in = g[i].inputs[s];
        if (in == NO_GOOD) continue;
        if (in >= g.size()) throw std::invalid_argument("goods: input index out of range");
        visit(g, in, state);
    }
    state[i] = 2;
}

}  // namespace

void assert_acyclic(const std::vector<Good>& g) {
    std::vector<uint8_t> state(g.size(), 0);
    for (uint32_t i = 0; i < g.size(); ++i) visit(g, i, state);
}

void recompute(std::vector<Good>& g, double k) {
    assert_acyclic(g);

    bool any_consumer = false;
    for (const Good& x : g) any_consumer = any_consumer || x.consumer;
    if (!any_consumer) {
        throw std::invalid_argument("goods: no consumer good to measure distance to");
    }

    // `remove` is the LONGEST path from a good to a consumer good -- not the
    // shortest. A good that feeds both a deep chain and a shallow one is
    // still an upstream commodity; taking the shortest path would call it
    // consumer-facing on the strength of its least typical buyer.
    //
    // Relaxation rather than a topological sort: the graph is tiny (tens of
    // goods) and acyclicity is already established, so |V| passes always
    // converge and the code stays short enough to be obviously right.
    const int unset = -1;
    std::vector<int> rem(g.size(), unset);
    for (uint32_t i = 0; i < g.size(); ++i) {
        if (g[i].consumer) rem[i] = 0;
    }

    for (std::size_t pass = 0; pass < g.size(); ++pass) {
        for (uint32_t i = 0; i < g.size(); ++i) {
            for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
                uint32_t in = g[i].inputs[s];
                if (in == NO_GOOD) continue;
                if (rem[i] == unset) continue;
                int candidate = rem[i] + 1;
                if (candidate > rem[in]) rem[in] = candidate;
            }
        }
    }

    for (uint32_t i = 0; i < g.size(); ++i) {
        // A good that reaches no consumer good is dead stock: nothing it can
        // become is ever bought. Treat it as maximally upstream rather than
        // failing the run -- innovation can strand a branch.
        int r = (rem[i] == unset) ? 255 : rem[i];
        if (r > 255) r = 255;
        g[i].remove = static_cast<uint8_t>(r);

        // 1 - exp(-k * remove): zero at the consumer, rising and saturating
        // upstream. Never reaches 1, so even ore keeps a sliver of pricing
        // discretion.
        g[i].commoditization = 1.0 - std::exp(-k * static_cast<double>(g[i].remove));
    }
}

}  // namespace goods
