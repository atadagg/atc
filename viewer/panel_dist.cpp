#include <cstdio>
#include <vector>

#include "app.hpp"
#include "histogram.hpp"
#include "imgui.h"

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

std::vector<double> firm_field(const World& w, int which) {
    std::vector<double> v;
    v.reserve(w.firms.size());
    for (const Firm& f : w.firms) {
        switch (which) {
            case 0: v.push_back(f.wage); break;
            case 1: v.push_back(f.price); break;
            case 2: v.push_back(f.inventory); break;
            case 3: v.push_back(static_cast<double>(f.employees.size())); break;
            default: v.push_back(f.liquidity); break;
        }
    }
    return v;
}

}  // namespace

void draw_dist_panel(App& app) {
    static int bins = 20;
    ImGui::Begin("distributions");
    ImGui::SliderInt("bins", &bins, 4, 60);
    ImGui::TextDisabled("click a firm bin to pin those firms in the agents panel");

    const World& w = app.sim.world();
    draw_one(app, "firm wage", firm_field(w, 0), bins, true);
    draw_one(app, "firm price", firm_field(w, 1), bins, true);
    draw_one(app, "firm inventory", firm_field(w, 2), bins, true);
    draw_one(app, "firm headcount", firm_field(w, 3), bins, true);
    draw_one(app, "firm liquidity", firm_field(w, 4), bins, true);

    std::vector<double> hl;
    std::vector<double> rw;
    hl.reserve(w.households.size());
    rw.reserve(w.households.size());
    for (const Household& h : w.households) {
        hl.push_back(h.liquidity);
        rw.push_back(h.reservation_wage);
    }
    draw_one(app, "household liquidity", hl, bins, false);
    draw_one(app, "household reservation wage", rw, bins, false);

    ImGui::End();
}
