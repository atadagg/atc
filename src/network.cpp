#include "network.hpp"

#include <algorithm>
#include <vector>

#include "goods.hpp"
#include "params.hpp"
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

namespace {

// The good a household shops for. This ASSUMES exactly one consumer good in
// the world, which is what the supply chain currently builds; the first one
// found wins, so a world with two consumer goods would route every household
// to whichever has the lower index. Households choosing between consumer goods
// is a later piece of work. NO_GOOD means "no consumer good declared", in
// which case the filter passes everything -- the pre-supply-chain behaviour.
uint32_t consumer_good(const World& w) {
    for (uint32_t g = 0; g < w.goods.size(); ++g) {
        if (w.goods[g].consumer) return g;
    }
    return NO_GOOD;
}

// Shared PREDICATE for the two household draws. Only the predicate is shared;
// the draws themselves stay apart on purpose (one size-weighted, one flat).
bool shoppable(const World& w, const Household& h, uint32_t f, uint32_t good) {
    if (good != NO_GOOD && w.firms[f].good != good) return false;
    return !has_shop(h, f);
}

bool buys_from(const Firm& f, uint32_t input, uint32_t seller) {
    const uint32_t base = input * params::kSuppliersPerInput;
    for (uint32_t s = 0; s < params::kSuppliersPerInput; ++s) {
        if (f.suppliers[base + s] == seller) return true;
    }
    return false;
}

}  // namespace

uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng) {
    const Household& h = w.households[household];

    const uint32_t good = consumer_good(w);

    double total_weight = 0.0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!shoppable(w, h, f, good)) continue;
        total_weight += 1.0 + static_cast<double>(w.firms[f].employees.size());
    }
    if (total_weight <= 0.0) return NO_FIRM;

    double draw = rng.uniform(0.0, total_weight);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!shoppable(w, h, f, good)) continue;
        draw -= 1.0 + static_cast<double>(w.firms[f].employees.size());
        if (draw <= 0.0) return f;
    }
    return NO_FIRM;  // unreachable except for floating-point edge cases
}

uint32_t random_unconnected_firm(const World& w, uint32_t household, Rng& rng) {
    const Household& h = w.households[household];

    const uint32_t good = consumer_good(w);

    uint32_t n = 0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (shoppable(w, h, f, good)) ++n;
    }
    if (n == 0) return NO_FIRM;

    uint32_t pick = rng.index(n);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!shoppable(w, h, f, good)) continue;
        if (pick == 0) return f;
        --pick;
    }
    return NO_FIRM;  // unreachable
}

uint32_t random_unconnected_producer(const World& w, uint32_t firm, uint32_t good, Rng& rng) {
    const Firm& buyer = w.firms[firm];

    // Which input slot group `good` belongs to. A firm may buy the same good
    // for only one input, so the first match is the group.
    uint32_t input = params::kMaxInputs;
    const Good& recipe = w.goods[buyer.good];
    for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
        if (recipe.inputs[i] == good) { input = i; break; }
    }
    if (input == params::kMaxInputs) return NO_FIRM;

    auto eligible = [&](uint32_t f) {
        return f != firm && w.firms[f].good == good && !buys_from(buyer, input, f);
    };

    double total_weight = 0.0;
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!eligible(f)) continue;
        total_weight += 1.0 + static_cast<double>(w.firms[f].employees.size());
    }
    if (total_weight <= 0.0) return NO_FIRM;

    double draw = rng.uniform(0.0, total_weight);
    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        if (!eligible(f)) continue;
        draw -= 1.0 + static_cast<double>(w.firms[f].employees.size());
        if (draw <= 0.0) return f;
    }
    return NO_FIRM;  // unreachable except for floating-point edge cases
}

}  // namespace network
