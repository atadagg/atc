#include "identity.hpp"

#include "identity_palette.hpp"

namespace identity {

namespace {
constexpr uint64_t kGolden = 0x9E3779B97F4A7C15ull;

// Weighted tables. Repetition is set by the SMALLEST pool in a template, so
// these are kept deliberately wide (spec Part 5).
constexpr Sil kSils[] = {Sil::Disc, Sil::Disc, Sil::Block, Sil::Block, Sil::Square};
constexpr Div kDivs[] = {Div::Plain, Div::Plain, Div::Plain, Div::Plain,
                         Div::Pale, Div::Fess, Div::Quarterly, Div::Chief};
constexpr Charge kCharges[] = {Charge::Roundel, Charge::Annulet, Charge::Lozenge,
                               Charge::Pale, Charge::Fess, Charge::Roundel,
                               Charge::Annulet, Charge::Lozenge, Charge::None,
                               Charge::Cross};
constexpr Kind kKinds[] = {Kind::Combination, Kind::Combination, Kind::Combination,
                           Kind::Lettermark, Kind::Wordmark, Kind::Abstract};
constexpr Lock kLocks[] = {Lock::Horizontal, Lock::Horizontal, Lock::Stacked,
                           Lock::MarkRight, Lock::Lettermark, Lock::Wordmark,
                           Lock::StackedWords, Lock::Enclosed};

template <typename T, size_t N>
const T& pick_of(Stream& s, const T (&arr)[N]) { return arr[s.index(N)]; }
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

Mark mark(uint64_t seed) {
    Stream s(seed);
    Mark m{};
    m.sil    = pick_of(s, kSils);
    m.div    = pick_of(s, kDivs);
    m.charge = pick_of(s, kCharges);

    // A cross on anything but a disc is a flag, not a mark.
    if (m.sil != Sil::Disc && m.charge == Charge::Cross) m.charge = Charge::Annulet;
    // A plain field with no charge is a blank emblem.
    if (m.div == Div::Plain && m.charge == Charge::None) m.charge = Charge::Roundel;

    m.charge_size = static_cast<uint8_t>(12 + s.index(5) * 12);
    const auto np = static_cast<uint32_t>(palettes().size());
    m.palette  = static_cast<uint8_t>(s.index(np));
    m.palette2 = static_cast<uint8_t>(s.index(np));
    m.kind     = pick_of(s, kKinds);
    m.lock     = pick_of(s, kLocks);
    m.font     = static_cast<uint8_t>(s.index(kFontCount));
    return m;
}

}  // namespace identity
