#include <cstdio>
#include <vector>

#include "app.hpp"
#include "histogram.hpp"
#include "imgui.h"
#include "model_dist.hpp"

namespace {

// Draws one distribution. `over_firms` decides whether a bin click pins the
// firm table to that bin's members.
void draw_one(App& app, const char* name, const std::vector<double>& values,
              int bins, bool over_firms) {
    const Histogram h = bin(values, bins);

    int max_count = 1;
    for (int c : h.counts) max_count = c > max_count ? c : max_count;

    ImGui::SeparatorText(name);
    // The mean is an annotation on the distribution, never the headline --
    // see the averaging-artifact note in CLAUDE.md.
    ImGui::TextDisabled("range [%.4f, %.4f]   mean %.4f   n=%zu   lowest bin holds %d",
                        h.lo, h.hi, h.mean, values.size(), h.counts[0]);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float w = avail / static_cast<float>(h.counts.size()) - 1.0f;
    for (size_t b = 0; b < h.counts.size(); ++b) {
        if (b > 0) ImGui::SameLine(0.0f, 1.0f);
        const float frac = static_cast<float>(h.counts[b]) / static_cast<float>(max_count);
        char id[32];
        std::snprintf(id, sizeof(id), "##%s%zu", name, b);
        // A button whose height encodes the count: clickable, and no plotting
        // library needed.
        ImGui::BeginGroup();
        const float height = 80.0f;
        ImGui::Dummy(ImVec2(w, height * (1.0f - frac)));
        if (ImGui::Button(id, ImVec2(w, height * frac + 1.0f)) && over_firms) {
            app.filters.pinned_firms = h.members[b];
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            const double width = (h.hi - h.lo) / static_cast<double>(h.counts.size());
            ImGui::SetTooltip("[%.4f, %.4f): %d\n%s", h.lo + width * b,
                              h.lo + width * (b + 1), h.counts[b],
                              over_firms ? "click to pin these firms" : "");
        }
    }
}

}  // namespace

void draw_dist_panel(App& app) {
    static int bins = 20;
    ImGui::Begin("distributions");
    ImGui::SliderInt("bins", &bins, 4, 60, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("click a firm bin to pin those firms in the agents panel");

    // Defensive floor: Ctrl+Click text entry is clamped by AlwaysClamp above, but
    // bin() throws std::invalid_argument for bins < 1 with nothing in main.cpp's
    // render loop to catch it, so re-assert the precondition here too.
    const int effective_bins = bins < 1 ? 1 : bins;

    const World& w = app.sim.world();
    const auto specs = distributions();
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        const DistSpec& s = specs[static_cast<size_t>(i)];
        draw_one(app, s.name, dist_values(w, i), effective_bins, s.pinnable);
    }

    ImGui::End();
}
