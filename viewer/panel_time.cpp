#include <cfloat>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "model_time.hpp"

namespace {

void plot(const RingRecorder& r, const SeriesSpec& s) {
    const std::vector<double> values = series_values(r, s);
    if (values.size() < 2) {
        ImGui::TextDisabled("%s: not enough months yet", s.name);
        return;
    }
    std::vector<float> v;
    v.reserve(values.size());
    for (double d : values) v.push_back(static_cast<float>(d));
    ImGui::PlotLines(s.name, v.data(), static_cast<int>(v.size()), 0, nullptr,
                     FLT_MAX, FLT_MAX, ImVec2(0, 60));
}

}  // namespace

void draw_time_panel(App& app) {
    ImGui::Begin("time");

    // The scrub bar ranges over every month the run has ever reached
    // (max_month), not app.sim.month() -- sim.month() is whatever is
    // currently displayed and shrinks on a backward seek, which would
    // otherwise shrink the slider's own ceiling and make it impossible to
    // scrub forward again after one seek back.
    int month = app.sim.month();
    ImGui::SetNextItemWidth(-120.0f);
    if (app.max_month > 0) {
        // Seeking backward restores the nearest snapshot and re-simulates
        // forward, so what is shown is real state, not interpolation.
        if (ImGui::SliderInt("month", &month, 0, app.max_month)) {
            if (month < 0) month = 0;
            if (month > app.max_month) month = app.max_month;
            app.seek_to(month);
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
