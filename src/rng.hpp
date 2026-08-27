#pragma once

#include <cstdint>
#include <random>
#include <vector>

// Deterministic RNG. Every source of randomness in the simulation must
// draw from an instance of this, passed explicitly. Same seed => same run.
class Rng {
public:
    explicit Rng(uint64_t seed) : engine_(seed) {}

    // Uniform real in [lo, hi).
    double uniform(double lo, double hi);

    // Uniform integer in [0, n). Throws std::invalid_argument if n == 0.
    uint32_t index(uint32_t n);

    // True with probability p.
    bool chance(double p);

    // In-place Fisher-Yates using this engine.
    void shuffle(std::vector<uint32_t>& v);

private:
    // Raw 64-bit draw from the engine. All other methods build their
    // conversions from this by hand (see rng.cpp) rather than going
    // through <random>'s distribution classes, whose engine-output-to-value
    // mapping is implementation-defined and not guaranteed stable across
    // standard library versions/vendors for a fixed seed.
    uint64_t next_u64();

    std::mt19937_64 engine_;
};
