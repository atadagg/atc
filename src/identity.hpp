#pragma once

// Procedural firm identity. PURE: this module never touches Rng, and must
// never include rng.hpp. Drawing from the simulation's RNG would make the
// economy depend on how many logos the viewer happened to draw (spec Part 1).

#include <cstddef>
#include <cstdint>
#include <span>

#include "identity_glyphs.hpp"

namespace identity {

// splitmix64 finalizer. Stateless, so identity has random access: firm
// 999,999 costs what firm 0 costs, and no call has an ordering effect.
uint64_t mix(uint64_t x);

uint64_t firm_seed(uint64_t run_seed, uint32_t firm_index);

// A sequence of independent draws from one seed. Deliberately not Rng.
class Stream {
public:
    explicit Stream(uint64_t seed) : seed_(seed), i_(0) {}
    uint64_t next();
    uint32_t index(uint32_t n);
    template <typename T>
    const T& pick(std::span<const T> xs) {
        return xs[index(static_cast<uint32_t>(xs.size()))];
    }

private:
    uint64_t seed_;
    uint64_t i_;
};


// ---- The mark -------------------------------------------------------------

enum class Sil    : uint8_t { Disc, Block, Square };
enum class Div    : uint8_t { Plain, Pale, Fess, Quarterly, Chief };
enum class Charge : uint8_t { None, Roundel, Annulet, Lozenge, Pale, Fess, Cross };
enum class Kind   : uint8_t { Abstract, Lettermark, Wordmark, Combination };
enum class Lock   : uint8_t { Horizontal, Stacked, MarkRight, Lettermark,
                              Wordmark, StackedWords, Enclosed };

// Geometry lives in a fixed 0-255 unit box so the SVG and ImGui backends
// agree exactly rather than approximately.
struct Mark {
    Sil     sil;
    Div     div;
    Charge  charge;
    uint8_t charge_size;
    uint8_t palette;
    uint8_t palette2;   // the divided half
    Kind    kind;
    Lock    lock;
    uint8_t font;
};

Mark mark(uint64_t seed);

}  // namespace identity
