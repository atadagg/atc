#include "world.hpp"

#include <numeric>

World::World(uint32_t n_households, uint32_t n_firms, Rng& rng) {
    firms.resize(n_firms);
    households.resize(n_households);

    for (uint32_t h = 0; h < n_households; ++h) {
        Household& hh = households[h];

        // Seven distinct shop connections, drawn without replacement.
        // The initial network is scaffolding only; it is expected to wash
        // out during burn-in.
        for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
            uint32_t candidate;
            bool duplicate;
            do {
                candidate = rng.index(n_firms);
                duplicate = false;
                for (uint32_t prior = 0; prior < s; ++prior) {
                    if (hh.shops[prior] == candidate) { duplicate = true; break; }
                }
            } while (duplicate);
            hh.shops[s] = candidate;
        }

        // Start fully employed, spread evenly across firms. Unemployment must
        // emerge from the dynamics, not from the initial conditions.
        uint32_t f = h % n_firms;
        hh.employer = f;
        firms[f].employees.push_back(h);
    }
}

double World::total_money() const {
    double total = 0.0;
    for (const auto& f : firms) total += f.liquidity;
    for (const auto& h : households) total += h.liquidity;
    return total;
}
