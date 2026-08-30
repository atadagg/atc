#include "intervention.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "world.hpp"

namespace intervention {
namespace {

// One row per behavioural parameter. `Member` carries the field's real type,
// so an integer parameter converts once here rather than at every call site.
template <auto Member>
constexpr ParamSpec spec(std::string_view name, double lo, double hi) {
    using Field = std::remove_reference_t<decltype(std::declval<Params&>().*Member)>;
    return ParamSpec{
        name,
        [](Params& p, double v) { p.*Member = static_cast<Field>(v); },
        [](const Params& p) { return static_cast<double>(p.*Member); },
        lo,
        hi,
    };
}

// A probability is [0, 1]; the rest are bounded generously enough to explore
// with and tightly enough to catch a typo in a hand-written scenario.
constexpr double kProb = 1.0;

constexpr ParamSpec kSpecs[] = {
    spec<&Params::price_switch_threshold>("price_switch_threshold", 0.0, kProb),
    spec<&Params::unemployed_firm_visits>("unemployed_firm_visits", 0.0, 100.0),
    spec<&Params::employed_search_prob>("employed_search_prob", 0.0, kProb),
    spec<&Params::consumption_exponent>("consumption_exponent", 0.0, kProb),
    spec<&Params::max_shop_visits_per_day>("max_shop_visits_per_day", 0.0, static_cast<double>(params::kShopsPerHousehold)),
    spec<&Params::prob_price_search>("prob_price_search", 0.0, kProb),
    spec<&Params::prob_quant_search>("prob_quant_search", 0.0, kProb),
    spec<&Params::unemployed_reservation_cut>("unemployed_reservation_cut", 0.0, kProb),
    spec<&Params::months_before_wage_cut>("months_before_wage_cut", 0.0, 240.0),
    spec<&Params::wage_adjust_max>("wage_adjust_max", 0.0, kProb),
    spec<&Params::inventory_upper_factor>("inventory_upper_factor", 0.0, 10.0),
    spec<&Params::inventory_lower_factor>("inventory_lower_factor", 0.0, 10.0),
    spec<&Params::price_upper_factor>("price_upper_factor", 0.0, 10.0),
    spec<&Params::price_lower_factor>("price_lower_factor", 0.0, 10.0),
    spec<&Params::price_adjust_max>("price_adjust_max", 0.0, kProb),
    spec<&Params::price_change_prob>("price_change_prob", 0.0, kProb),
    spec<&Params::productivity>("productivity", 0.0, 100.0),
    spec<&Params::buffer_factor>("buffer_factor", 0.0, kProb),
    spec<&Params::demand_satisfied_fraction>("demand_satisfied_fraction", 0.0, kProb),
    spec<&Params::max_emergency_wage_cut>("max_emergency_wage_cut", 0.0, kProb),
    spec<&Params::min_wage>("min_wage", 0.0, 1000.0),
};

}  // namespace

std::span<const ParamSpec> param_specs() { return std::span<const ParamSpec>(kSpecs); }

bool param_in_range(uint16_t param, double value) {
    if (param >= std::size(kSpecs)) return false;
    return value >= kSpecs[param].lo && value <= kSpecs[param].hi;
}

uint16_t find_param(std::string_view name) {
    for (uint16_t i = 0; i < std::size(kSpecs); ++i) {
        if (kSpecs[i].name == name) return i;
    }
    return kNoParam;
}

void apply(World& w, const Intervention& i, ledger::Journal* journal) {
    switch (i.action) {
        case Action::SetParam:
            if (i.param >= std::size(kSpecs)) {
                throw std::out_of_range("intervention::apply: no such parameter");
            }
            kSpecs[i.param].set(w.params, i.value);
            return;

        case Action::Issue:
            ledger::transfer(w, ledger::issuer(i.issuer), ledger::household(i.target),
                             i.value, ledger::Reason::Issuance, journal);
            return;

        case Action::IssueToAllHouseholds:
            for (uint32_t h = 0; h < w.households.size(); ++h) {
                ledger::transfer(w, ledger::issuer(i.issuer), ledger::household(h),
                                 i.value, ledger::Reason::Issuance, journal);
            }
            return;
    }
    throw std::invalid_argument("intervention::apply: unknown action");
}

const Intervention& Log::add(Intervention i) {
    i.seq = next_seq_++;
    // Kept sorted by (month, seq). Appends are the common case; an insertion
    // for an earlier month is a scenario being assembled out of order, which
    // is allowed and must still land in the right place.
    auto at = std::upper_bound(entries_.begin(), entries_.end(), i.month,
                               [](int m, const Intervention& e) { return m < e.month; });
    return *entries_.insert(at, i);
}

void Log::set_param(int month, std::string_view name, double value) {
    const uint16_t id = find_param(name);
    if (id == kNoParam) {
        throw std::invalid_argument("intervention::Log::set_param: no such parameter");
    }
    if (!param_in_range(id, value)) {
        throw std::out_of_range("intervention::Log::set_param: value outside the range of '" +
                                std::string(name) + "'");
    }
    Intervention i;
    i.month = month;
    i.action = Action::SetParam;
    i.param = id;
    i.value = value;
    add(i);
}

void Log::issue(int month, uint32_t issuer, uint32_t household, double amount) {
    Intervention i;
    i.month = month;
    i.action = Action::Issue;
    i.issuer = issuer;
    i.target = household;
    i.value = amount;
    add(i);
}

void Log::issue_to_all_households(int month, uint32_t issuer, double each) {
    Intervention i;
    i.month = month;
    i.action = Action::IssueToAllHouseholds;
    i.issuer = issuer;
    i.value = each;
    add(i);
}

std::span<const Intervention> Log::due(int month) const {
    auto lo = std::lower_bound(entries_.begin(), entries_.end(), month,
                               [](const Intervention& e, int m) { return e.month < m; });
    auto hi = std::upper_bound(lo, entries_.end(), month,
                               [](int m, const Intervention& e) { return m < e.month; });
    return std::span<const Intervention>(&*lo, static_cast<size_t>(hi - lo));
}

}  // namespace intervention
