#include "model_dist.hpp"

#include "agents.hpp"
#include "world.hpp"

namespace {

constexpr DistSpec kDists[] = {
    {"firm wage",                  AgentKind::Firm,      true},
    {"firm price",                 AgentKind::Firm,      true},
    {"firm inventory",             AgentKind::Firm,      true},
    {"firm headcount",             AgentKind::Firm,      true},
    {"firm liquidity",             AgentKind::Firm,      true},
    {"household liquidity",        AgentKind::Household, false},
    {"household reservation wage", AgentKind::Household, false},
};

// Builds the (value, real agent index) pairs behind distributions()[which] in
// one pass, so dist_values() and dist_indices() cannot drift apart -- there
// is exactly one place that decides which agents a distribution covers.
//
// Dead firms are skipped here, not zeroed and kept: a corpse's wage and price
// are stale, not zero, and letting them into a bin is the same averaging
// trap recorder.cpp::compute_metrics already excludes them from. Households
// never die, so household rows are unfiltered.
struct DistRows {
    std::vector<double> values;
    std::vector<uint32_t> indices;
};

DistRows dist_rows(const World& w, int which) {
    DistRows r;

    auto firm_field = [&](auto&& field) {
        r.values.reserve(w.firms.size());
        r.indices.reserve(w.firms.size());
        for (uint32_t i = 0; i < w.firms.size(); ++i) {
            const Firm& f = w.firms[i];
            if (!f.alive) continue;
            r.values.push_back(static_cast<double>(field(f)));
            r.indices.push_back(i);
        }
    };
    auto household_field = [&](auto&& field) {
        r.values.reserve(w.households.size());
        r.indices.reserve(w.households.size());
        for (uint32_t i = 0; i < w.households.size(); ++i) {
            r.values.push_back(static_cast<double>(field(w.households[i])));
            r.indices.push_back(i);
        }
    };

    switch (which) {
        case 0: firm_field([](const Firm& f) { return f.wage; }); break;
        case 1: firm_field([](const Firm& f) { return f.price; }); break;
        case 2: firm_field([](const Firm& f) { return f.inventory; }); break;
        case 3: firm_field([](const Firm& f) { return f.employees.size(); }); break;
        case 4: firm_field([](const Firm& f) { return f.liquidity; }); break;
        case 5: household_field([](const Household& h) { return h.liquidity; }); break;
        default: household_field([](const Household& h) { return h.reservation_wage; }); break;
    }
    return r;
}

}  // namespace

std::span<const DistSpec> distributions() {
    return std::span<const DistSpec>(kDists);
}

std::vector<double> dist_values(const World& w, int which) {
    return dist_rows(w, which).values;
}

std::vector<uint32_t> dist_indices(const World& w, int which) {
    return dist_rows(w, which).indices;
}
