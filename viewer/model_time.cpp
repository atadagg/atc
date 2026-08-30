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
