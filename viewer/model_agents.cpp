#include "model_agents.hpp"

#include <algorithm>

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

bool firm_passes(const Filters& f, uint32_t i, const Firm& firm) {
    if (!f.pinned_firms.empty() &&
        std::find(f.pinned_firms.begin(), f.pinned_firms.end(), i) == f.pinned_firms.end()) {
        return false;
    }
    if (f.wage_at_floor && firm.wage > params::kMinWage) return false;
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
        if (firm_passes(f, i, w.firms[i])) rows.push_back(i);
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
