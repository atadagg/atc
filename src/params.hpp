#pragma once

#include <cstdint>

namespace params {

// =====================================================================
// STRUCTURAL — compile-time constants, and they must stay that way.
//
// These define memory layout and the shape of the tick, not behaviour.
// `kShopsPerHousehold` is the size of the array inside every Household;
// `kDaysPerMonth` is the tick shape. Making one of these runtime state
// would stop `Household::shops` being a fixed array, which is what keeps
// agents trivially copyable and therefore what makes the viewer's
// backward scrub byte-exact (invariants 3 and 4).
//
// The test for this pile: "could a UI slider move it mid-run?" If no,
// it belongs here.
// =====================================================================

// --- Time ---
inline constexpr int kDaysPerMonth = 21;

// --- Population ---
inline constexpr uint32_t kNumHouseholds = 1000;
inline constexpr uint32_t kNumFirms = 100;
inline constexpr uint32_t kShopsPerHousehold = 7;

// Recipe and supplier-link capacity. Structural for the same reason as
// kShopsPerHousehold: these size fixed arrays inside Good and Firm, and a
// runtime size would stop agents being trivially copyable.
inline constexpr uint32_t kMaxInputs = 3;
inline constexpr uint32_t kSuppliersPerInput = 3;

// Shareholder capacity. Structural for the same reason as the above: this
// sizes the fixed arrays inside Firm that hold the register. A firm's shares
// are dealt once, at founding, and never move, so the register can never
// outgrow its slots.
//
// The two holder counts are the shape of the deal, not a rate: a private firm
// is closely held, a public one is held by dozens. They are read at founding,
// which does happen mid-run -- but they choose an array length, and a length
// is not something a slider may move. `Params::share_skew` is the part of the
// deal a slider does move.
inline constexpr uint32_t kMaxHolders = 32;
inline constexpr uint32_t kMinPublicHolders = 8;
inline constexpr uint32_t kMaxPrivateHolders = 2;

// =====================================================================
// INITIAL CONDITIONS — compile-time, and structural in the same sense.
//
// Not specified by the paper; chosen for this implementation. They are
// consumed once, at construction (several are default member initializers
// on `Firm`/`Household` in agents.hpp), so "change it at month 300" is not
// a meaningful operation. They fail the slider test for a different reason
// than the layout constants above, but they fail it.
// =====================================================================

inline constexpr double kInitialFirmLiquidity = 0.0;
inline constexpr double kInitialHouseholdLiquidity = 100.0;
inline constexpr double kInitialPrice = 1.0;
inline constexpr double kInitialWage = 30.0;
inline constexpr double kInitialInventory = 0.0;

// =====================================================================
// RUN CONFIGURATION — CLI defaults, not simulation parameters.
// =====================================================================

inline constexpr int kBurnInMonths = 1000;
inline constexpr int kMeasurementMonths = 6000;

}  // namespace params

// =====================================================================
// BEHAVIOURAL — runtime state, one copy per World (`World::params`).
//
// Every rate, threshold and probability the model consults while running.
// These are runtime rather than `constexpr` so that a parameter can change
// mid-run without breaking invariant 2 (determinism): a run is then defined
// by (seed, intervention log) rather than by the seed alone, and because
// `Params` lives inside `World` it is carried by `SimSnapshot` for free —
// restoring month 400 restores the parameters as of month 400, which is the
// detail that makes the viewer's backward scrub correct across a parameter
// change.
//
// The defaults are Lengnick 2013 Table 1 and must match the paper exactly.
// The Greek letter in each comment is the paper's symbol.
//
// Must stay a plain aggregate: `SimSnapshot` is a plain assignment, and a
// non-copyable member here is the line that stops it compiling.
// =====================================================================

struct Params {
    // --- Households (Lengnick 2013 Table 1) ---
    double price_switch_threshold = 0.01;   // xi
    uint32_t unemployed_firm_visits = 5;    // beta
    double employed_search_prob = 0.1;      // pi
    double consumption_exponent = 0.9;      // alpha
    uint32_t max_shop_visits_per_day = 7;   // n
    double prob_price_search = 0.25;        // psi_Price
    double prob_quant_search = 0.25;        // psi_Quant
    double unemployed_reservation_cut = 0.10;

    // --- Firms (Lengnick 2013 Table 1) ---
    uint32_t months_before_wage_cut = 24;   // gamma
    double wage_adjust_max = 0.019;         // delta
    double inventory_upper_factor = 1.0;    // phi upper
    double inventory_lower_factor = 0.25;   // phi lower
    double price_upper_factor = 1.15;       // varphi upper
    double price_lower_factor = 1.025;      // varphi lower
    double price_adjust_max = 0.02;         // vartheta
    double price_change_prob = 0.75;        // theta
    double productivity = 3.0;              // lambda
    double buffer_factor = 0.1;             // chi

    // --- Shopping (not in Table 1) ---
    double demand_satisfied_fraction = 0.95;

    // --- Emergency wage cut bounds (not specified by the paper) ---
    // Lengnick's no-bankruptcy rule (§2.4) cuts the wage to whatever the firm
    // can afford. Unbounded, a firm that sells nothing repeatedly cuts by ~90%
    // a month and reaches zero -- and zero is absorbing, because every recovery
    // channel in firm_decisions::adjust_wage is multiplicative. A zero wage also
    // collapses the price band to [0, 0], permanently disabling that firm's
    // price-raise gate. These two bounds keep the rule intact while removing
    // the absorbing state.
    double max_emergency_wage_cut = 0.5;
    double min_wage = 1.0;

    // --- Supply chain (not in Table 1) ---
    // How fast a good becomes commodity-like as it moves upstream, in
    // 1 - exp(-k * remove). Behavioural: a slider could move it mid-run, and
    // moving it re-prices the whole chain.
    double commoditization_k = 0.7;

    // Firm entry. Default 0 so the single-good legacy world sees no entry at
    // all and its draw sequence is untouched; the multi-good configurations
    // raise it. Behavioural -- a slider could move it, and moving it changes
    // how fast a shortage is competed away.
    double firm_entry_prob = 0.0;
    double firm_entry_margin = 1.2;     // reference price must exceed cost by this
    double firm_entry_capital = 100.0;  // cash a founded firm starts with
    // Ceiling on LIVING producers of one good, not on firms ever assigned to
    // it -- a corpse must not count, and a replacement must not be blocked
    // forever by the incumbent it replaces. This is what lets the ceiling
    // reopen as firms die rather than only ratchet upward.
    double firm_entry_max_per_good = 8.0;
    // Ceiling on how many firms may be founded across ALL goods in one
    // month, so one price shock cannot spawn a whole new sector at once.
    double firm_entry_max_per_month = 5.0;

    // --- Ownership (not in Table 1) ---
    // How likely a firm is dealt as a public company -- held by dozens --
    // rather than closely held by one or two households. Read at founding,
    // which happens mid-run, so it is behavioural.
    double public_firm_fraction = 0.3;
    // How steeply a firm's shares are skewed across its holders. Holder k of
    // n receives weight `share_skew^k`, normalised, so 1.0 is an equal split
    // and smaller is more concentrated. This is the dial on how unequal the
    // starting deal is -- and, because holdings never move, on how unequal
    // ownership stays.
    double share_skew = 0.6;
};
