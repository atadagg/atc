#pragma once

#include <cstdint>

#include "agents.hpp"
#include "params.hpp"
#include "rng.hpp"

class World;

namespace firm_decisions {

// Cost of producing one unit: LABOUR PLUS MATERIALS.
//
// Labour: one worker costs `wage` per month and yields
// `productivity * days_per_month / labour_per_unit` units (the same quantity
// `daily::produce` calls `by_labour`), so the labour term is
// `wage / (productivity * days) * labour_per_unit` and is independent of
// headcount.
//
// Materials: the recipe's `input_qty[s]` times what one unit of input `s`
// costs. Three sources, in order of how much the firm actually knows:
//   1. `Firm::input_cost_basis[s]` -- what this firm has actually paid, the
//      moving average `daily::buy` maintains.
//   2. the good's published `market::GoodMarket::reference_price` -- last
//      month's realised average, used before the firm has ever bought.
//   3. the mean posted price of the input's living producers -- the cold
//      start, before any month has traded, when neither of the above exists.
// Only if the input has NO living producer at all is the term dropped, and
// then the recipe cannot be made regardless. Treating an unknown input as
// free is what made every downstream firm insolvent by construction; it is
// deliberately the last resort and not silent (see CLAUDE.md).
//
// `w` is optional. With no world there is no recipe and no market, so the
// result is the labour term alone -- which is exactly the legacy single-good
// value, since that good has `labour_per_unit == 1.0` and no inputs.
double marginal_cost(const Firm& f, const Params& p, const World* w = nullptr);

// The inventory comfort band computed from last month's demand:
// [inventory_lower_factor * demand_last_month, inventory_upper_factor *
// demand_last_month]. Shared by adjust_workforce and adjust_price so the two
// rules can never silently drift apart (spec §5.1).
struct InventoryBand {
    double lower;
    double upper;
};
InventoryBand inventory_band(const Firm& f, const Params& p);

void adjust_wage(Firm& f, const Params& p, Rng& rng);
void adjust_workforce(World& w, uint32_t firm, Rng& rng);
void adjust_price(Firm& f, const Params& p, Rng& rng, const World* w = nullptr);

// The commoditization pull. The firm's own gated rule above (raise only
// while below the cap, lower only while above the floor -- see CLAUDE.md, it
// is gated rather than clamped) decides what the firm WANTS. This blends
// that toward what the good actually trades at, weighted by how far the good
// sits from the consumer.
//
// At 0 this is the identity and today's economy is untouched. At 1 the firm
// is a pure price taker: iron ore sells at the iron ore price whatever its
// producer would prefer. Nothing here is an auctioneer -- the reference is
// last month's realised average, and a firm at the reference price can still
// fail to sell.
void pull_toward_reference(Firm& f, const World& w);

// Sets `input_demand[s]` for every living firm from its current headcount --
// the same labour capacity `daily::produce` computes (productivity *
// employees / labour_per_unit), extended over the month and priced in units
// of input `s` via the recipe's `input_qty[s]`. Empty recipe slots are
// skipped. A firm whose good has no inputs touches nothing.
void plan_inputs(World& w);

// Mirrors household_decisions::search_reliable_shop for firms: any supplier
// slot that is empty (NO_FIRM, including a newly founded firm's whole
// roster) or that rationed the firm this month (input_unsatisfied > 0) is
// replaced by a fresh draw from network::random_unconnected_producer. Clears
// input_unsatisfied afterwards, matching how Household::unsatisfied is
// consumed by shop rewiring and then cleared.
//
// A firm whose good has no material inputs returns before touching `rng` --
// that is what keeps the single-good legacy world's draw sequence, and
// therefore the golden hash, untouched.
void rewire_suppliers(World& w, Rng& rng);

// Runs the rules above for every firm, in index order.
void run_monthly(World& w, Rng& rng);

}  // namespace firm_decisions
