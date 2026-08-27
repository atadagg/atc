#include "rng.hpp"

#include <algorithm>

double Rng::uniform(double lo, double hi) {
    std::uniform_real_distribution<double> d(lo, hi);
    return d(engine_);
}

uint32_t Rng::index(uint32_t n) {
    std::uniform_int_distribution<uint32_t> d(0, n - 1);
    return d(engine_);
}

bool Rng::chance(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return uniform(0.0, 1.0) < p;
}

void Rng::shuffle(std::vector<uint32_t>& v) {
    // Explicit Fisher-Yates rather than std::shuffle: guarantees the same
    // sequence across standard library versions, which std::shuffle does not.
    if (v.size() < 2) return;
    for (size_t i = v.size() - 1; i > 0; --i) {
        uint32_t j = index(static_cast<uint32_t>(i + 1));
        std::swap(v[i], v[j]);
    }
}
