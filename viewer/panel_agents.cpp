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

// Find a field by label. Returns a Missing cell if the model did not emit
// one -- used for fields the model only adds conditionally (a firm's
// pending fire).
const Cell& find_field(const DetailModel& d, const char* label) {
    for (const DetailField& f : d.fields) {
        if (std::strcmp(f.label, label) == 0) return f.value;
    }
    static const Cell missing = missing_cell();
    return missing;
}

// Find a link group by label. Returns nullptr if the model did not emit one
// -- a household's "employer" group only exists while employed.
const LinkGroup* find_group(const DetailModel& d, const char* label) {
    for (const LinkGroup& g : d.groups) {
        if (std::strcmp(g.label, label) == 0) return &g;
    }
    return nullptr;
}

// Layout and label wording are display concerns, so the detail panel lays
// each agent kind out explicitly rather than looping generically over
// fields -- that generic loop is what regressed the on-screen output (see
// task-6 review). Values still come from the model's Cells (numbers,
// precisions, refs); only the grouping into lines is decided here.
void draw_firm_detail(App& app, const DetailModel& d) {
    if (!d.present) return;

    ImGui::SeparatorText("selected firm");
    ImGui::Text("firm %u", d.id);

    const Cell& wage = find_field(d, "wage");
    const Cell& price = find_field(d, "price");
    const Cell& inventory = find_field(d, "inventory");
    ImGui::Text("wage %.*f   price %.*f   inventory %.*f", wage.precision, wage.number,
                price.precision, price.number, inventory.precision, inventory.number);

    const Cell& liquidity = find_field(d, "liquidity");
    ImGui::Text("liquidity %.*f", liquidity.precision, liquidity.number);

    const Cell& demand_this = find_field(d, "demand this month");
    const Cell& demand_last = find_field(d, "demand last month");
    ImGui::Text("demand this month %.*f, last month %.*f", demand_this.precision,
                demand_this.number, demand_last.precision, demand_last.number);

    const Cell& staffed = find_field(d, "months fully staffed");
    ImGui::Text("months fully staffed %u", static_cast<uint32_t>(staffed.number));

    const Cell& vacancy_open = find_field(d, "vacancy open");
    const Cell& vacancy_unfilled = find_field(d, "vacancy went unfilled");
    ImGui::Text("vacancy open %s, went unfilled %s",
                vacancy_open.number != 0.0 ? "yes" : "no",
                vacancy_unfilled.number != 0.0 ? "yes" : "no");

    const Cell& pending_fire = find_field(d, "pending fire");
    if (pending_fire.kind != CellKind::Missing) {
        ImGui::Text("pending fire: household %u", pending_fire.ref);
    }

    if (const LinkGroup* employees = find_group(d, "employees"); employees != nullptr) {
        ImGui::Text("employees (%zu):", employees->links.size());
        char buf[16];
        for (const DetailLink& link : employees->links) {
            ImGui::SameLine();
            std::snprintf(buf, sizeof(buf), "%u", link.target);
            if (ImGui::SmallButton(buf)) app.selected_household = link.target;
        }
    }
}

void draw_household_detail(App& app, const DetailModel& d) {
    if (!d.present) return;

    ImGui::SeparatorText("selected household");
    ImGui::Text("household %u", d.id);

    const Cell& liquidity = find_field(d, "liquidity");
    const Cell& reservation_wage = find_field(d, "reservation wage");
    ImGui::Text("liquidity %.*f   reservation wage %.*f", liquidity.precision, liquidity.number,
                reservation_wage.precision, reservation_wage.number);

    const Cell& planned_consumption = find_field(d, "planned consumption");
    ImGui::Text("planned consumption %.*f", planned_consumption.precision,
                planned_consumption.number);

    const LinkGroup* employer = find_group(d, "employer");
    if (employer != nullptr && !employer->links.empty()) {
        ImGui::Text("employer:");
        ImGui::SameLine();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u", employer->links[0].target);
        if (ImGui::SmallButton(buf)) app.selected_firm = employer->links[0].target;
    } else {
        ImGui::TextUnformatted("unemployed");
    }

    ImGui::TextUnformatted("shops:");
    if (const LinkGroup* shops = find_group(d, "shops"); shops != nullptr) {
        // Sized for GCC's worst-case %zu width, not the real (small) shop
        // count -- the printed text is unaffected.
        char buf[40];
        for (size_t s = 0; s < shops->links.size(); ++s) {
            const DetailLink& link = shops->links[s];
            ImGui::SameLine();
            std::snprintf(buf, sizeof(buf), "%u##shop%zu", link.target, s);
            if (ImGui::SmallButton(buf)) app.selected_firm = link.target;
            if (link.has_annotation) {
                ImGui::SameLine();
                ImGui::TextDisabled("(-%.1f)", link.annotation);
            }
        }
    }
}

void draw_detail(App& app) {
    const World& w = app.sim.world();
    draw_firm_detail(app, firm_detail(w, app.selected_firm));
    draw_household_detail(app, household_detail(w, app.selected_household));
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
