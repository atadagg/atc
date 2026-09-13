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
    // A dead firm is left in place rather than erased: every cross-agent
    // reference is an index (invariant 3), and erasing would invalidate every
    // index above it. Dead firms are skipped, not removed.
    bool alive = true;

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

    // Input stock, one slot per recipe input, parallel to Good::inputs.
    std::array<double, params::kMaxInputs> input_stock{};

    // What this firm still wants of each input this month, set at month open
    // from planned output and drawn down as it buys. The firm-side analogue
    // of Household::planned_consumption.
    std::array<double, params::kMaxInputs> input_demand{};

    // Supplier links: kSuppliersPerInput slots per input, laid out
    // input-major, so input i owns slots [i*kSuppliersPerInput,
    // (i+1)*kSuppliersPerInput). NO_FIRM means the slot is empty.
    std::array<uint32_t, params::kMaxInputs * params::kSuppliersPerInput> suppliers{};

    // Unmet input demand per supplier slot, parallel to `suppliers`. Drives
    // supplier rewiring exactly as Household::unsatisfied drives shop
    // rewiring.
    std::array<double, params::kMaxInputs * params::kSuppliersPerInput> input_unsatisfied{};

    // Who this firm owes, and how much, for inputs bought this month. Slots
    // are parallel to `suppliers`. One month of credit -- everything here
    // settles at month close, so no per-invoice due dates are needed.
    std::array<uint32_t, params::kMaxInputs * params::kSuppliersPerInput> payable_to{};
    std::array<double, params::kMaxInputs * params::kSuppliersPerInput> payable_amount{};

    // Trade credit. `payables` is owed to suppliers, `receivables` is owed by
    // customers; both settle at month close.
    double payables = 0.0;
    double receivables = 0.0;

    // Inventory is carried at cost, so equity does not move when a firm
    // merely re-prices its shelf. One moving-average cost per firm: a firm
    // makes one good, so one number is enough.
    double output_cost_basis = 0.0;
    std::array<double, params::kMaxInputs> input_cost_basis{};
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
