#include "model_agents.hpp"

#include <algorithm>
#include <utility>

#include "agents.hpp"
#include "params.hpp"
#include "world.hpp"

namespace {

constexpr ColumnSpec kFirmColumns[] = {
    {"firm",      CellKind::AgentRef, 0, true},
    {"wage",      CellKind::Number,   2, true},
    {"price",     CellKind::Number,   3, true},
    {"inventory", CellKind::Number,   1, true},
    {"staff",     CellKind::Count,    0, true},
    {"liquidity", CellKind::Number,   1, true},
    {"vacancy",   CellKind::Flag,     0, true},
    {"demand",    CellKind::Number,   1, true},
};

// Not sortable: the current table offers no sort. household_rows still
// honours a SortSpec -- whether to expose sorting is a skin's decision, and
// the model should not foreclose it.
constexpr ColumnSpec kHouseholdColumns[] = {
    {"household",           CellKind::AgentRef, 0, false},
    {"employer",            CellKind::AgentRef, 0, false},
    {"liquidity",           CellKind::Number,   2, false},
    {"reservation wage",    CellKind::Number,   2, false},
    {"planned consumption", CellKind::Number,   2, false},
};

bool firm_passes(const Filters& f, uint32_t i, const Firm& firm, double min_wage) {
    if (!f.pinned_firms.empty() &&
        std::find(f.pinned_firms.begin(), f.pinned_firms.end(), i) == f.pinned_firms.end()) {
        return false;
    }
    if (f.wage_at_floor && firm.wage > min_wage) return false;
    if (f.vacancy_open && !firm.has_open_vacancy) return false;
    if (f.inventory_zero && firm.inventory > 0.0) return false;
    return true;
}

}  // namespace

std::span<const ColumnSpec> firm_columns() {
    return std::span<const ColumnSpec>(kFirmColumns);
}

Cell firm_cell(const World& w, uint32_t firm, int col) {
    const Firm& f = w.firms[firm];
    switch (col) {
        case 0: return ref_cell(firm);
        case 1: return number_cell(f.wage, 2);
        case 2: return number_cell(f.price, 3);
        case 3: return number_cell(f.inventory, 1);
        case 4: return count_cell(f.employees.size());
        case 5: return number_cell(f.liquidity, 1);
        case 6: return flag_cell(f.has_open_vacancy);
        default: return number_cell(f.demand_last_month, 1);
    }
}

std::vector<uint32_t> firm_rows(const World& w, const Filters& f, SortSpec sort) {
    std::vector<uint32_t> rows;
    rows.reserve(w.firms.size());
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        if (firm_passes(f, i, w.firms[i], w.params.min_wage)) rows.push_back(i);
    }

    const int columns = static_cast<int>(firm_columns().size());
    if (sort.column < 0 || sort.column >= columns) return rows;

    // Stable, so equal keys keep index order and the table does not shuffle
    // under the reader between frames.
    std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
        const Cell ca = firm_cell(w, a, sort.column);
        const Cell cb = firm_cell(w, b, sort.column);
        return sort.ascending ? cell_less(ca, cb) : cell_less(cb, ca);
    });
    return rows;
}

std::span<const ColumnSpec> household_columns() {
    return std::span<const ColumnSpec>(kHouseholdColumns);
}

Cell household_cell(const World& w, uint32_t household, int col) {
    const Household& h = w.households[household];
    switch (col) {
        case 0: return ref_cell(household);
        // Absence as a value. The sentinel must never reach a skin, which
        // would print 4294967295 or sort it as the largest employer index.
        case 1: return h.employed() ? ref_cell(h.employer) : missing_cell();
        case 2: return number_cell(h.liquidity, 2);
        case 3: return number_cell(h.reservation_wage, 2);
        default: return number_cell(h.planned_consumption, 2);
    }
}

std::vector<uint32_t> household_rows(const World& w, const Filters& f, SortSpec sort) {
    std::vector<uint32_t> rows;
    rows.reserve(w.households.size());
    for (uint32_t i = 0; i < w.households.size(); ++i) {
        if (f.unemployed && w.households[i].employed()) continue;
        rows.push_back(i);
    }

    const int columns = static_cast<int>(household_columns().size());
    if (sort.column < 0 || sort.column >= columns) return rows;

    std::stable_sort(rows.begin(), rows.end(), [&](uint32_t a, uint32_t b) {
        const Cell ca = household_cell(w, a, sort.column);
        const Cell cb = household_cell(w, b, sort.column);
        return sort.ascending ? cell_less(ca, cb) : cell_less(cb, ca);
    });
    return rows;
}

DetailModel firm_detail(const World& w, uint32_t firm) {
    DetailModel d;
    if (firm == NO_FIRM || firm >= w.firms.size()) return d;

    const Firm& f = w.firms[firm];
    d.present = true;
    d.kind = AgentKind::Firm;
    d.id = firm;
    d.fields = {
        {"wage", number_cell(f.wage, 4)},
        {"price", number_cell(f.price, 4)},
        {"inventory", number_cell(f.inventory, 3)},
        {"liquidity", number_cell(f.liquidity, 3)},
        {"demand this month", number_cell(f.demand_this_month, 2)},
        {"demand last month", number_cell(f.demand_last_month, 2)},
        {"months fully staffed", count_cell(f.months_fully_staffed)},
        {"vacancy open", flag_cell(f.has_open_vacancy)},
        {"vacancy went unfilled", flag_cell(f.vacancy_went_unfilled)},
    };
    if (f.pending_fire != NO_HOUSEHOLD) {
        d.fields.push_back({"pending fire", ref_cell(f.pending_fire)});
    }

    LinkGroup employees{"employees", {}};
    employees.links.reserve(f.employees.size());
    for (uint32_t e : f.employees) {
        employees.links.push_back({e, AgentKind::Household, false, 0.0});
    }
    d.groups.push_back(std::move(employees));
    return d;
}

DetailModel household_detail(const World& w, uint32_t household) {
    DetailModel d;
    if (household == NO_HOUSEHOLD || household >= w.households.size()) return d;

    const Household& h = w.households[household];
    d.present = true;
    d.kind = AgentKind::Household;
    d.id = household;
    d.fields = {
        {"liquidity", number_cell(h.liquidity, 4)},
        {"reservation wage", number_cell(h.reservation_wage, 4)},
        {"planned consumption", number_cell(h.planned_consumption, 4)},
        {"employer", h.employed() ? ref_cell(h.employer) : missing_cell()},
    };

    if (h.employed()) {
        d.groups.push_back({"employer", {{h.employer, AgentKind::Firm, false, 0.0}}});
    }

    LinkGroup shops{"shops", {}};
    shops.links.reserve(h.shops.size());
    for (size_t s = 0; s < h.shops.size(); ++s) {
        const double unmet = h.unsatisfied[s];
        shops.links.push_back({h.shops[s], AgentKind::Firm, unmet > 0.0, unmet});
    }
    d.groups.push_back(std::move(shops));
    return d;
}
