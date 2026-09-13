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

#include <stdexcept>
#include <vector>

namespace {

// ore -> iron -> cloth -> garments(consumer)
std::vector<Good> four_deep_chain() {
    std::vector<Good> g(4);
    for (Good& x : g) {
        x.inputs.fill(NO_GOOD);
        x.input_qty.fill(0.0);
    }
    // 0 = ore (raw), 1 = iron, 2 = cloth, 3 = garments
    g[1].inputs[0] = 0; g[1].input_qty[0] = 2.0;
    g[2].inputs[0] = 1; g[2].input_qty[0] = 1.5;
    g[3].inputs[0] = 2; g[3].input_qty[0] = 1.0;
    g[3].consumer = true;
    return g;
}

}  // namespace

TEST_CASE("remove counts steps to the nearest consumer good") {
    auto g = four_deep_chain();
    goods::recompute(g, 0.7);

    CHECK(g[3].remove == 0);  // garments: the consumer good
    CHECK(g[2].remove == 1);  // cloth
    CHECK(g[1].remove == 2);  // iron
    CHECK(g[0].remove == 3);  // ore
}

TEST_CASE("remove takes the LONGEST path when a good feeds two chains") {
    // ore feeds both iron (deep) and a consumer good directly (shallow).
    auto g = four_deep_chain();
    g.push_back(Good{});
    g[4].inputs.fill(NO_GOOD);
    g[4].input_qty.fill(0.0);
    g[4].inputs[0] = 0;
    g[4].input_qty[0] = 1.0;
    g[4].consumer = true;

    goods::recompute(g, 0.7);

    // Longest, not shortest: ore is still a deeply upstream commodity even
    // though one of its buyers sells straight to households.
    CHECK(g[0].remove == 3);
}

TEST_CASE("commoditization rises with remove and is zero at the consumer") {
    auto g = four_deep_chain();
    goods::recompute(g, 0.7);

    CHECK(g[3].commoditization == doctest::Approx(0.0));
    CHECK(g[2].commoditization > g[3].commoditization);
    CHECK(g[1].commoditization > g[2].commoditization);
    CHECK(g[0].commoditization > g[1].commoditization);
    CHECK(g[0].commoditization < 1.0);
}

TEST_CASE("a single consumer good is not a commodity") {
    Rng rng(1);
    World w(10, 5, rng);
    goods::recompute(w.goods, w.params.commoditization_k);

    CHECK(w.goods[0].remove == 0);
    CHECK(w.goods[0].commoditization == doctest::Approx(0.0));
}

TEST_CASE("a cycle in the graph is rejected") {
    std::vector<Good> g(2);
    for (Good& x : g) { x.inputs.fill(NO_GOOD); x.input_qty.fill(0.0); }
    g[0].inputs[0] = 1;
    g[1].inputs[0] = 0;
    g[1].consumer = true;

    CHECK_THROWS_AS(goods::assert_acyclic(g), std::invalid_argument);
}
