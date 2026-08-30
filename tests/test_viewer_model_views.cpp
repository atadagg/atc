#include <vector>

#include "doctest.h"
#include "histogram.hpp"
#include "model_dist.hpp"
#include "model_time.hpp"
#include "recorder.hpp"
#include "rng.hpp"
#include "ring_recorder.hpp"
#include "world.hpp"

TEST_CASE("every distribution yields one value per agent of its kind") {
    Rng rng(1);
    World w(6, 4, rng);

    const auto specs = distributions();
    REQUIRE(specs.size() == 7);
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        const std::vector<double> v = dist_values(w, i);
        const size_t expected =
            specs[static_cast<size_t>(i)].over == AgentKind::Firm ? 4u : 6u;
        CHECK(v.size() == expected);
    }
}

TEST_CASE("only firm distributions are pinnable") {
    // Pinning filters the firm table, so a household bin has nothing to pin.
    for (const DistSpec& s : distributions()) {
        if (s.over == AgentKind::Household) CHECK_FALSE(s.pinnable);
    }
}

TEST_CASE("a distribution's bin members index back into the agent array") {
    Rng rng(1);
    World w(3, 3, rng);
    w.firms[0].wage = 0.0;
    w.firms[1].wage = 0.0;
    w.firms[2].wage = 10.0;

    const std::vector<double> wages = dist_values(w, 0);
    const Histogram h = bin(wages, 2);
    REQUIRE(h.members.front().size() == 2);
    CHECK(h.members.front()[0] == 0u);
    CHECK(h.members.front()[1] == 1u);
    REQUIRE(h.members.back().size() == 1);
    CHECK(h.members.back()[0] == 2u);
}

TEST_CASE("series read the months the ring retains, oldest first") {
    RingRecorder r(4);
    for (int m = 0; m < 3; ++m) {
        Metrics metrics;
        metrics.month = m;
        metrics.unemployment_rate = 0.1 * m;
        r.record(metrics);
    }

    const auto specs = series();
    REQUIRE(specs.size() == 6);
    const std::vector<double> v = series_values(r, specs[0]);
    REQUIRE(v.size() == 3);
    CHECK(v[0] == doctest::Approx(0.0));
    CHECK(v[2] == doctest::Approx(0.2));
}

TEST_CASE("an empty ring yields an empty series rather than throwing") {
    const RingRecorder r(4);
    CHECK(series_values(r, series()[0]).empty());
}
