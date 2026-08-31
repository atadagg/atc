#include "identity.hpp"

#include "identity_lexicon.hpp"
#include "identity_palette.hpp"

#include <cstring>
#include <string>

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

namespace {

void put(char* dst, size_t cap, const char* src) {
    const size_t len = std::strlen(src);
    const size_t n = len < cap - 1 ? len : cap - 1;
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}

void put2(char* dst, size_t cap, const char* a, const char* b, const char* sep) {
    const std::string s = std::string(a) + sep + b;   // local only; never stored
    put(dst, cap, s.c_str());
}

// Truncate-and-join: petrol+kimya -> Petkim. Byte-wise truncation would split
// a UTF-8 sequence, so advance only to a lead byte.
std::string truncate_utf8(const char* w, size_t want) {
    const std::string s(w);
    size_t i = 0, seen = 0;
    while (i < s.size() && seen < want) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        ++seen;
    }
    return s.substr(0, i);
}

// The first CODEPOINT, not the first byte. Half of a UTF-8 sequence is not a
// letter: byte-wise initials turn Çelik into a broken glyph.
std::string first_letter(const char* w) {
    if (*w == '\0') return {};
    size_t i = 1;
    while (w[i] != '\0' && (static_cast<unsigned char>(w[i]) & 0xC0) == 0x80) ++i;
    return std::string(w, i);
}

}  // namespace

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

Name name(uint64_t seed, Locale locale) {
    Stream s(seed ^ 0xA5A5A5A5A5A5A5A5ull);   // decorrelated from mark()
    const Lexicon& L = lexicon(locale);
    Name n{};
    n.locale = locale;
    n.strategy = static_cast<Strategy>(s.index(7));

    switch (n.strategy) {
    case Strategy::Founder:
        put(n.head, sizeof(n.head), s.pick(L.surname));
        put(n.tail, sizeof(n.tail), s.pick(L.sector));
        break;
    case Strategy::Descriptive:
        put2(n.head, sizeof(n.head), s.pick(L.qual), s.pick(L.nature), "");
        put(n.tail, sizeof(n.tail), s.pick(L.sector));
        break;
    case Strategy::Toponymic:
        put(n.head, sizeof(n.head), s.pick(L.place));
        put(n.tail, sizeof(n.tail), s.pick(L.sector));
        break;
    case Strategy::Compound:
        put2(n.head, sizeof(n.head), s.pick(L.nature), s.pick(L.material), "");
        put(n.tail, sizeof(n.tail), s.pick(L.legal));
        break;
    case Strategy::Coined: {
        const std::string a = truncate_utf8(s.pick(L.material), 3 + s.index(2));
        const std::string b = truncate_utf8(s.pick(L.sector),   3 + s.index(2));
        put(n.head, sizeof(n.head), (a + b).c_str());
        put(n.tail, sizeof(n.tail), s.pick(L.legal));
        break;
    }
    case Strategy::Acronym: {
        const char* w1 = s.pick(L.qual);
        const char* w2 = s.pick(L.nature);
        const char* w3 = s.pick(L.sector);
        const std::string ini = first_letter(w1) + first_letter(w2) + first_letter(w3);
        put(n.head, sizeof(n.head), ini.c_str());
        const std::string full = std::string(w1) + " " + w2 + " " + w3;
        put(n.tail, sizeof(n.tail), full.c_str());
        break;
    }
    case Strategy::Evocative:
        put(n.head, sizeof(n.head), s.pick(L.nature));
        put(n.tail, sizeof(n.tail), s.pick(L.sector));
        break;
    }
    return n;
}

}  // namespace identity
