#include "model_steering.hpp"

#include <sstream>

#include "world.hpp"

namespace {

// Trailing zeros make a log of parameter changes unreadable at a glance.
std::string trim_number(double v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

}  // namespace

std::vector<SteeringRow> steering_rows(const World& w) {
    auto specs = intervention::param_specs();
    std::vector<SteeringRow> rows;
    rows.reserve(specs.size());
    for (const auto& s : specs) {
        rows.push_back(SteeringRow{s.name, s.get(w.params), s.lo, s.hi});
    }
    return rows;
}

std::string describe(const intervention::Intervention& i) {
    std::ostringstream out;
    out << "month " << i.month << ": ";
    switch (i.action) {
        case intervention::Action::SetParam:
            out << "set " << intervention::param_specs()[i.param].name << " = "
                << trim_number(i.value);
            break;
        case intervention::Action::Issue:
            out << "issuer " << i.issuer << " gives household " << i.target << ' '
                << trim_number(i.value);
            break;
        case intervention::Action::IssueToAllHouseholds:
            out << "issuer " << i.issuer << " gives every household "
                << trim_number(i.value);
            break;
    }
    return out.str();
}
