#include "model_time.hpp"

#include "ring_recorder.hpp"

namespace {

constexpr SeriesSpec kSeries[] = {
    {"unemployment",        &Metrics::unemployment_rate},
    {"mean wage",           &Metrics::mean_wage},
    {"mean price",          &Metrics::mean_price},
    {"gini",                &Metrics::gini},
    {"inventory",           &Metrics::inventory},
    {"planned consumption", &Metrics::planned_consumption},
};

}  // namespace

std::span<const SeriesSpec> series() {
    return std::span<const SeriesSpec>(kSeries);
}

std::vector<double> series_values(const RingRecorder& r, const SeriesSpec& s) {
    std::vector<double> v;
    v.reserve(r.size());
    for (size_t i = 0; i < r.size(); ++i) v.push_back(r.at(i).*s.field);
    return v;
}

ScrubBounds scrub_bounds(int max_month) {
    ScrubBounds b;
    b.lo = 0;
    b.hi = max_month;
    b.enabled = max_month > 0;
    return b;
}

int clamp_to_bounds(const ScrubBounds& b, int requested) {
    if (requested < b.lo) return b.lo;
    if (requested > b.hi) return b.hi;
    return requested;
}
