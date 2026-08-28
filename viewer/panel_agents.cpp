#include <algorithm>
#include <cstdio>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "params.hpp"

namespace {

bool firm_passes(const App& app, uint32_t i, const Firm& f) {
    const auto& pinned = app.filters.pinned_firms;
    if (!pinned.empty() && std::find(pinned.begin(), pinned.end(), i) == pinned.end()) {
        return false;
    }
    if (app.filters.wage_at_floor && f.wage > params::kMinWage) return false;
    if (app.filters.vacancy_open && !f.has_open_vacancy) return false;
    if (app.filters.inventory_zero && f.inventory > 0.0) return false;
    return true;
}

void draw_firm_table(App& app) {
    const auto& firms = app.sim.world().firms;

    ImGui::Checkbox("wage at floor", &app.filters.wage_at_floor);
    ImGui::SameLine();
    ImGui::Checkbox("vacancy open", &app.filters.vacancy_open);
    ImGui::SameLine();
    ImGui::Checkbox("inventory zero", &app.filters.inventory_zero);
    if (!app.filters.pinned_firms.empty()) {
        ImGui::SameLine();
        ImGui::Text("| %zu pinned", app.filters.pinned_firms.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("clear")) app.filters.pinned_firms.clear();
    }

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("firms", 8, kFlags, ImVec2(0, 300))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("firm");
    ImGui::TableSetupColumn("wage");
    ImGui::TableSetupColumn("price");
    ImGui::TableSetupColumn("inventory");
    ImGui::TableSetupColumn("staff");
    ImGui::TableSetupColumn("liquidity");
    ImGui::TableSetupColumn("vacancy");
    ImGui::TableSetupColumn("demand");
    ImGui::TableHeadersRow();

    std::vector<uint32_t> rows;
    rows.reserve(firms.size());
    for (uint32_t i = 0; i < firms.size(); ++i) {
        if (firm_passes(app, i, firms[i])) rows.push_back(i);
    }

    if (ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
        sort != nullptr && sort->SpecsCount > 0) {
        const ImGuiTableColumnSortSpecs& s = sort->Specs[0];
        const bool asc = s.SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
            const Firm& fa = firms[a];
            const Firm& fb = firms[b];
            double va = 0.0;
            double vb = 0.0;
            switch (s.ColumnIndex) {
                case 0: va = a; vb = b; break;
                case 1: va = fa.wage; vb = fb.wage; break;
                case 2: va = fa.price; vb = fb.price; break;
                case 3: va = fa.inventory; vb = fb.inventory; break;
                case 4: va = static_cast<double>(fa.employees.size());
                        vb = static_cast<double>(fb.employees.size()); break;
                case 5: va = fa.liquidity; vb = fb.liquidity; break;
                case 6: va = fa.has_open_vacancy; vb = fb.has_open_vacancy; break;
                default: va = fa.demand_last_month; vb = fb.demand_last_month; break;
            }
            return asc ? (va < vb) : (vb < va);
        });
    }

    for (uint32_t i : rows) {
        const Firm& f = firms[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        char label[16];
        std::snprintf(label, sizeof(label), "%u", i);
        if (ImGui::Selectable(label, app.selected_firm == i,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            app.selected_firm = i;
        }
        ImGui::TableNextColumn(); ImGui::Text("%.2f", f.wage);
        ImGui::TableNextColumn(); ImGui::Text("%.3f", f.price);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.inventory);
        ImGui::TableNextColumn(); ImGui::Text("%zu", f.employees.size());
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.liquidity);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(f.has_open_vacancy ? "yes" : "");
        ImGui::TableNextColumn(); ImGui::Text("%.1f", f.demand_last_month);
    }
    ImGui::EndTable();
}

void draw_household_table(App& app) {
    const auto& households = app.sim.world().households;

    ImGui::Checkbox("unemployed only", &app.filters.unemployed);

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("households", 5, kFlags, ImVec2(0, 300))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("household");
    ImGui::TableSetupColumn("employer");
    ImGui::TableSetupColumn("liquidity");
    ImGui::TableSetupColumn("reservation wage");
    ImGui::TableSetupColumn("planned consumption");
    ImGui::TableHeadersRow();

    // 1000 rows: let ImGui skip the off-screen ones rather than formatting
    // text for all of them every frame.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(households.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const uint32_t i = static_cast<uint32_t>(row);
            const Household& h = households[i];
            if (app.filters.unemployed && h.employed()) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[16];
            std::snprintf(label, sizeof(label), "%u", i);
            if (ImGui::Selectable(label, app.selected_household == i,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                app.selected_household = i;
            }
            ImGui::TableNextColumn();
            if (h.employed()) {
                ImGui::Text("%u", h.employer);
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.liquidity);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.reservation_wage);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", h.planned_consumption);
        }
    }
    ImGui::EndTable();
}

void draw_detail(App& app) {
    const World& w = app.sim.world();

    if (app.selected_firm != NO_FIRM && app.selected_firm < w.firms.size()) {
        const Firm& f = w.firms[app.selected_firm];
        ImGui::SeparatorText("selected firm");
        ImGui::Text("firm %u", app.selected_firm);
        ImGui::Text("wage %.4f   price %.4f   inventory %.3f", f.wage, f.price, f.inventory);
        ImGui::Text("liquidity %.3f", f.liquidity);
        ImGui::Text("demand this month %.2f, last month %.2f",
                    f.demand_this_month, f.demand_last_month);
        ImGui::Text("months fully staffed %u", f.months_fully_staffed);
        ImGui::Text("vacancy open %s, went unfilled %s",
                    f.has_open_vacancy ? "yes" : "no",
                    f.vacancy_went_unfilled ? "yes" : "no");
        if (f.pending_fire != NO_HOUSEHOLD) {
            ImGui::Text("pending fire: household %u", f.pending_fire);
        }
        ImGui::Text("employees (%zu):", f.employees.size());
        for (uint32_t e : f.employees) {
            ImGui::SameLine();
            char b[16];
            std::snprintf(b, sizeof(b), "%u", e);
            if (ImGui::SmallButton(b)) app.selected_household = e;
        }
    }

    if (app.selected_household != NO_HOUSEHOLD && app.selected_household < w.households.size()) {
        const Household& h = w.households[app.selected_household];
        ImGui::SeparatorText("selected household");
        ImGui::Text("household %u", app.selected_household);
        ImGui::Text("liquidity %.4f   reservation wage %.4f", h.liquidity, h.reservation_wage);
        ImGui::Text("planned consumption %.4f", h.planned_consumption);
        if (h.employed()) {
            ImGui::Text("employer:");
            ImGui::SameLine();
            char b[16];
            std::snprintf(b, sizeof(b), "%u", h.employer);
            if (ImGui::SmallButton(b)) app.selected_firm = h.employer;
        } else {
            ImGui::TextUnformatted("unemployed");
        }
        ImGui::TextUnformatted("shops:");
        for (size_t s = 0; s < h.shops.size(); ++s) {
            ImGui::SameLine();
            char b[24];
            std::snprintf(b, sizeof(b), "%u##shop%zu", h.shops[s], s);
            if (ImGui::SmallButton(b)) app.selected_firm = h.shops[s];
            if (h.unsatisfied[s] > 0.0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(-%.1f)", h.unsatisfied[s]);
            }
        }
    }
}

}  // namespace

void draw_agents_panel(App& app) {
    ImGui::Begin("agents");
    if (ImGui::BeginTabBar("agent_tabs")) {
        if (ImGui::BeginTabItem("firms")) {
            draw_firm_table(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("households")) {
            draw_household_table(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    draw_detail(app);
    ImGui::End();
}
