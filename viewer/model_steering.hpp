#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "intervention.hpp"

class World;

// What a steering UI needs to show, decided here rather than in whichever skin
// happens to be drawing it -- the same separation the rest of the view-model
// keeps. A row carries raw numbers, never a formatted string, so a slider, a
// text field and an ASCII table can all render the same decision.
struct SteeringRow {
    std::string_view name;
    double value;  // this World's current value, not the default
    double lo;
    double hi;
};

std::vector<SteeringRow> steering_rows(const World& w);

// One line of the audit trail: what an operator did, in words. This one *is* a
// string, because it is prose rather than a quantity -- there is no sort key
// hiding in it.
std::string describe(const intervention::Intervention& i);
