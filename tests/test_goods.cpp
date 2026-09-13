#include <type_traits>

#include "doctest.h"
#include "goods.hpp"
#include "world.hpp"

TEST_CASE("a fresh World has exactly one consumer good") {
    Rng rng(1);
    World w(10, 5, rng);

    REQUIRE(w.goods.size() == 1);
    CHECK(w.goods[0].consumer == true);
    CHECK(w.goods[0].remove == 0);
    CHECK(w.goods[0].labour_per_unit > 0.0);
    for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
        CHECK(w.goods[0].inputs[i] == NO_GOOD);
    }
}

TEST_CASE("every firm makes the consumer good by default") {
    Rng rng(1);
    World w(10, 5, rng);
    for (const Firm& f : w.firms) CHECK(f.good == 0u);
}

TEST_CASE("Good stays trivially copyable") {
    CHECK(std::is_trivially_copyable_v<Good>);
}
