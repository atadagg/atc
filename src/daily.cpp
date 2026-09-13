#include "daily.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

#include "goods.hpp"
#include "ledger.hpp"
#include "market.hpp"
#include "network.hpp"
#include "params.hpp"
#include "world.hpp"

namespace daily {

namespace {

// The buyer's cash account, its supplier slots, and its unmet-demand slots.
// A household shops for the consumer good; a firm buys one input good. Both
// hold a fixed-size set of links and a parallel set of shortfall counters,
// which is why one routine can serve both.
struct BuyerView {
    const double* liquidity;
    const uint32_t* links;
    double* unsatisfied;
    uint32_t link_count;
    double daily_target;
    ledger::Account account;

    // Where a firm's purchases land. Null for a household: it consumes what
    // it buys rather than stockpiling it.
    //
    // There is deliberately no pointer to the monthly want here. `input_demand`
    // is the firm's analogue of `planned_consumption`: it is set at month open
    // and stands unchanged for the whole month, and the daily slice is
    // recomputed from it each day. Drawing it down as the firm bought made the
    // slice decay geometrically -- a firm with unlimited supply and cash filled
    // only ~64% of its monthly want.
    double* stock = nullptr;
    ledger::Reason reason = ledger::Reason::Purchase;

    // Non-null for a firm buyer: firms buy inputs on one month's trade
    // credit, so nothing moves through the ledger at purchase time. A
    // household pays cash -- consumers do not get trade credit.
    Firm* buyer_firm = nullptr;
    uint32_t input = 0;
    uint32_t slot_base = 0;  // where this input's slots start in the firm arrays
};

// Where a firm records a new bill from `seller`. Prefers the slot parallel to
// the supplier link it just bought from, then any slot already owed to that
// seller, then any free slot. Returns kNoSlot when the firm is out of slots,
// which the caller treats as "cannot buy" rather than as an untracked debt.
inline constexpr std::size_t kNoSlot = static_cast<std::size_t>(-1);

std::size_t payable_slot_for(Firm& f, std::size_t preferred, uint32_t seller) {
    if (f.payable_to[preferred] == seller || f.payable_to[preferred] == NO_FIRM) {
        return preferred;
    }
    for (std::size_t s = 0; s < f.payable_to.size(); ++s) {
        if (f.payable_to[s] == seller) return s;
    }
    for (std::size_t s = 0; s < f.payable_to.size(); ++s) {
        if (f.payable_to[s] == NO_FIRM) return s;
    }
    return kNoSlot;
}

// The widest set of links any buyer holds: a household's shops, or a firm's
// suppliers for one input.
inline constexpr uint32_t kMaxBuyerLinks =
    params::kShopsPerHousehold > params::kSuppliersPerInput ? params::kShopsPerHousehold
                                                            : params::kSuppliersPerInput;

BuyerView view_of(World& w, Buyer b) {
    if (b.kind == BuyerKind::Household) {
        Household& h = w.households[b.index];
        return BuyerView{&h.liquidity,
                         h.shops.data(),
                         h.unsatisfied.data(),
                         params::kShopsPerHousehold,
                         h.planned_consumption / params::kDaysPerMonth,
                         ledger::household(b.index)};
    }

    // A firm making a good with no material inputs buys nothing. The empty
    // view is formed WITHOUT touching w.firms, so this is also the answer for
    // a caller that has no firm in mind -- and, more importantly, `buy`
    // returns on link_count == 0 before it draws a single random number. That
    // is what keeps the single-good world byte-identical.
    const BuyerView none{nullptr, nullptr, nullptr, 0, 0.0, ledger::firm(b.index)};
    if (b.good == NO_GOOD) return none;

    Firm& f = w.firms[b.index];
    const Good& recipe = w.goods[f.good];

    // Which input slot group this good occupies in the recipe.
    uint32_t input = params::kMaxInputs;
    for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
        if (recipe.inputs[i] == b.good) { input = i; break; }
    }
    if (input == params::kMaxInputs) return none;

    const uint32_t base = input * params::kSuppliersPerInput;
    return BuyerView{&f.liquidity,
                     f.suppliers.data() + base,
                     f.input_unsatisfied.data() + base,
                     params::kSuppliersPerInput,
                     f.input_demand[input] / params::kDaysPerMonth,
                     ledger::firm(b.index),
                     &f.input_stock[input],
                     ledger::Reason::TradeCreditSettlement,
                     &f,
                     input,
                     base};
}

}  // namespace

