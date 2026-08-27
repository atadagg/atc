#include "doctest.h"
#include "rng.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

TEST_CASE("same seed produces identical sequences") {
    Rng a(12345);
    Rng b(12345);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.uniform(0.0, 1.0) == b.uniform(0.0, 1.0));
    }
}

TEST_CASE("different seeds produce different sequences") {
    Rng a(1);
    Rng b(2);
    bool any_different = false;
    for (int i = 0; i < 100; ++i) {
        if (a.uniform(0.0, 1.0) != b.uniform(0.0, 1.0)) any_different = true;
    }
    CHECK(any_different);
}

TEST_CASE("uniform respects bounds") {
    Rng r(7);
    for (int i = 0; i < 10000; ++i) {
        double v = r.uniform(2.0, 5.0);
        CHECK(v >= 2.0);
        CHECK(v < 5.0);
    }
}

TEST_CASE("index returns values below n") {
    Rng r(7);
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.index(10) < 10u);
    }
}

TEST_CASE("index(1) always returns 0") {
    Rng r(7);
    for (int i = 0; i < 1000; ++i) {
        CHECK(r.index(1) == 0u);
    }
}

TEST_CASE("index(n) is unbiased for a small n") {
    constexpr uint32_t n = 7;
    constexpr int draws = 100000;
    std::array<int, n> counts{};
    Rng r(123);
    for (int i = 0; i < draws; ++i) {
        counts[r.index(n)]++;
    }
    const double expected = static_cast<double>(draws) / n;
    for (uint32_t outcome = 0; outcome < n; ++outcome) {
        double deviation = std::abs(counts[outcome] - expected) / expected;
        CHECK(deviation < 0.05);
    }
}

TEST_CASE("uniform covers the full range, not just a collapsed sliver") {
    constexpr int buckets = 10;
    constexpr int draws = 100000;
    std::array<int, buckets> counts{};
    Rng r(456);
    for (int i = 0; i < draws; ++i) {
        double v = r.uniform(0.0, 1.0);
        int bucket = static_cast<int>(v * buckets);
        if (bucket >= buckets) bucket = buckets - 1; // guard v == 1.0 edge, shouldn't happen
        counts[static_cast<std::size_t>(bucket)]++;
    }
    for (int bucket = 0; bucket < buckets; ++bucket) {
        CHECK(counts[static_cast<std::size_t>(bucket)] > 0);
    }
}

TEST_CASE("same seed produces identical index sequences") {
    Rng a(2024);
    Rng b(2024);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.index(97) == b.index(97));
    }
}

TEST_CASE("chance(0) never fires and chance(1) always fires") {
    Rng r(7);
    for (int i = 0; i < 1000; ++i) {
        CHECK_FALSE(r.chance(0.0));
        CHECK(r.chance(1.0));
    }
}

TEST_CASE("shuffle is deterministic for a given seed and permutes") {
    std::vector<uint32_t> a{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint32_t> b = a;
    Rng ra(99);
    Rng rb(99);
    ra.shuffle(a);
    rb.shuffle(b);
    CHECK(a == b);

    std::vector<uint32_t> sorted = a;
    std::sort(sorted.begin(), sorted.end());
    for (uint32_t i = 0; i < 10; ++i) CHECK(sorted[i] == i);
}

TEST_CASE("shuffle diverges for different seeds") {
    std::vector<uint32_t> a{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint32_t> c = a;
    Rng ra(99);
    Rng rc(100);
    ra.shuffle(a);
    rc.shuffle(c);
    CHECK(a != c);
}
