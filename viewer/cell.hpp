#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// What a cell means, independent of how any skin draws it. A skin decides
// pixels; this decides significance. Nothing display-specific belongs here.
enum class CellKind {
    Number,    // a real quantity, rendered to `precision` decimals
    Count,     // a whole number of things
    Flag,      // a boolean
    AgentRef,  // an agent index: navigable, not merely printable
    Identity,  // a firm's logo and name, derived from the run seed and index
    Missing,   // no value exists (an unemployed household's employer)
};

struct Cell {
    CellKind kind = CellKind::Number;
    // Populated for EVERY kind -- flags as 0/1, counts and refs as their
    // value -- so one comparator sorts any column and no skin ever sorts
    // formatted text.
    double number = 0.0;
    uint32_t ref = 0;    // AgentRef only
    int precision = 2;   // Number only
};

inline Cell number_cell(double v, int precision) {
    return Cell{CellKind::Number, v, 0u, precision};
}
inline Cell count_cell(size_t n) {
    return Cell{CellKind::Count, static_cast<double>(n), 0u, 0};
}
inline Cell flag_cell(bool b) {
    return Cell{CellKind::Flag, b ? 1.0 : 0.0, 0u, 0};
}
inline Cell ref_cell(uint32_t r) {
    return Cell{CellKind::AgentRef, static_cast<double>(r), r, 0};
}
// The firm index is the whole payload: identity is a pure function of
// (run seed, index), so a skin resolves the mark and the name itself rather
// than carrying either through the view model. Sorting by that index is
// therefore sorting by identity -- stable, and the order marks are generated
// in. Alphabetical order over a name is a locale question, not a Cell one.
inline Cell identity_cell(uint32_t firm) {
    return Cell{CellKind::Identity, static_cast<double>(firm), firm, 0};
}
inline Cell missing_cell() {
    return Cell{CellKind::Missing, 0.0, 0u, 0};
}

// Strict weak ordering. Missing sorts below every present value regardless
// of the numbers involved, and two Missing cells are equivalent.
bool cell_less(const Cell& a, const Cell& b);

enum class AgentKind { Firm, Household };

struct ColumnSpec {
    const char* header;
    CellKind kind;
    int precision;
    bool sortable;
};

struct SortSpec {
    int column = -1;  // -1 means unsorted: rows keep ascending index order
    bool ascending = true;
};

// Which agents the tables show. Lives here rather than in App because the
// model layer must not know a running app exists.
struct Filters {
    bool wage_at_floor = false;
    bool vacancy_open = false;
    bool inventory_zero = false;
    bool unemployed = false;
    // Non-empty means: show only these firms. Written by a histogram bin
    // click, cleared by the "clear" button.
    std::vector<uint32_t> pinned_firms;
};
