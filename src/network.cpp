#include "network.hpp"

#include <algorithm>
#include <vector>

#include "world.hpp"

namespace network {

void hire(World& w, uint32_t household, uint32_t firm) {
    fire(w, household);
    w.households[household].employer = firm;
    w.firms[firm].employees.push_back(household);
}

void fire(World& w, uint32_t household) {
    uint32_t f = w.households[household].employer;
    if (f == NO_FIRM) return;
    auto& e = w.firms[f].employees;
    e.erase(std::remove(e.begin(), e.end(), household), e.end());
    w.households[household].employer = NO_FIRM;
}

void replace_shop(World& w, uint32_t household, uint32_t slot, uint32_t new_firm) {
    w.households[household].shops[slot] = new_firm;
    w.households[household].unsatisfied[slot] = 0.0;
}

bool has_shop(const Household& h, uint32_t firm) {
    return std::find(h.shops.begin(), h.shops.end(), firm) != h.shops.end();
}

uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng) {
    const Household& h = w.households[household];

    double total_weight = 0.0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (has_shop(h, f)) continue;
        total_weight += 1.0 + static_cast<double>(w.firms[f].employees.size());
    }
    if (total_weight <= 0.0) return NO_FIRM;

    double draw = rng.uniform(0.0, total_weight);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (has_shop(h, f)) continue;
        draw -= 1.0 + static_cast<double>(w.firms[f].employees.size());
        if (draw <= 0.0) return f;
    }
    return NO_FIRM;  // unreachable except for floating-point edge cases
}

uint32_t random_unconnected_firm(const World& w, uint32_t household, Rng& rng) {
    const Household& h = w.households[household];

    uint32_t n = 0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!has_shop(h, f)) ++n;
    }
    if (n == 0) return NO_FIRM;

    uint32_t pick = rng.index(n);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (has_shop(h, f)) continue;
        if (pick == 0) return f;
        --pick;
    }
    return NO_FIRM;  // unreachable
}

}  // namespace network
