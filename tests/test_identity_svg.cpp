#include "doctest.h"
#include "identity_glyphs.hpp"

TEST_CASE("every font covers the charset every lexicon needs") {
    // A missing glyph silently becomes '?' at render time, which would
    // mangle Turkish and German names.
    for (uint8_t f = 0; f < identity::kFontCount; ++f)
        for (const char* w : {"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                              "ÇŞĞİÖÜÂÎÛ", "ÄÖÜß", "&.,- "}) {
            for (const char* p = w; *p;) {
                const uint32_t cp = identity::next_codepoint(p);
                CHECK_MESSAGE(identity::glyph(f, cp) != nullptr,
                              "font ", f, " missing U+", cp);
            }
        }
}
