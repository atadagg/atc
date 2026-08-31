#include "doctest.h"
#include "identity.hpp"
#include "identity_lexicon.hpp"

#include <cstring>

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

TEST_CASE("acronym initials are whole letters, not half a UTF-8 sequence") {
    // Çelik -> "Ç", never the lead byte alone. A split sequence renders as a
    // replacement glyph and sorts as garbage.
    for (uint8_t l = 0; l < static_cast<uint8_t>(identity::Locale::Count); ++l)
        for (uint64_t s = 0; s < 20000; ++s) {
            const identity::Name n = identity::name(s, static_cast<identity::Locale>(l));
            if (n.strategy != identity::Strategy::Acronym) continue;
            for (const char* p = n.head; *p;) {
                const unsigned char c = static_cast<unsigned char>(*p);
                REQUIRE_MESSAGE((c & 0xC0) != 0x80, "head starts mid-sequence");
                size_t w = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
                for (size_t k = 1; k < w; ++k)
                    REQUIRE_MESSAGE((static_cast<unsigned char>(p[k]) & 0xC0) == 0x80,
                                    "truncated UTF-8 sequence in an initial");
                p += w;
            }
        }
}

TEST_CASE("no word appears in two head slots") {
    // Founder, Toponymic and Evocative each put one word in the head. A word
    // listed in two of those slots is drawn by two strategies and lands at
    // twice the rate of its neighbours -- which is how Şahin and Yıldız broke
    // the two-sided coverage bound.
    for (uint8_t i = 0; i < static_cast<uint8_t>(identity::Locale::Count); ++i) {
        const auto loc = static_cast<identity::Locale>(i);
        const auto& L = identity::lexicon(loc);
        INFO("locale ", identity::locale_name(loc));
        std::set<std::string> seen;
        for (auto slot : {L.surname, L.nature, L.place})
            for (const char* w : slot)
                CHECK_MESSAGE(seen.insert(w).second, "'", w, "' is in two head slots");
    }
}
