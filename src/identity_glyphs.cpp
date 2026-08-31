#include "identity_glyphs.hpp"

#include <algorithm>

namespace identity {

std::span<const Glyph> glyphs(uint8_t font) {
    switch (font) {
    case 0:  return kGlyphs0;
    case 1:  return kGlyphs1;
    case 2:  return kGlyphs2;
    case 3:  return kGlyphs3;
    default: return kGlyphs4;
    }
}

const Glyph* glyph(uint8_t font, uint32_t cp) {
    // The generator emits each table sorted by codepoint, so this is a search
    // rather than a scan -- text rendering hits it once per character.
    const auto g = glyphs(font);
    const auto it = std::lower_bound(g.begin(), g.end(), cp,
        [](const Glyph& a, uint32_t b) { return a.codepoint < b; });
    return (it != g.end() && it->codepoint == cp) ? &*it : nullptr;
}

uint32_t next_codepoint(const char*& p) {
    const auto b0 = static_cast<unsigned char>(*p);
    if (b0 < 0x80) { ++p; return b0; }
    const size_t n = (b0 >> 5) == 0x6 ? 2 : (b0 >> 4) == 0xE ? 3 : 4;
    uint32_t cp = b0 & (0xFFu >> (n + 1));
    for (size_t i = 1; i < n; ++i) {
        const auto b = static_cast<unsigned char>(p[i]);
        if ((b & 0xC0) != 0x80) { ++p; return 0xFFFDu; }   // malformed
        cp = (cp << 6) | (b & 0x3Fu);
    }
    p += n;
    return cp;
}

}  // namespace identity
