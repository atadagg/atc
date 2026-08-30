#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "cell.hpp"

class World;

// The firm table's schema. Column identity is the index into this span.
std::span<const ColumnSpec> firm_columns();

// The firms to show, filtered and sorted. Returns agent indices -- the
// materialized part of the view model. Cells are pulled per row on demand,
// so a skin that draws only visible rows formats only visible rows.
std::vector<uint32_t> firm_rows(const World& w, const Filters& f, SortSpec sort);

// One cell. `col` indexes firm_columns(). Out-of-range columns return the
// last column rather than reading out of bounds.
Cell firm_cell(const World& w, uint32_t firm, int col);

// The household table's schema. Column identity is the index into this span.
std::span<const ColumnSpec> household_columns();

std::vector<uint32_t> household_rows(const World& w, const Filters& f, SortSpec sort);

Cell household_cell(const World& w, uint32_t household, int col);
