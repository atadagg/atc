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
inline constexpr Rgb kCream{0xF0, 0xE0, 0xB8};

// Fields are a hue sweep at held saturation and value, then spaced against
// each other: every pair is at least 40 apart in sRGB and every field carries
// white ink at a WCAG ratio of 5.4 or better. Hand-picked hex did not survive
// that check -- two navies and a charcoal read as one colour.
inline std::span<const Palette> palettes() {
    static constexpr std::array<Palette, 16> kTable{{
        {{0x3D, 0x2B, 0x28}, kPaper, kGold},
        {{0x80, 0x65, 0x46}, kPaper, kCream},
        {{0x5C, 0x56, 0x12}, kPaper, kCream},
        {{0x52, 0x5C, 0x3C}, kPaper, kCream},
        {{0x2B, 0x5C, 0x12}, kPaper, kGold},
        {{0x0C, 0x3D, 0x0E}, kPaper, kGold},
        {{0x23, 0x6E, 0x42}, kPaper, kCream},
        {{0x47, 0x6E, 0x66}, kPaper, kCream},
        {{0x19, 0x6E, 0x80}, kPaper, kCream},
        {{0x12, 0x34, 0x5C}, kPaper, kGold},
        {{0x39, 0x3F, 0x80}, kPaper, kGold},
        {{0x1B, 0x0C, 0x3D}, kPaper, kGold},
        {{0x71, 0x53, 0x80}, kPaper, kCream},
        {{0x6E, 0x16, 0x6A}, kPaper, kGold},
        {{0x5C, 0x32, 0x4B}, kPaper, kGold},
        {{0x80, 0x19, 0x2F}, kPaper, kGold},
    }};
    return kTable;
}

}  // namespace identity
