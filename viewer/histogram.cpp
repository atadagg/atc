#include "histogram.hpp"

#include <algorithm>
#include <stdexcept>

Histogram bin(const std::vector<double>& values, int bins) {
    if (bins < 1) throw std::invalid_argument("bin() needs at least one bin");

    Histogram h;
    h.counts.assign(static_cast<size_t>(bins), 0);
    h.members.assign(static_cast<size_t>(bins), {});
    if (values.empty()) return h;

    const auto [lo_it, hi_it] = std::minmax_element(values.begin(), values.end());
    h.lo = *lo_it;
    h.hi = *hi_it;

    double sum = 0.0;
    for (double v : values) sum += v;
    h.mean = sum / static_cast<double>(values.size());

    const double width = h.hi - h.lo;
    for (uint32_t i = 0; i < values.size(); ++i) {
        size_t b = 0;
        if (width > 0.0) {
            const double t = (values[i] - h.lo) / width;
            b = static_cast<size_t>(t * static_cast<double>(bins));
            // t == 1.0 for the maximum: clamp it into the last bin rather
            // than one past the end.
            b = std::min(b, static_cast<size_t>(bins) - 1);
        }
        ++h.counts[b];
        h.members[b].push_back(i);
    }
    return h;
}
