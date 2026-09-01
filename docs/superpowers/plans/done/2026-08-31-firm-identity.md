# Firm Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate a deterministic logo and company name for any firm from its seed and locale, and dump a contact sheet of them as a self-contained SVG.

**Architecture:** A pure, stateless module (`identity`) that never touches `Rng`. It turns a `uint64_t` seed into a POD `Mark` (heraldic geometry, palette indices) and a `Name` (naming strategy applied to per-locale lexicons). A separate `identity_svg` renders both, using glyph outlines precomputed into a generated header so no font is resolved at render time. `main.cpp` gains `--identity-sheet`.

**Tech Stack:** C++23, doctest, CMake. `scripts/gen_glyphs.py` (Python + fontTools, run offline) generates committed glyph data. DINish (OFL) vendored under `third_party/fonts/`.

**Spec:** `docs/superpowers/specs/2026-08-31-firm-identity-design.md`

## Global Constraints

- **`identity` must never include or call `Rng`.** It has its own splitmix64. A `#include "rng.hpp"` anywhere in `src/identity*.{hpp,cpp}` is a defect.
- **No field is added to `Firm` or `World`.** Identity derives from `mix(run_seed, firm_index)`.
- **`Mark` must satisfy `std::is_trivially_copyable_v`.** No virtuals, no pointers, no `std::string`.
- **Locale is a parameter**, never stored on an agent: `name(seed, locale)`.
- **`identity_svg` opens no files.** It returns `std::string`; the caller owns the handle (invariant 5).
- **Generated SVG must contain zero `<text>` elements.** All glyphs are `<path>`.
- Build warning-clean under `-Wall -Wextra -Wpedantic` (invariant 6).
- Palette entries are indices into one table; never colour literals in the grammar.

---

### Task 1: The identity hash and seed derivation

**Files:**
- Create: `src/identity.hpp`, `src/identity.cpp`
- Create: `tests/test_identity.cpp`
- Modify: `CMakeLists.txt` (add `src/identity.cpp` to `atc_core`, `tests/test_identity.cpp` to `atc_tests`)

**Interfaces:**
- Consumes: nothing
- Produces: `identity::mix(uint64_t) -> uint64_t`, `identity::firm_seed(uint64_t run_seed, uint32_t firm_index) -> uint64_t`, and `identity::Stream` with `Stream(uint64_t)`, `next() -> uint64_t`, `index(uint32_t n) -> uint32_t`, `pick(std::span<const T>) -> const T&`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "identity.hpp"
#include <set>

TEST_CASE("identity hash is stateless and order-independent") {
    // Same seed, same answer, no matter how many other seeds were hashed first.
    const uint64_t a = identity::mix(12345);
    for (uint64_t k = 0; k < 1000; ++k) (void)identity::mix(k);
    CHECK(identity::mix(12345) == a);
}

TEST_CASE("firm_seed separates runs and firms") {
    CHECK(identity::firm_seed(1, 0) != identity::firm_seed(2, 0));
    CHECK(identity::firm_seed(1, 0) != identity::firm_seed(1, 1));
    std::set<uint64_t> seen;
    for (uint32_t i = 0; i < 10000; ++i) seen.insert(identity::firm_seed(7, i));
    CHECK(seen.size() == 10000);   // no collisions across a realistic population
}

TEST_CASE("Stream gives reproducible independent draws") {
    identity::Stream s1(99), s2(99);
    for (int i = 0; i < 50; ++i) CHECK(s1.index(100) == s2.index(100));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/atc_tests -ts="*identity*"`
Expected: compile failure — `identity.hpp` not found.

- [ ] **Step 3: Write minimal implementation**

`src/identity.hpp`:

```cpp
#pragma once

// Procedural firm identity. PURE: this module never touches Rng, and must
// never include rng.hpp. Drawing from the simulation's RNG would make the
// economy depend on how many logos the viewer happened to draw (spec Part 1).

#include <cstdint>
#include <span>

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
    const T& pick(std::span<const T> xs) { return xs[index(static_cast<uint32_t>(xs.size()))]; }
private:
    uint64_t seed_;
    uint64_t i_;
};

}  // namespace identity
```

`src/identity.cpp`:

```cpp
#include "identity.hpp"

