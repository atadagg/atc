#include <stdexcept>
#include <vector>

#include "doctest.h"
#include "histogram.hpp"

TEST_CASE("values spread across bins land where expected") {
    const std::vector<double> v{0.0, 1.0, 2.0, 3.0};
    const Histogram h = bin(v, 4);
    CHECK(h.lo == 0.0);
    CHECK(h.hi == 3.0);
    REQUIRE(h.counts.size() == 4);
    CHECK(h.counts[0] == 1);
    CHECK(h.counts[3] == 1);
    CHECK(h.mean == doctest::Approx(1.5));
}

TEST_CASE("the maximum value lands in the last bin, not past it") {
    const std::vector<double> v{0.0, 10.0};
    const Histogram h = bin(v, 5);
    CHECK(h.counts.back() == 1);
    REQUIRE(h.members.back().size() == 1);
    CHECK(h.members.back()[0] == 1u);
}

TEST_CASE("all-identical values collapse into one bin") {
    // This is the zero-wage spike: 67 firms at exactly 0.0.
    const std::vector<double> v(67, 0.0);
    const Histogram h = bin(v, 10);
    CHECK(h.lo == 0.0);
    CHECK(h.hi == 0.0);
    CHECK(h.counts[0] == 67);
    CHECK(h.members[0].size() == 67);
    CHECK(h.mean == 0.0);
}

TEST_CASE("members record the original indices") {
    const std::vector<double> v{5.0, 0.0, 5.0};
    const Histogram h = bin(v, 2);
    REQUIRE(h.members.front().size() == 1);
    CHECK(h.members.front()[0] == 1u);
    REQUIRE(h.members.back().size() == 2);
    CHECK(h.members.back()[0] == 0u);
    CHECK(h.members.back()[1] == 2u);
}

TEST_CASE("an empty input produces empty bins, not a crash") {
    const Histogram h = bin({}, 4);
    CHECK(h.counts.size() == 4);
    CHECK(h.counts[0] == 0);
    CHECK(h.mean == 0.0);
}

TEST_CASE("a bin count below one is rejected") {
    CHECK_THROWS_AS(bin({1.0}, 0), std::invalid_argument);
}
