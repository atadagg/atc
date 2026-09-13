#include "world.hpp"

#include <algorithm>
#include <stdexcept>

World::World(uint32_t n_households, uint32_t n_firms, Rng& rng) {
    if (n_households == 0) {
        throw std::invalid_argument("World: n_households must be > 0");
    }
    if (n_firms == 0) {
        throw std::invalid_argument("World: n_firms must be > 0");
    }

    // One issuer account, balance zero, representing what is outside the
    // modelled economy. Lengnick v1 never touches it, so the money supply is
    // constant -- but the account exists, so an extension that creates money
    // (a bank, a government) needs no new machinery, and a money injection is
    // an ordinary balanced transfer rather than an exception to invariant 1.
    issuers.assign(1, 0.0);

    // Create the single consumer good that represents today's economy in the
    // new vocabulary.
    Good consumer;
    consumer.inputs.fill(NO_GOOD);
    consumer.input_qty.fill(0.0);
    consumer.labour_per_unit = 1.0;
    consumer.consumer = true;
    consumer.remove = 0;
    consumer.commoditization = 0.0;
    goods.push_back(consumer);

    markets.resize(goods.size());

    firms.resize(n_firms);
    households.resize(n_households);

    // An empty supplier slot is NO_FIRM, not firm 0.
    for (Firm& f : firms) {
        f.suppliers.fill(NO_FIRM);
        f.payable_to.fill(NO_FIRM);
    }

    for (uint32_t h = 0; h < n_households; ++h) {
        Household& hh = households[h];

        // Assign shop connections. Distinctness is preserved when n_firms >= 7;
        // for smaller test worlds with fewer firms, shop slots may repeat.
        const uint32_t distinct_target = std::min<uint32_t>(params::kShopsPerHousehold, n_firms);
        for (uint32_t s = 0; s < params::kShopsPerHousehold; ++s) {
            if (s < distinct_target) {
                // Draw without replacement while pool is large enough.
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
            } else {
                // Pool is exhausted; accept repeats in small-world test cases.
                hh.shops[s] = rng.index(n_firms);
            }
        }

        // Start fully employed, spread evenly across firms. Unemployment must
        // emerge from the dynamics, not from the initial conditions.
        uint32_t f = h % n_firms;
        hh.employer = f;
        firms[f].employees.push_back(h);
    }
}

double World::money_in_circulation() const {
    double total = 0.0;
    for (const auto& f : firms) total += f.liquidity;
    for (const auto& h : households) total += h.liquidity;
    return total;
}

double World::books_balance() const {
    double total = money_in_circulation();
    for (double balance : issuers) total += balance;
    return total;
}
