#pragma once

#include <cstdint>
#include <cstring>

#include "world.hpp"

// The run hash, shared by the golden single-good gate and the multi-good
// determinism tests. One definition, so the two cannot drift apart.
namespace golden {

// FNV-1a over the raw bytes of a double, so the hash is byte-exact rather
// than tolerance-based. A tolerance here would defeat the entire point.
inline void mix(uint64_t& h, double v) {
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<uint64_t>((bits >> (i * 8)) & 0xFFu);
        h *= 1099511628211ull;
    }
}

inline void mix_u32(uint64_t& h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xFFu);
        h *= 1099511628211ull;
    }
}

inline uint64_t run_hash(const World& w) {
    uint64_t h = 14695981039346656037ull;
    for (const Firm& f : w.firms) {
        mix(h, f.liquidity);
        mix(h, f.inventory);
        mix(h, f.price);
        mix(h, f.wage);
        mix(h, f.demand_this_month);
        mix(h, f.demand_last_month);
        mix_u32(h, static_cast<uint32_t>(f.employees.size()));
        for (uint32_t e : f.employees) mix_u32(h, e);
        mix_u32(h, f.has_open_vacancy ? 1u : 0u);
        mix_u32(h, f.vacancy_went_unfilled ? 1u : 0u);
        mix_u32(h, f.months_fully_staffed);
        mix_u32(h, f.pending_fire);
    }
    for (const Household& hh : w.households) {
        mix(h, hh.liquidity);
        mix(h, hh.reservation_wage);
        mix_u32(h, hh.employer);
        for (uint32_t s : hh.shops) mix_u32(h, s);
        mix(h, hh.planned_consumption);
        for (double u : hh.unsatisfied) mix(h, u);
    }
    return h;
}

}  // namespace golden
