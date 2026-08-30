#pragma once

#include <cstdint>
#include <vector>

// A distribution over agents, plus the agent indices in each bin so that
// clicking a bin can filter the agent table.
struct Histogram {
    double lo = 0.0;
    double hi = 0.0;
    std::vector<int> counts;
    std::vector<std::vector<uint32_t>> members;
    double mean = 0.0;
};

// Bins `values` into `bins` equal-width buckets over [min, max]. The maximum
// value lands in the last bin. If every value is identical (the degenerate
// range), everything lands in bin 0.
// Throws std::invalid_argument if bins < 1.
Histogram bin(const std::vector<double>& values, int bins);
