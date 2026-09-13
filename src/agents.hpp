#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "params.hpp"

inline constexpr uint32_t NO_FIRM = 0xFFFFFFFFu;
inline constexpr uint32_t NO_HOUSEHOLD = 0xFFFFFFFFu;

// Plain aggregate. No virtuals, no inheritance. Cross-agent references are
// indices, never pointers.
struct Firm {
    double liquidity = params::kInitialFirmLiquidity;
    double inventory = params::kInitialInventory;
    double price = params::kInitialPrice;
    double wage = params::kInitialWage;

    // Which good this firm makes. A firm makes exactly one.
    uint32_t good = 0;

    // Demand encountered this month, and last month's total (drives all rules).
    double demand_this_month = 0.0;
    double demand_last_month = 0.0;

    std::vector<uint32_t> employees;

    bool has_open_vacancy = false;
    bool vacancy_went_unfilled = false;
    uint32_t months_fully_staffed = 0;

    // Firing is decided this month, applied next month (job-protection lag).
    uint32_t pending_fire = NO_HOUSEHOLD;
};

struct Household {
    double liquidity = params::kInitialHouseholdLiquidity;
    double reservation_wage = params::kInitialWage;
    uint32_t employer = NO_FIRM;

    std::array<uint32_t, params::kShopsPerHousehold> shops{};

    // Planned real consumption for the month, and the daily slice of it.
    double planned_consumption = 0.0;

    // Per-shop unmet demand accumulated over the month, used for rewiring.
    std::array<double, params::kShopsPerHousehold> unsatisfied{};

    bool employed() const { return employer != NO_FIRM; }
};
