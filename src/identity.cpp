#include "identity.hpp"

namespace identity {

namespace {
constexpr uint64_t kGolden = 0x9E3779B97F4A7C15ull;
}  // namespace

uint64_t mix(uint64_t x) {
    x += kGolden;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint64_t firm_seed(uint64_t run_seed, uint32_t firm_index) {
    return mix(mix(run_seed) ^ (static_cast<uint64_t>(firm_index) * kGolden));
}

uint64_t Stream::next() {
    ++i_;
    return mix(seed_ + i_ * kGolden);
}

uint32_t Stream::index(uint32_t n) {
    return n == 0 ? 0u : static_cast<uint32_t>(next() % n);
}

}  // namespace identity
