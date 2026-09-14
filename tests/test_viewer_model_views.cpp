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

TEST_CASE("a dead firm is excluded from every firm distribution") {
    Rng rng(1);
    World w(3, 3, rng);
    w.firms[0].wage = 1.0;
    w.firms[1].wage = 2.0;
    w.firms[2].wage = 3.0;
    w.firms[1].alive = false;

    const auto specs = distributions();
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        if (specs[static_cast<size_t>(i)].over != AgentKind::Firm) continue;
        const std::vector<double> v = dist_values(w, i);
        const std::vector<uint32_t> idx = dist_indices(w, i);
        REQUIRE(v.size() == 2);
        REQUIRE(idx.size() == 2);
        // The corpse (firm 1) never appears, and the survivors keep their
        // real indices.
        CHECK(idx[0] == 0u);
        CHECK(idx[1] == 2u);
    }
}

TEST_CASE("dist_indices maps a Histogram's members back to real firm indices "
          "once a firm has died") {
    Rng rng(1);
    World w(3, 3, rng);
    w.firms[0].wage = 0.0;
    w.firms[1].wage = 0.0;  // dead: must not appear in either vector
    w.firms[2].wage = 10.0;
    w.firms[1].alive = false;

    const std::vector<double> wages = dist_values(w, 0);
    const std::vector<uint32_t> idx = dist_indices(w, 0);
    REQUIRE(wages.size() == 2);
    const Histogram h = bin(wages, 2);
    REQUIRE(h.members.front().size() == 1);
    CHECK(idx[h.members.front()[0]] == 0u);  // living firm 0, at wage 0.0
    REQUIRE(h.members.back().size() == 1);
    CHECK(idx[h.members.back()[0]] == 2u);  // living firm 2, at wage 10.0
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

TEST_CASE("scrub bounds are disabled before the run has advanced") {
    const ScrubBounds b = scrub_bounds(0);
    CHECK_FALSE(b.enabled);
    CHECK(b.lo == 0);
    CHECK(b.hi == 0);
}

TEST_CASE("scrub bounds track max_month, not the displayed month") {
    // A backward seek must not shrink the control's own ceiling -- hi
    // follows the furthest month the run has ever reached.
    const ScrubBounds b = scrub_bounds(120);
    CHECK(b.enabled);
    CHECK(b.lo == 0);
    CHECK(b.hi == 120);
}

TEST_CASE("clamp_to_bounds pulls a requested month inside the range") {
    const ScrubBounds b = scrub_bounds(100);
    CHECK(clamp_to_bounds(b, -5) == 0);
    CHECK(clamp_to_bounds(b, 200) == 100);
    CHECK(clamp_to_bounds(b, 42) == 42);
}
