#include <cfloat>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "model_time.hpp"

namespace {

void plot(const RingRecorder& r, const SeriesSpec& s) {
    const size_t n = r.size();
    if (n < 2) {
        ImGui::TextDisabled("%s: not enough months yet", s.name);
        return;
    }
    // Built directly into the float buffer ImGui::PlotLines wants, rather
    // than through series_values()'s double vector -- that would be a
    // second ~4096-entry allocation per series, per frame.
    std::vector<float> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        v.push_back(static_cast<float>(r.at(i).*s.field));
    }
    ImGui::PlotLines(s.name, v.data(), static_cast<int>(v.size()), 0, nullptr,
                     FLT_MAX, FLT_MAX, ImVec2(0, 60));
}

}  // namespace

void draw_time_panel(App& app) {
    ImGui::Begin("time");

    // See ScrubBounds in model_time.hpp for why the slider's ceiling is
    // max_month rather than the currently displayed month.
    const ScrubBounds bounds = scrub_bounds(app.max_month);
    int month = app.sim.month();
    ImGui::SetNextItemWidth(-120.0f);
    if (bounds.enabled) {
        // Seeking backward restores the nearest snapshot and re-simulates
        // forward, so what is shown is real state, not interpolation.
        if (ImGui::SliderInt("month", &month, bounds.lo, bounds.hi)) {
            app.seek_to(clamp_to_bounds(bounds, month));
        }
    } else {
        ImGui::BeginDisabled();
        int zero = 0;
        ImGui::SliderInt("month", &zero, 0, 0);
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("latest")) {
        app.seek_to(app.max_month);
    }
    ImGui::TextDisabled("showing month %d of %d", app.sim.month(), app.max_month);

    ImGui::SeparatorText("recorded since the last seek");
    for (const SeriesSpec& s : series()) plot(app.metrics, s);

    ImGui::End();
}