namespace identity {

namespace {
constexpr uint64_t kGolden = 0x9E3779B97F4A7C15ull;
}

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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*identity*"`
Expected: PASS, 3 test cases.

- [ ] **Step 5: Commit**

```bash
git add src/identity.hpp src/identity.cpp tests/test_identity.cpp CMakeLists.txt
git commit -m "feat(identity): the stateless identity hash, separate from Rng"
```

---

### Task 2: The palette, with an enforced contrast floor

**Files:**
- Create: `src/identity_palette.hpp`
- Modify: `tests/test_identity.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `identity::Rgb {uint8_t r,g,b;}`, `identity::Palette {Rgb field, ink, accent;}`, `identity::palettes() -> std::span<const Palette>`, `identity::luminance(Rgb) -> double`, `identity::contrast(Rgb, Rgb) -> double`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "identity_palette.hpp"

TEST_CASE("every palette obeys the contrast floor") {
    // The rule of tincture, made mechanical: a field always carries a charge
    // it can be told apart from. WCAG contrast ratio, floor 4.5.
    for (const auto& p : identity::palettes()) {
        CHECK(identity::contrast(p.field, p.ink) >= 4.5);
        CHECK(identity::contrast(p.field, p.accent) >= 3.0);
    }
}

TEST_CASE("palette fields are perceptually spaced from each other") {
    // Two navies that read as one colour was a real defect; distinct hex is
    // not distinct enough.
    const auto ps = identity::palettes();
    CHECK(ps.size() >= 12);
    for (size_t i = 0; i < ps.size(); ++i)
        for (size_t j = i + 1; j < ps.size(); ++j) {
            const double d = identity::distance(ps[i].field, ps[j].field);
            CHECK_MESSAGE(d >= 40.0, "palettes ", i, " and ", j, " too close");
        }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `identity_palette.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`src/identity_palette.hpp`:

```cpp
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace identity {

struct Rgb { uint8_t r, g, b; };

// One sanctioned palette. `field` is the emblem ground, `ink` the charge,
// `accent` the single secondary. The set is fixed: a sampled hue is what
// makes generated marks look generated (spec Part 4).
struct Palette { Rgb field, ink, accent; };

inline double srgb_channel(uint8_t c) {
    const double s = c / 255.0;
    return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

inline double luminance(Rgb c) {
    return 0.2126 * srgb_channel(c.r) + 0.7152 * srgb_channel(c.g)
         + 0.0722 * srgb_channel(c.b);
}

inline double contrast(Rgb a, Rgb b) {
    const double la = luminance(a), lb = luminance(b);
    const double hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

// Plain Euclidean distance in sRGB. Crude, but it is a floor test, not a
// colour-science claim, and it catches the two-navies case.
inline double distance(Rgb a, Rgb b) {
    const double dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

inline constexpr Rgb kPaper{0xFF, 0xFF, 0xFF};
inline constexpr Rgb kGold {0xC9, 0xA2, 0x27};

inline std::span<const Palette> palettes() {
    static constexpr std::array<Palette, 16> kTable{{
        {{0x0F, 0x20, 0x38}, kPaper, {0xC8, 0x10, 0x2E}},
        {{0xB3, 0x20, 0x2E}, kPaper, kGold},
        {{0x1D, 0x6B, 0x45}, kPaper, kGold},
        {{0x57, 0x29, 0x6B}, kPaper, {0xC9, 0xA2, 0x27}},
        {{0xC0, 0x60, 0x2A}, kPaper, {0x20, 0x24, 0x2B}},
        {{0x7A, 0x22, 0x46}, kPaper, kGold},
        {{0x2E, 0x7C, 0x9B}, kPaper, {0x20, 0x24, 0x2B}},
        {{0x0F, 0x5C, 0x55}, kPaper, kGold},
        {{0x20, 0x24, 0x2B}, kPaper, {0xC8, 0x10, 0x2E}},
        {{0x2B, 0x4C, 0x7E}, kPaper, kGold},
        {{0x8A, 0x15, 0x38}, kPaper, kPaper},
        {{0x15, 0x5E, 0x3B}, kPaper, kPaper},
        {{0x4A, 0x3B, 0x8C}, kPaper, kGold},
        {{0x96, 0x59, 0x1F}, kPaper, kPaper},
        {{0x0E, 0x4C, 0x6B}, kPaper, kGold},
        {{0x63, 0x10, 0x2A}, kPaper, kGold},
    }};
    return kTable;
}

}  // namespace identity
```

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*identity*"`
Expected: PASS. If a pair fails the 40.0 spacing floor, replace one entry's `field` — do not lower the floor.

- [ ] **Step 5: Commit**

```bash
git add src/identity_palette.hpp tests/test_identity.cpp
git commit -m "feat(identity): sanctioned palettes with an enforced contrast floor"
```

---

### Task 3: The Mark type and the heraldic grammar

**Files:**
- Modify: `src/identity.hpp`, `src/identity.cpp`, `tests/test_identity.cpp`

**Interfaces:**
- Consumes: `identity::Stream`, `identity::palettes()`
- Produces: enums `Sil`, `Div`, `Charge`, `Kind`, `Lock`; `struct Mark`; `identity::mark(uint64_t seed) -> Mark`.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("Mark is trivially copyable") {
    // Invariants 3 and 4: this is what keeps SimSnapshot a plain assignment.
    static_assert(std::is_trivially_copyable_v<identity::Mark>);
    CHECK(sizeof(identity::Mark) <= 16);
}

TEST_CASE("mark is a pure function of its seed") {
    for (uint64_t s : {1ull, 7ull, 999999ull}) {
        const identity::Mark a = identity::mark(s);
        for (uint64_t k = 0; k < 100; ++k) (void)identity::mark(k);
        const identity::Mark b = identity::mark(s);
        CHECK(std::memcmp(&a, &b, sizeof(a)) == 0);
    }
}

TEST_CASE("every generated mark is well formed") {
    const auto np = identity::palettes().size();
    for (uint64_t s = 0; s < 200000; ++s) {
        const identity::Mark m = identity::mark(s);
        CHECK(m.palette < np);
        CHECK(m.charge_size >= 12);
        CHECK(m.charge_size <= 60);
        // never a blank emblem: a plain field with no charge is a wasted mark
        CHECK_FALSE(m.div == identity::Div::Plain
                    && m.charge == identity::Charge::None);
        // cross and saltire on a non-disc silhouette read as a flag
        if (m.sil != identity::Sil::Disc)
            CHECK(m.charge != identity::Charge::Cross);
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `'Mark' is not a member of 'identity'`.

- [ ] **Step 3: Write minimal implementation**

Append to `src/identity.hpp` inside `namespace identity`:

```cpp
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
```

Append to `src/identity.cpp`:

```cpp
namespace {

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
```

`kFontCount` is **not** declared here — it comes from the generated
`identity_glyphs.hpp` (Task 6), which is the single source of truth for how
many fonts exist. Add `#include "identity_glyphs.hpp"` to `identity.hpp`.
Until Task 6 lands, stub it in `identity.hpp` as
`inline constexpr uint32_t kFontCount = 5;` and DELETE that stub in Task 6.

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*identity*"`
Expected: PASS. `charge_size` max is `12 + 4*12 = 60`, matching the test bound.

- [ ] **Step 5: Commit**

```bash
git add src/identity.hpp src/identity.cpp tests/test_identity.cpp
git commit -m "feat(identity): the Mark type and the heraldic grammar"
```

---

### Task 4: Locale lexicons with per-slot minimum pool sizes

**Files:**
- Create: `src/identity_lexicon.hpp`, `src/identity_lexicon.cpp`
- Create: `tests/test_identity_names.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing
- Produces: `enum class Locale : uint8_t { Turkish, English, Japanese, German, Count }`, `struct Lexicon { std::span<const char* const> surname, material, nature, qual, place, sector, legal; bool verified; }`, `identity::lexicon(Locale) -> const Lexicon&`, `identity::locale_name(Locale) -> const char*`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "identity_lexicon.hpp"
#include <set>
#include <string>

TEST_CASE("every lexicon slot meets its minimum pool size") {
    // Repetition is set by the smallest pool in a template, not by the size
    // of the product. This floor is the fix for VOLD/ALLGEMEINE clustering.
    for (uint8_t i = 0; i < static_cast<uint8_t>(identity::Locale::Count); ++i) {
        const auto loc = static_cast<identity::Locale>(i);
        const auto& L = identity::lexicon(loc);
        INFO("locale ", identity::locale_name(loc));
        CHECK(L.surname.size()  >= 24);
        CHECK(L.material.size() >= 12);
        CHECK(L.nature.size()   >= 16);
        CHECK(L.qual.size()     >= 16);
        CHECK(L.place.size()    >= 16);
        CHECK(L.sector.size()   >= 10);
        CHECK(L.legal.size()    >= 2);
    }
}

TEST_CASE("lexicons contain no duplicate entries within a slot") {
    for (uint8_t i = 0; i < static_cast<uint8_t>(identity::Locale::Count); ++i) {
        const auto& L = identity::lexicon(static_cast<identity::Locale>(i));
        for (auto slot : {L.surname, L.material, L.nature, L.qual, L.place, L.sector}) {
            std::set<std::string> seen;
            for (const char* w : slot) CHECK(seen.insert(w).second);
        }
    }
}

TEST_CASE("locale verification status is recorded") {
    // Only Turkish has been reviewed by a native speaker; the rest must say so.
    CHECK(identity::lexicon(identity::Locale::Turkish).verified);
    CHECK_FALSE(identity::lexicon(identity::Locale::Japanese).verified);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `identity_lexicon.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`src/identity_lexicon.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <span>

namespace identity {

enum class Locale : uint8_t { Turkish, English, Japanese, German, Count };

// Word lists a naming strategy draws from. Slot sizes are load-bearing:
// see the minimum-pool test.
struct Lexicon {
    std::span<const char* const> surname;
    std::span<const char* const> material;
    std::span<const char* const> nature;
    std::span<const char* const> qual;
    std::span<const char* const> place;
    std::span<const char* const> sector;
    std::span<const char* const> legal;
    // Reviewed by a speaker of the language. Unverified locales must be
    // reported as such rather than silently shipped (spec Part 10).
    bool verified;
};

const Lexicon& lexicon(Locale l);
const char*    locale_name(Locale l);

}  // namespace identity
```

`src/identity_lexicon.cpp` — Turkish shown in full; fill English, Japanese and German to the same slot sizes using the word lists in `docs/superpowers/specs/2026-08-31-firm-identity-design.md` Part 5 and the prototype at `output/identity_strategies.svg`:

```cpp
#include "identity_lexicon.hpp"

namespace identity {
namespace {

// Turkish. Reviewed by a native speaker. Surnames are ORDINARY surnames:
// seeding this with conglomerate names (Koç, Sabancı) generates real firms.
const char* const kTrSurname[] = {
    "Demir","Kaya","Yılmaz","Şahin","Çelik","Yıldız","Aydın","Özdemir","Arslan",
    "Kılıç","Çetin","Korkmaz","Karaca","Polat","Öztürk","Şimşek","Tekin","Bulut",
    "Güneş","Ateş","Toprak","Akın","Uysal","Erdem","Aksoy","Barut","Cengiz",
    "Duran","Ergin","Fidan","Gürsoy","Hakan"};
const char* const kTrMaterial[] = {
    "Demir","Çelik","Cam","Şişe","Boru","Taş","Kum","Çimento","Bakır","Kağıt",
    "İplik","Dokuma","Tuğla","Maden","Kömür","Petrol","Kimya","Plastik",
    "Alüminyum","Mermer"};
const char* const kTrNature[] = {
    "Yıldız","Güneş","Ay","Deniz","Dağ","Kartal","Şahin","Aslan","Meşe","Çınar",
    "Gül","Lale","Pınar","Irmak","Toprak","Rüzgar","Şafak","Umut","Ufuk","Doruk"};
const char* const kTrQual[] = {
    "Ak","Kara","Al","Yeni","Büyük","Ulu","Öz","Tek","Can","Er","Baş","Gür",
    "Sarp","Yüce","Bir","Genç"};
const char* const kTrPlace[] = {
    "Anadolu","Marmara","Ege","Akdeniz","Karadeniz","Toros","Boğaziçi","Trakya",
    "Kapadokya","Çukurova","Konya","Bursa","Aydın","Menderes","Fırat","Dicle"};
const char* const kTrSector[] = {
    "Sanayi","Ticaret","Holding","Bankası","Denizcilik","Enerji","İnşaat",
    "Tekstil","Gıda","Otomotiv","Lojistik","Madencilik","Kimya","Makina"};
const char* const kTrLegal[] = {"A.Ş.","Ltd. Şti.","Holding"};

// ... kEn*, kJa*, kDe* follow the same shape and slot sizes ...

const Lexicon kTable[] = {
    {kTrSurname, kTrMaterial, kTrNature, kTrQual, kTrPlace, kTrSector, kTrLegal, true},
    // English, Japanese, German: verified = false
};

}  // namespace

const Lexicon& lexicon(Locale l) { return kTable[static_cast<uint8_t>(l)]; }

const char* locale_name(Locale l) {
    static const char* const kNames[] = {"Turkish", "English", "Japanese", "German"};
    return kNames[static_cast<uint8_t>(l)];
}

}  // namespace identity
```

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*lexicon*,*name*"`
Expected: PASS once all four locales are filled to the slot minimums.

- [ ] **Step 5: Commit**

```bash
git add src/identity_lexicon.hpp src/identity_lexicon.cpp tests/test_identity_names.cpp CMakeLists.txt
git commit -m "feat(identity): locale lexicons with enforced per-slot pool sizes"
```

---

### Task 5: Naming strategies

**Files:**
- Modify: `src/identity.hpp`, `src/identity.cpp`, `tests/test_identity_names.cpp`

**Interfaces:**
- Consumes: `identity::Stream`, `identity::lexicon`, `identity::Locale`
- Produces: `enum class Strategy : uint8_t { Founder, Descriptive, Toponymic, Compound, Coined, Acronym, Evocative }`; `struct Name { char head[32]; char tail[48]; Strategy strategy; Locale locale; }`; `identity::name(uint64_t seed, Locale) -> Name`.

`Name` uses fixed char buffers rather than `std::string` so it stays trivially copyable, matching `Mark`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "identity.hpp"
#include <cstring>

TEST_CASE("name is a pure function of seed and locale") {
    const identity::Name a = identity::name(42, identity::Locale::Turkish);
    for (uint64_t k = 0; k < 100; ++k) (void)identity::name(k, identity::Locale::German);
    const identity::Name b = identity::name(42, identity::Locale::Turkish);
    CHECK(std::strcmp(a.head, b.head) == 0);
    CHECK(std::strcmp(a.tail, b.tail) == 0);
}

TEST_CASE("a name always has a non-empty head and never overruns") {
    for (uint8_t l = 0; l < static_cast<uint8_t>(identity::Locale::Count); ++l)
        for (uint64_t s = 0; s < 50000; ++s) {
            const identity::Name n = identity::name(s, static_cast<identity::Locale>(l));
            CHECK(n.head[0] != '\0');
            CHECK(std::strlen(n.head) < sizeof(n.head));
            CHECK(std::strlen(n.tail) < sizeof(n.tail));
            CHECK(n.locale == static_cast<identity::Locale>(l));
        }
}

TEST_CASE("no generated name is a known real company") {
    // Seeding a lexicon with conglomerate names produced KOÇ HOLDING.
    static const char* const kReal[] = {"Koç","Sabancı","Ülker","Zorlu",
                                        "Rheinstahl","Şişecam","Borusan","Petkim"};
    for (uint8_t l = 0; l < static_cast<uint8_t>(identity::Locale::Count); ++l)
        for (uint64_t s = 0; s < 50000; ++s) {
            const identity::Name n = identity::name(s, static_cast<identity::Locale>(l));
            for (const char* r : kReal) CHECK(std::strcmp(n.head, r) != 0);
        }
}

TEST_CASE("no single strategy dominates") {
    int count[7] = {};
    for (uint64_t s = 0; s < 20000; ++s)
        ++count[static_cast<int>(identity::name(s, identity::Locale::Turkish).strategy)];
    for (int c : count) CHECK(c > 20000 / 7 / 3);   // none starved
    for (int c : count) CHECK(c < 20000 / 7 * 3);   // none dominant
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `'Name' is not a member of 'identity'`.

- [ ] **Step 3: Write minimal implementation**

Append to `src/identity.hpp`:

```cpp
enum class Strategy : uint8_t { Founder, Descriptive, Toponymic, Compound,
                                Coined, Acronym, Evocative };

// Fixed buffers, not std::string: Name stays trivially copyable like Mark.
struct Name {
    char     head[32];
    char     tail[48];
    Strategy strategy;
    Locale   locale;
};

Name name(uint64_t seed, Locale locale);
```

Append to `src/identity.cpp` (include `<cstring>`, `<string>` and
`"identity_lexicon.hpp"`):

```cpp
namespace {

void put(char* dst, size_t cap, const char* src) {
    const size_t n = std::strlen(src) < cap - 1 ? std::strlen(src) : cap - 1;
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}

void put2(char* dst, size_t cap, const char* a, const char* b, const char* sep) {
    std::string s = std::string(a) + sep + b;      // local only; never stored
    put(dst, cap, s.c_str());
}

// Truncate-and-join: petrol+kimya -> Petkim. Byte-wise truncation would split
// a UTF-8 sequence, so advance only to a lead byte.
std::string truncate_utf8(const char* w, size_t want) {
    std::string s(w);
    size_t i = 0, seen = 0;
    while (i < s.size() && seen < want) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
        ++seen;
    }
    return s.substr(0, i);
}

}  // namespace

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
        const char ini[4] = {w1[0], w2[0], w3[0], '\0'};
        put(n.head, sizeof(n.head), ini);
        std::string full = std::string(w1) + " " + w2 + " " + w3;
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*name*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/identity.hpp src/identity.cpp tests/test_identity_names.cpp
git commit -m "feat(identity): naming strategies over locale lexicons"
```

---

### Task 6: Precomputed glyph outlines

**Files:**
- Create: `scripts/gen_glyphs.py`
- Create: `src/identity_glyphs.hpp` (generated, committed)
- Create: `third_party/fonts/` (vendored DINish + OFL licence)
- Modify: `tests/test_identity_svg.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `identity::Glyph { uint32_t codepoint; float advance; const char* path; }`, `identity::glyphs(uint8_t font) -> std::span<const Glyph>`, `identity::glyph(uint8_t font, uint32_t cp) -> const Glyph*`, `identity::kFontNames[]`.

Fonts are vendored because a system font resolves against whoever is *viewing*, so the same firm would otherwise have a different identity per machine. Outlines are precomputed so `atc_core` needs no font parser.

- [ ] **Step 1: Write the failing test**

```cpp
#include "identity_glyphs.hpp"

TEST_CASE("every font covers the charset every lexicon needs") {
    // A missing glyph silently becomes '?' at render time, which would
    // mangle Turkish and German names.
    for (uint8_t f = 0; f < identity::kFontCount; ++f)
        for (const char* w : {"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                              "ÇŞĞİÖÜÂÎÛ", "ÄÖÜß", "&.,- "}) {
            for (const char* p = w; *p; )
                { uint32_t cp = identity::next_codepoint(p);
                  CHECK_MESSAGE(identity::glyph(f, cp) != nullptr,
                                "font ", f, " missing U+", cp); }
        }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `identity_glyphs.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

Vendor the fonts:

```bash
mkdir -p third_party/fonts
cp /usr/share/fonts/dinish/DINish-Bold.ttf third_party/fonts/
cp /usr/share/fonts/dinish/DINish-Medium.ttf third_party/fonts/
cp /usr/share/fonts/dinish/DINishCondensed-Bold.ttf third_party/fonts/
cp /usr/share/fonts/dinish/DINishExpanded-Bold.ttf third_party/fonts/
cp /usr/share/fonts/gnu-free/FreeSerif.otf third_party/fonts/
# record the OFL/GPL+FE licence text and source URL in third_party/fonts/PROVENANCE
```

`scripts/gen_glyphs.py` — run offline, output committed:

```python
#!/usr/bin/env python3
"""Generates src/identity_glyphs.hpp from the vendored fonts.

Outlines are precomputed so atc_core needs no font parser and so no font is
resolved at render time. Run after changing the charset or the font set:

    .venv/bin/python scripts/gen_glyphs.py
"""
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

FONTS = [("DINish-Bold.ttf", "DINish Bold"),
         ("DINish-Medium.ttf", "DINish Medium"),
         ("DINishCondensed-Bold.ttf", "DINish Condensed Bold"),
         ("DINishExpanded-Bold.ttf", "DINish Expanded Bold"),
         ("FreeSerif.otf", "FreeSerif")]

CHARSET = (" !&'-.,()0123456789"
           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
           "abcdefghijklmnopqrstuvwxyz"
           "ÇŞĞİIÖÜÂÎÛÅÄÆØßÑÉÈÊÁÀÓÒÔÍÌÚÙ"
           "çşğıöüâîûåäæøñéèêáàóòôíìúù")

out = ['#pragma once', '',
       '// GENERATED by scripts/gen_glyphs.py -- do not edit by hand.', '',
       '#include <cstdint>', '#include <span>', '',
       'namespace identity {', '',
       'struct Glyph { uint32_t codepoint; float advance; const char* path; };', '']

for fi, (fname, label) in enumerate(FONTS):
    f = TTFont(f"third_party/fonts/{fname}")
    upem = f["head"].unitsPerEm
    cmap, gs, hmtx = f.getBestCmap(), f.getGlyphSet(), f["hmtx"]
    rows = []
    for ch in sorted(set(CHARSET)):
        n = cmap.get(ord(ch))
        if n is None:
            raise SystemExit(f"{fname} has no glyph for {ch!r} (U+{ord(ch):04X})")
        pen = SVGPathPen(gs, ntos=lambda v: f"{v/upem:.5f}")
        gs[n].draw(pen)
        rows.append((ord(ch), hmtx[n][0] / upem, pen.getCommands()))
    out.append(f'// {label}')
    out.append(f'inline constexpr Glyph kGlyphs{fi}[] = {{')
    for cp, adv, d in rows:
        out.append(f'    {{{cp}u, {adv:.5f}f, "{d}"}},')
    out.append('};')
    out.append('')

out.append(f'inline constexpr uint32_t kFontCount = {len(FONTS)};')
# Task 3 stubs kFontCount before this header exists; delete that stub now, or
# the two definitions conflict.
out.append('inline const char* const kFontNames[] = {'
           + ', '.join(f'"{l}"' for _, l in FONTS) + '};')
out.append('')
out.append('std::span<const Glyph> glyphs(uint8_t font);')
out.append('const Glyph* glyph(uint8_t font, uint32_t cp);')
out.append('uint32_t next_codepoint(const char*& p);')
out.append('')
out.append('}  // namespace identity')
open("src/identity_glyphs.hpp", "w").write("\n".join(out) + "\n")
print("wrote src/identity_glyphs.hpp")
```

Glyph paths are emitted already divided by units-per-em, so all coordinates are in em units and a caller multiplies by the font size. Implement `glyphs`, `glyph` and `next_codepoint` (a small UTF-8 decoder) in `src/identity_svg.cpp` in Task 7.

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv/bin/python scripts/gen_glyphs.py && cmake --build build -j && ./build/atc_tests -ts="*glyph*"`
Expected: PASS. If the script exits with "no glyph for", drop that character from `CHARSET` or choose a font that covers it — do not let it fall back.

- [ ] **Step 5: Commit**

```bash
git add scripts/gen_glyphs.py src/identity_glyphs.hpp third_party/fonts tests/test_identity_svg.cpp
git commit -m "feat(identity): vendored fonts and precomputed glyph outlines"
```

---

### Task 7: SVG rendering

**Files:**
- Create: `src/identity_svg.hpp`, `src/identity_svg.cpp`
- Modify: `tests/test_identity_svg.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `Mark`, `Name`, `palettes()`, `glyph()`
- Produces: `identity::svg_mark(const Mark&, double x, double y, double size, uint32_t uid) -> std::string`, `identity::svg_text(const char*, uint8_t font, double x, double y, double size, double tracking, Rgb) -> std::string`, `identity::svg_identity(const Mark&, const Name&, double w, double h, uint32_t uid) -> std::string`, `identity::svg_sheet(uint64_t run_seed, uint32_t count, Locale) -> std::string`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "identity_svg.hpp"
#include <string>

TEST_CASE("rendered svg contains no text elements") {
    // A <text> element resolves a font against the viewer. This is the bug
    // that made the masthead render wrong on GitHub.
    const std::string s = identity::svg_sheet(1, 64, identity::Locale::Turkish);
    CHECK(s.find("<text") == std::string::npos);
    CHECK(s.find("font-family") == std::string::npos);
    CHECK(s.find("<path") != std::string::npos);
}

TEST_CASE("svg is deterministic") {
    CHECK(identity::svg_sheet(1, 16, identity::Locale::Turkish)
       == identity::svg_sheet(1, 16, identity::Locale::Turkish));
    CHECK(identity::svg_sheet(1, 16, identity::Locale::Turkish)
       != identity::svg_sheet(2, 16, identity::Locale::Turkish));
}

TEST_CASE("clip path ids are unique across a sheet") {
    const std::string s = identity::svg_sheet(1, 64, identity::Locale::Turkish);
    size_t pos = 0; std::set<std::string> ids;
    while ((pos = s.find("<clipPath id=\"", pos)) != std::string::npos) {
        pos += 14;
        const size_t end = s.find('"', pos);
        CHECK(ids.insert(s.substr(pos, end - pos)).second);
    }
    CHECK(ids.size() >= 64);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: `identity_svg.hpp: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`src/identity_svg.hpp` declares the four functions above. In `src/identity_svg.cpp`:

- `svg_mark` emits, in order: `<clipPath id="m{uid}">` holding the silhouette path, then a `<g clip-path>` containing the field `<rect>`, the division rect (if any), and the charge. Ordered fills only — no boolean geometry.
- Silhouette paths in the 0–100 box: disc `M50,2 A48,48 0 1,1 49.9,2 Z`; block `M9,2 H91 Q98,2 98,9 V91 Q98,98 91,98 H9 Q2,98 2,91 V9 Q2,2 9,2 Z`; square `M2,2 H98 V98 H2 Z`.
- `svg_text` walks the UTF-8 string, looks up each glyph, and emits
  `<path transform="translate(x,0) scale(size,-size)" d="..."/>`, advancing
  `x += advance * size + tracking`. Return the group plus its measured width
  through an out-parameter so lockups can centre properly — the masthead's
  magic-offset bug came from not measuring.
- `svg_identity` switches on `Mark::lock` to place mark and text.
- `svg_sheet` lays out `count` cells in a grid, calling `firm_seed(run_seed, i)`,
  `mark()` and `name()`, and passing `i` as `uid`.
- Escape `&` as `&amp;` and `<` as `&lt;` **at render time only** — storing the
  entity broke acronym extraction in the prototype (`WESTORGATE & CO` → `W`).

- [ ] **Step 4: Run test to verify it passes**

Run: `./build/atc_tests -ts="*svg*"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/identity_svg.hpp src/identity_svg.cpp tests/test_identity_svg.cpp CMakeLists.txt
git commit -m "feat(identity): svg rendering with outlined glyphs"
```

---

### Task 8: The `--identity-sheet` CLI

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `identity::svg_sheet`
- Produces: nothing for other tasks.

- [ ] **Step 1: Add the flags**

In `main.cpp`'s argument loop, alongside `--scenario`:

```cpp
std::string identity_sheet;
uint32_t    identity_count = 256;
identity::Locale identity_locale = identity::Locale::Turkish;
// --identity-sheet <path>   --identity-count <n>   --identity-locale <name>
```

- [ ] **Step 2: Write the sheet and exit before the simulation runs**

```cpp
if (!identity_sheet.empty()) {
    std::ofstream out(identity_sheet);
    if (!out) { std::cerr << "cannot open " << identity_sheet << "\n"; return 1; }
    out << identity::svg_sheet(seed, identity_count, identity_locale);
    std::cout << "wrote " << identity_sheet << " (" << identity_count
              << " identities, locale " << identity::locale_name(identity_locale);
    if (!identity::lexicon(identity_locale).verified)
        std::cout << ", UNVERIFIED";      // spec Part 10
    std::cout << ")\n";
    return 0;
}
```

`main.cpp` is the only file permitted console I/O (invariant 5), and it owns the file handle so `identity_svg` stays stream-free.

- [ ] **Step 3: Verify by eye**

```bash
cmake --build build -j
./build/atc --identity-sheet output/marks.svg --identity-count 256 --identity-locale turkish
rsvg-convert --zoom 1.5 output/marks.svg -o output/marks.png
grep -c "<text" output/marks.svg    # must print 0
```

Expected: `0`, and a contact sheet that looks like `output/identity_strategies.png`.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(cli): --identity-sheet, the identity tuning loop"
```

---

### Task 9: The non-perturbation test

**Files:**
- Modify: `tests/test_identity.cpp`

This is the test with teeth. The others check the generator works; this checks it cannot reach into the simulation.

**Interfaces:**
- Consumes: `identity::mark`, `identity::name`, `World`, `Scheduler`, `Rng`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scheduler.hpp"
#include "world.hpp"

namespace {
std::vector<double> run_months(int months, bool generate_identities) {
    Rng rng(1234);
    World w;
    Scheduler sched(w, rng);
    std::vector<double> trace;
    for (int m = 0; m < months; ++m) {
        if (generate_identities)
            for (uint32_t f = 0; f < 5000; ++f) {
                (void)identity::mark(identity::firm_seed(1234, f));
                (void)identity::name(identity::firm_seed(1234, f),
                                     identity::Locale::Turkish);
            }
        sched.run_month();
        trace.push_back(w.money_in_circulation());
        trace.push_back(w.firms[0].liquidity);
        trace.push_back(w.households[0].liquidity);
    }
    return trace;
}
}  // namespace

TEST_CASE("generating identities cannot perturb the simulation") {
    // If identity ever drew from Rng, how many logos the viewer happened to
    // draw would change the economy. This is invariant 2 (spec Part 1).
    CHECK(run_months(40, false) == run_months(40, true));
}
```

Adjust the `World`/`Scheduler` construction to match `tests/test_scheduler.cpp` — read that file first rather than guessing the signatures.

- [ ] **Step 2: Run test to verify it fails if the rule is broken**

Temporarily add `Rng tmp(1); (void)tmp.index(10);` inside `identity::mark`, rebuild, and confirm the test FAILS. Then remove it and confirm it PASSES. A test that cannot fail is not a test.

- [ ] **Step 3: Add the include guard against regression**

```bash
! grep -r "rng.hpp" src/identity.hpp src/identity.cpp src/identity_svg.cpp \
    src/identity_lexicon.cpp && echo "clean"
```

Add that grep to the test as a comment noting the intent.

- [ ] **Step 4: Commit**

```bash
git add tests/test_identity.cpp
git commit -m "test(identity): prove identity generation cannot perturb the run"
```

---

### Task 10: Coverage and legibility measurement

**Files:**
- Create: `scripts/identity_stats.py`
- Modify: `tests/test_identity.cpp`

**Interfaces:**
- Consumes: `identity::mark`, `identity::name`

- [ ] **Step 1: Write the failing distribution test**

```cpp
TEST_CASE("no mark or name is over-represented") {
    // Two-sided on purpose: "a million distinct marks" is satisfied by a
    // generator that also emits one mark 40% of the time.
    std::map<std::string, int> heads;
    constexpr int kN = 100000;
    for (uint64_t s = 0; s < kN; ++s)
        ++heads[identity::name(s, identity::Locale::Turkish).head];
    CHECK(heads.size() > 4000);
    for (const auto& [h, c] : heads)
        CHECK_MESSAGE(c < kN / 200, "name '", h, "' appears ", c, " times");
}
```

- [ ] **Step 2: Run it and widen the lexicons until it passes**

Run: `./build/atc_tests -ts="*over-represented*"`
Expected: initially FAIL. The fix is more entries in whichever slot is
starved — never a lower bound. Print the top ten heads to find the culprit.

- [ ] **Step 3: Write the failing legibility test**

```cpp
TEST_CASE("every mark stays legible at favicon size") {
    // Rasterise the mark's silhouette and charge into a 16x16 grid and check
    // it has neither gone solid nor vanished. Free-form geometry measured
    // 29.9% illegible at this size, which is why the grammar is constrained.
    for (uint64_t s = 0; s < 20000; ++s) {
        const double cov = identity::ink_coverage(identity::mark(s), 16);
        CHECK(cov >= 0.15);
        CHECK(cov <= 0.85);
    }
}
```

Implement `identity::ink_coverage(const Mark&, int grid)` in `identity.cpp`:
sample `grid x grid` pixel centres in the 0-100 box, return the fraction whose
painted colour is not the page background. It shares the ordered-fill logic
with `svg_mark`, so factor that decision into one
`identity::paint_at(const Mark&, double x, double y) -> uint8_t` used by both —
otherwise the measurement and the drawing can disagree, which would make the
test meaningless.

- [ ] **Step 4: Write `scripts/identity_stats.py`**

Stdlib only, matching `validate.py`. Reads a sheet produced by
`--identity-sheet` and reports: distinct name heads, the ten most frequent,
the per-strategy split, and the count of `<text>` elements (must be 0).

- [ ] **Step 5: Commit**

```bash
git add scripts/identity_stats.py tests/test_identity.cpp src/identity.cpp src/identity.hpp
git commit -m "test(identity): two-sided coverage bounds, legibility floor, stats script"
```

---

## Follow-up plans (not in this plan)

- **Viewer integration** — `viewer/mark_draw.{hpp,cpp}`, an identity column in
  `panel_agents`, `Cell` support for a mark. Needs a window; different tests.
- **More locales** — each needs a speaker to review it before `verified` is set.
- **Logo grammar improvements** — the user has explicitly deferred these.
