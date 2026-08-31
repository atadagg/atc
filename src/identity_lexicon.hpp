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
