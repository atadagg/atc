#include "doctest.h"
#include "identity.hpp"
#include "identity_palette.hpp"

#include <cstring>
#include <set>
#include <type_traits>

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
        CHECK_FALSE((m.div == identity::Div::Plain
                     && m.charge == identity::Charge::None));
        // cross and saltire on a non-disc silhouette read as a flag
        if (m.sil != identity::Sil::Disc)
            CHECK(m.charge != identity::Charge::Cross);
    }
}
