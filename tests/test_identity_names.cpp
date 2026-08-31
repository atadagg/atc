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