void buy(World& w, Buyer b, Rng& rng, ledger::Journal* journal) {
    BuyerView view = view_of(w, b);
    if (view.link_count == 0) return;

    double target = view.daily_target;
    if (target <= 0.0) return;

    double remaining = target;
    const double stop_at = target * (1.0 - w.params.demand_satisfied_fraction);

    // Visit each of the buyer's link slots at most once per day, in a random
    // order (spec §5.2: retries go to *other* connected sellers, not repeated
    // draws with replacement from the same slot).
    std::array<uint32_t, kMaxBuyerLinks> order{};
    for (uint32_t s = 0; s < view.link_count; ++s) order[s] = s;
    for (std::size_t i = view.link_count - 1; i > 0; --i) {
        uint32_t j = rng.index(static_cast<uint32_t>(i + 1));
        std::swap(order[i], order[j]);
    }

    // The paper's n is a *behavioural* parameter but the visit order lives in
    // a fixed-size array (one slot per link), so a raised n cannot visit
    // sellers the buyer does not have.
    const uint32_t visits = std::min(w.params.max_shop_visits_per_day, view.link_count);
    for (uint32_t visit = 0; visit < visits; ++visit) {
        if (remaining <= stop_at) break;

        uint32_t slot = order[visit];
        uint32_t firm_idx = view.links[slot];
        // An empty supplier slot. Households never hold one; a firm does until
        // its supplier search fills it.
        if (firm_idx == NO_FIRM) continue;
        Firm& f = w.firms[firm_idx];

        const double wanted = remaining;

        // The firm learns what was wanted, whether or not it can supply it.
        f.demand_this_month += wanted;

        if (f.price <= 0.0) continue;

        double affordable = *view.liquidity / f.price;
        double bought = std::min({wanted, f.inventory, affordable});

        std::size_t credit_slot = kNoSlot;
        if (bought > 0.0 && view.buyer_firm != nullptr) {
            credit_slot = payable_slot_for(*view.buyer_firm,
                                           static_cast<std::size_t>(view.slot_base + slot), firm_idx);
            // Out of payable slots: the bill would be untracked debt, so the
            // firm does not buy. Unreachable in practice -- there are as many
            // slots as supplier links.
            if (credit_slot == kNoSlot) bought = 0.0;
        }

        if (bought > 0.0) {
            // Clamp the cost to available liquidity so floating-point division
            // can never push the buyer slightly negative.
            double cost = std::min(bought * f.price, *view.liquidity);

            if (view.buyer_firm != nullptr) {
                // Trade credit: no money moves. The bill is firm state on
                // both sides until month close settles it, which is why
                // `payables`/`receivables` are not accounts and invariant 1
                // is untouched here.
                Firm& b_f = *view.buyer_firm;
                b_f.payable_to[credit_slot] = firm_idx;
                b_f.payable_amount[credit_slot] += cost;
                b_f.payables += cost;
                f.receivables += cost;

                // Input stock is carried at a moving average cost over
                // quantity, so the balance sheet values what the firm holds
                // rather than leaving it at zero. Computed BEFORE the stock
                // grows, because the old quantity is the weight.
                const double held = b_f.input_stock[view.input];
                const double total = held + bought;
                if (total > 0.0) {
                    b_f.input_cost_basis[view.input] =
                        (held * b_f.input_cost_basis[view.input] + cost) / total;
                }
            } else {
                ledger::transfer(w, view.account, ledger::firm(firm_idx), cost,
                                 view.reason, journal);
            }

            market::record_trade(w, f.good, bought, cost);
            f.inventory -= bought;
            remaining -= bought;

            // A household consumes what it buys; a firm stockpiles it against
            // the month's production.
            if (view.stock != nullptr) *view.stock += bought;
        }

        // Whatever this seller could not supply is its share of the shortfall.
        // Only stock-outs count as rationing — running out of money is the
        // buyer's own constraint, not the seller's failure.
        double stock_shortfall = wanted - std::min(wanted, f.inventory + bought);
        if (stock_shortfall > 0.0) {
            view.unsatisfied[slot] += stock_shortfall;
        }

        if (*view.liquidity <= 0.0) break;
    }
    // Any demand still unmet at the end of the day simply vanishes.
}

void shop(World& w, uint32_t household, Rng& rng, ledger::Journal* journal) {
    buy(w, Buyer{BuyerKind::Household, household, 0}, rng, journal);
}

void produce(World& w) {
    for (Firm& f : w.firms) {
        const Good& g = w.goods[f.good];

        double by_labour = w.params.productivity * static_cast<double>(f.employees.size());

        // Leontief: no substitution between inputs. Short of iron, extra
        // fibre does not help. That is exactly what makes a stockout travel
        // up the chain instead of being absorbed at the first firm that hits
        // it.
        double limit = by_labour;
        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            if (g.input_qty[s] <= 0.0) continue;
            limit = std::min(limit, f.input_stock[s] / g.input_qty[s]);
        }
        if (limit <= 0.0) continue;

        for (uint32_t s = 0; s < params::kMaxInputs; ++s) {
            if (g.inputs[s] == NO_GOOD) continue;
            f.input_stock[s] -= limit * g.input_qty[s];
            if (f.input_stock[s] < 0.0) f.input_stock[s] = 0.0;
        }
        f.inventory += limit;
    }
}

void run_day(World& w, Rng& rng, ledger::Journal* journal) {
    std::vector<uint32_t> order(w.households.size());
    std::iota(order.begin(), order.end(), 0u);
    rng.shuffle(order);

    for (uint32_t h : order) shop(w, h, rng, journal);

    // Firms restock their inputs after households have shopped and before
    // production consumes them. The guard matters: in a single-good world
    // there is nothing upstream to buy, and the shuffle alone would shift
    // every existing run's draw sequence (invariant 2).
    if (w.goods.size() > 1) {
        std::vector<uint32_t> forder(w.firms.size());
        std::iota(forder.begin(), forder.end(), 0u);
        rng.shuffle(forder);

        for (uint32_t fi : forder) {
            const Good& recipe = w.goods[w.firms[fi].good];
            for (uint32_t i = 0; i < params::kMaxInputs; ++i) {
                if (recipe.inputs[i] == NO_GOOD) continue;
                buy(w, Buyer{BuyerKind::Firm, fi, recipe.inputs[i]}, rng, journal);
            }
        }
    }

    produce(w);
}

}  // namespace daily
