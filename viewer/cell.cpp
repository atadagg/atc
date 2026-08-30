#include "cell.hpp"

bool cell_less(const Cell& a, const Cell& b) {
    const bool a_missing = a.kind == CellKind::Missing;
    const bool b_missing = b.kind == CellKind::Missing;
    if (a_missing || b_missing) return a_missing && !b_missing;
    return a.number < b.number;
}
