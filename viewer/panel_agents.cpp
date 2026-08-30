#include <cstdio>
#include <cstring>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "model_agents.hpp"

namespace {

// The whole skin, in one function: a Cell in, pixels out.
void draw_cell(App& app, const Cell& c, AgentKind kind, uint32_t row_id, int col) {
    char buf[32];
    switch (c.kind) {
        case CellKind::Missing:
            ImGui::TextUnformatted("-");
            break;
        case CellKind::Flag:
            ImGui::TextUnformatted(c.number != 0.0 ? "yes" : "");
            break;
        case CellKind::Count:
            ImGui::Text("%d", static_cast<int>(c.number));
            break;
        case CellKind::AgentRef:
            if (col == 0) {
                // The id column selects the row rather than navigating away.
                std::snprintf(buf, sizeof(buf), "%u", c.ref);
                if (ImGui::Selectable(buf, kind == AgentKind::Firm
                                               ? app.selected_firm == row_id
                                               : app.selected_household == row_id,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (kind == AgentKind::Firm) {
                        app.selected_firm = row_id;
                    } else {
                        app.selected_household = row_id;
                    }
                }
            } else {
                ImGui::Text("%u", c.ref);
            }
            break;
        case CellKind::Number:
            std::snprintf(buf, sizeof(buf), "%.*f", c.precision, c.number);
            ImGui::TextUnformatted(buf);
            break;
    }
}

void draw_firm_table(App& app) {
    const World& w = app.sim.world();

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

    const auto cols = firm_columns();
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("firms", static_cast<int>(cols.size()), kFlags, ImVec2(0, 300))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    for (const ColumnSpec& c : cols) {
        ImGui::TableSetupColumn(c.header, c.sortable ? ImGuiTableColumnFlags_None
                                                     : ImGuiTableColumnFlags_NoSort);
    }
    ImGui::TableHeadersRow();

    // Translate ImGui's sort state into the model's, and let the model sort.
    SortSpec sort;
    if (ImGuiTableSortSpecs* s = ImGui::TableGetSortSpecs();
        s != nullptr && s->SpecsCount > 0) {
        sort.column = s->Specs[0].ColumnIndex;
        sort.ascending = s->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
    }

    for (uint32_t i : firm_rows(w, app.filters, sort)) {
        ImGui::TableNextRow();
        for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
            ImGui::TableNextColumn();
            draw_cell(app, firm_cell(w, i, c), AgentKind::Firm, i, c);
        }
    }
    ImGui::EndTable();
}

void draw_household_table(App& app) {
    const World& w = app.sim.world();

    ImGui::Checkbox("unemployed only", &app.filters.unemployed);

    const auto cols = household_columns();
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("households", static_cast<int>(cols.size()), kFlags,
                           ImVec2(0, 300))) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    for (const ColumnSpec& c : cols) ImGui::TableSetupColumn(c.header);
    ImGui::TableHeadersRow();

    // Filter first, then clip over the filtered index vector -- ImGuiListClipper
    // sizes the scroll area from item_count x row_height, so skipping rows
    // inside the loop desyncs the scrollbar from what actually renders.
    const std::vector<uint32_t> rows = household_rows(w, app.filters, SortSpec{});

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const uint32_t i = rows[static_cast<size_t>(row)];
            ImGui::TableNextRow();
            for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
                ImGui::TableNextColumn();
                draw_cell(app, household_cell(w, i, c), AgentKind::Household, i, c);
            }
        }
    }
    ImGui::EndTable();
}

void draw_one_detail(App& app, const DetailModel& d) {
    if (!d.present) return;

    ImGui::SeparatorText(d.kind == AgentKind::Firm ? "selected firm" : "selected household");
    ImGui::Text("%s %u", d.kind == AgentKind::Firm ? "firm" : "household", d.id);

    char buf[64];
    for (const DetailField& f : d.fields) {
        // Skip a field that is also rendered as a link group (a household's
        // employer, present both ways in the model) -- otherwise the id
        // would appear twice, once as inert text and once as a button.
        bool has_group = false;
        for (const LinkGroup& g : d.groups) {
            if (std::strcmp(g.label, f.label) == 0) {
                has_group = true;
                break;
            }
        }
        if (has_group) continue;

        switch (f.value.kind) {
            case CellKind::Missing:
                ImGui::Text("%s: -", f.label);
                break;
            case CellKind::Flag:
                ImGui::Text("%s: %s", f.label, f.value.number != 0.0 ? "yes" : "no");
                break;
            case CellKind::Count:
                ImGui::Text("%s: %d", f.label, static_cast<int>(f.value.number));
                break;
            case CellKind::AgentRef:
                ImGui::Text("%s: %u", f.label, f.value.ref);
                break;
            case CellKind::Number:
                std::snprintf(buf, sizeof(buf), "%.*f", f.value.precision, f.value.number);
                ImGui::Text("%s: %s", f.label, buf);
                break;
        }
    }

    for (size_t g = 0; g < d.groups.size(); ++g) {
        const LinkGroup& group = d.groups[g];
        ImGui::Text("%s (%zu):", group.label, group.links.size());
        for (size_t i = 0; i < group.links.size(); ++i) {
            const DetailLink& link = group.links[i];
            ImGui::SameLine();
            // The ## suffix keeps ids unique when the same agent appears twice.
            std::snprintf(buf, sizeof(buf), "%u##g%zul%zu", link.target, g, i);
            if (ImGui::SmallButton(buf)) {
                if (link.kind == AgentKind::Firm) {
                    app.selected_firm = link.target;
                } else {
                    app.selected_household = link.target;
                }
            }
            if (link.has_annotation) {
                ImGui::SameLine();
                ImGui::TextDisabled("(-%.1f)", link.annotation);
            }
        }
    }
}

void draw_detail(App& app) {
    const World& w = app.sim.world();
    draw_one_detail(app, firm_detail(w, app.selected_firm));
    draw_one_detail(app, household_detail(w, app.selected_household));
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
