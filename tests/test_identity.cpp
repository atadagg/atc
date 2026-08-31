#include "doctest.h"
#include "identity.hpp"
#include "identity_palette.hpp"

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
