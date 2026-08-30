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

// The range a scrub control spans, and whether it can be used at all.
// `hi` is the furthest month the run has ever reached, never the month
// currently displayed -- a backward seek must not shrink the control's own
// ceiling, or the run could never be scrubbed forward again.
struct ScrubBounds {
    int lo = 0;
    int hi = 0;
    bool enabled = false;  // false before the run has advanced at all
};

ScrubBounds scrub_bounds(int max_month);

// Clamps a requested month into the bounds.
int clamp_to_bounds(const ScrubBounds& b, int requested);
