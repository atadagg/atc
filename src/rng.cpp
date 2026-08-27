#include "rng.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

uint64_t Rng::next_u64() {
    return engine_();
}

double Rng::uniform(double lo, double hi) {
    // Hand-rolled instead of std::uniform_real_distribution: that class's
    // mapping from engine output to a double is implementation-defined and
    // is not guaranteed to produce identical values across standard library
    // versions/vendors for the same engine state -- exactly the portability
    // hazard shuffle() below already avoids by hand-rolling Fisher-Yates.
    // The top 53 bits of a 64-bit draw exactly fill a double's mantissa, so
    // (raw >> 11) * 2^-53 is an exact (no rounding error) map to [0, 1),
    // which we then affinely scale into [lo, hi).
    const uint64_t raw = next_u64();
    const double unit = static_cast<double>(raw >> 11) * 0x1.0p-53;
    return lo + unit * (hi - lo);
}

uint32_t Rng::index(uint32_t n) {
    if (n == 0) {
        throw std::invalid_argument("Rng::index: n must be > 0");
    }
    // Hand-rolled instead of std::uniform_int_distribution: same portability
    // concern as uniform() above. Plain `raw % n` would also be biased
    // whenever n does not evenly divide 2^64, silently skewing agent
    // selection toward the low residues -- so we reject draws landing in
    // the trailing partial bucket to keep every outcome equally likely.
    const uint64_t n64 = static_cast<uint64_t>(n);
    const uint64_t limit = (std::numeric_limits<uint64_t>::max() / n64) * n64;
    uint64_t raw;
    do {
        raw = next_u64();
    } while (raw >= limit);
    return static_cast<uint32_t>(raw % n64);
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
    for (std::size_t i = v.size() - 1; i > 0; --i) {
        uint32_t j = index(static_cast<uint32_t>(i + 1));
        std::swap(v[i], v[j]);
    }
}
