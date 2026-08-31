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
