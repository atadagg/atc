#include "doctest.h"
#include "identity_glyphs.hpp"
#include "identity_svg.hpp"

#include <set>
#include <string>

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
    size_t pos = 0;
    std::set<std::string> ids;
    while ((pos = s.find("<clipPath id=\"", pos)) != std::string::npos) {
        pos += 14;
        const size_t end = s.find('"', pos);
        CHECK(ids.insert(s.substr(pos, end - pos)).second);
    }
    CHECK(ids.size() >= 40);   // every mark-bearing cell, wordmarks excepted
}
