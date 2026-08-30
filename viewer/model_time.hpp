#pragma once

#include <span>
#include <vector>

#include "recorder.hpp"

class RingRecorder;

// One plottable series. The member pointer names the quantity without
// naming a plotting library.
struct SeriesSpec {
    const char* name;
    double Metrics::* field;
};

std::span<const SeriesSpec> series();

// The retained months of one series, oldest first. Empty if nothing has
// been recorded since the last seek.
std::vector<double> series_values(const RingRecorder& r, const SeriesSpec& s);
