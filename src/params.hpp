#pragma once

#include <cstdint>

namespace params {

// =====================================================================
// Published calibration (Lengnick 2013 Table 1, plus the paper's stated
// population/network setup) — do not tune. These values come from the
// published paper and must match it exactly.
// =====================================================================

// --- Time ---
inline constexpr int kDaysPerMonth = 21;

// --- Population ---
inline constexpr uint32_t kNumHouseholds = 1000;
inline constexpr uint32_t kNumFirms = 100;
inline constexpr uint32_t kShopsPerHousehold = 7;

// --- Households (Lengnick 2013 Table 1) ---
inline constexpr double kPriceSwitchThreshold = 0.01;   // xi
inline constexpr uint32_t kUnemployedFirmVisits = 5;    // beta
inline constexpr double kEmployedSearchProb = 0.1;      // pi
inline constexpr double kConsumptionExponent = 0.9;     // alpha
inline constexpr uint32_t kMaxShopVisitsPerDay = 7;     // n
inline constexpr double kProbPriceSearch = 0.25;        // psi_Price
inline constexpr double kProbQuantSearch = 0.25;        // psi_Quant
inline constexpr double kUnemployedReservationCut = 0.10;

// --- Firms (Lengnick 2013 Table 1) ---
inline constexpr uint32_t kMonthsBeforeWageCut = 24;    // gamma
inline constexpr double kWageAdjustMax = 0.019;         // delta
inline constexpr double kInventoryUpperFactor = 1.0;    // phi upper
inline constexpr double kInventoryLowerFactor = 0.25;   // phi lower
inline constexpr double kPriceUpperFactor = 1.15;       // varphi upper
inline constexpr double kPriceLowerFactor = 1.025;      // varphi lower
inline constexpr double kPriceAdjustMax = 0.02;         // vartheta
inline constexpr double kPriceChangeProb = 0.75;        // theta
inline constexpr double kProductivity = 3.0;            // lambda
inline constexpr double kBufferFactor = 0.1;            // chi

// --- Shopping ---
inline constexpr double kDemandSatisfiedFraction = 0.95;

// =====================================================================
// Everything above this line is published calibration (Lengnick 2013
// Table 1) — do not tune. Everything below is initial conditions and run
// configuration: not specified by the paper, chosen for this
// implementation, and safe to adjust without touching the model itself.
// =====================================================================

// --- Initial conditions (not specified by the paper) ---
inline constexpr double kInitialFirmLiquidity = 0.0;
inline constexpr double kInitialHouseholdLiquidity = 100.0;
inline constexpr double kInitialPrice = 1.0;
inline constexpr double kInitialWage = 30.0;
inline constexpr double kInitialInventory = 0.0;

// --- Run configuration (not specified by the paper) ---
inline constexpr int kBurnInMonths = 1000;
inline constexpr int kMeasurementMonths = 6000;

}  // namespace params
