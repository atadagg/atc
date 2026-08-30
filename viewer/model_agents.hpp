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

struct DetailField {
    const char* label;
    Cell value;
};

// A navigable reference out of a detail view. The annotation carries a
// number that belongs to the link rather than to the target -- a shop's
// unmet demand, for instance.
struct DetailLink {
    uint32_t target;
    AgentKind kind;
    bool has_annotation;
    double annotation;
};

struct LinkGroup {
    const char* label;
    std::vector<DetailLink> links;
};

// Convention, not a rule enforced by the type system: a relationship that
// can hold zero, one, or many targets (a firm's employees, a household's
// shops) is a LinkGroup, even when it happens to hold exactly one. A
// relationship that is inherently singular and optional (a firm's pending
// fire) is emitted only as an AgentRef DetailField. Both are equally
// navigable -- the distinction is just where the value lives, not whether
// it is clickable -- so a household's employer appears as *both* an
// AgentRef field and a one-element LinkGroup, while pending_fire appears
// only as a field. This is existing screen behaviour and is not changed
// here; a skin must not assume a ref field's target is absent from groups.
//
// What a skin needs to render the selected agent. The link graph is data;
// only the button is ImGui.
struct DetailModel {
    bool present = false;  // false when nothing valid is selected
    AgentKind kind = AgentKind::Firm;
    uint32_t id = 0;
    std::vector<DetailField> fields;
    std::vector<LinkGroup> groups;
};

DetailModel firm_detail(const World& w, uint32_t firm);
DetailModel household_detail(const World& w, uint32_t household);
