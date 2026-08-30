#include "doctest.h"
#include "ring_recorder.hpp"

namespace {

Metrics month(int m) {
    Metrics x;
    x.month = m;
    x.mean_wage = static_cast<double>(m);
    return x;
}

}  // namespace

TEST_CASE("an empty ring reports its window honestly") {
    RingRecorder r(4);
    CHECK(r.size() == 0);
    CHECK(r.capacity() == 4);
}

TEST_CASE("a partially filled ring returns what it has, oldest first") {
    RingRecorder r(4);
    r.record(month(1));
    r.record(month(2));
    CHECK(r.size() == 2);
    CHECK(r.at(0).month == 1);
    CHECK(r.at(1).month == 2);
}

TEST_CASE("a full ring keeps the last N, oldest first") {
    RingRecorder r(3);
    for (int m = 1; m <= 5; ++m) r.record(month(m));
    CHECK(r.size() == 3);
    CHECK(r.at(0).month == 3);
    CHECK(r.at(1).month == 4);
    CHECK(r.at(2).month == 5);
    CHECK(r.at(2).mean_wage == 5.0);
}

TEST_CASE("clear empties the ring") {
    RingRecorder r(3);
    r.record(month(1));
    r.clear();
    CHECK(r.size() == 0);
}

TEST_CASE("capacity of zero is rejected") {
    CHECK_THROWS_AS(RingRecorder(0), std::invalid_argument);
}
