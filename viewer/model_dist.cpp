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

}  // namespace

std::span<const DistSpec> distributions() {
    return std::span<const DistSpec>(kDists);
}

std::vector<double> dist_values(const World& w, int which) {
    std::vector<double> v;
    switch (which) {
        case 0:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.wage);
            break;
        case 1:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.price);
            break;
        case 2:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.inventory);
            break;
        case 3:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) {
                v.push_back(static_cast<double>(f.employees.size()));
            }
            break;
        case 4:
            v.reserve(w.firms.size());
            for (const Firm& f : w.firms) v.push_back(f.liquidity);
            break;
        case 5:
            v.reserve(w.households.size());
            for (const Household& h : w.households) v.push_back(h.liquidity);
            break;
        default:
            v.reserve(w.households.size());
            for (const Household& h : w.households) v.push_back(h.reservation_wage);
            break;
    }
    return v;
}
