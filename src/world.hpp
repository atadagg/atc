#pragma once

#include <vector>

#include "agents.hpp"
#include "goods.hpp"
#include "params.hpp"
#include "rng.hpp"

// Owns all agent state. The single source of truth for the simulation.
class World {
public:
    // Throws std::invalid_argument if n_households == 0 or n_firms == 0.
    // Shop distinctness is preserved when n_firms >= kShopsPerHousehold (7);
    // with fewer firms, shops may repeat (test-only configuration).
    World(uint32_t n_households, uint32_t n_firms, Rng& rng);

    std::vector<Firm> firms;
    std::vector<Household> households;

    // The production graph. A fresh World holds exactly one consumer good
    // made from labour alone -- which is the economy as it was before goods
    // existed, expressed in the new vocabulary rather than special-cased.
    std::vector<Good> goods;

    // Accounts that are allowed to go negative. An issuer's negative balance
    // is not a deficit: it is the money it has issued.
    //
    // A World is constructed with exactly one, balance zero, standing for what
    // is outside the modelled economy. Lengnick v1 never touches it, so the
    // money supply is constant -- a property of that model, not of the engine.
    // A bank extension gives an issuer behaviour and an agent; the invariant
    // needs only the balance.
    std::vector<double> issuers;

    // Behavioural parameters for this run. Runtime state rather than
    // `constexpr` so they can change mid-run; carried by `SimSnapshot`
    // because it copies the whole World, which is what keeps a backward
    // scrub correct across a parameter change. See params.hpp.
    Params params;

    // The money supply: what non-issuer accounts hold. A *measurement*, not an
    // invariant -- it rises when money is issued and falls when it is repaid.
    // Constant only while there are no issuers.
    double money_in_circulation() const;

    // The sum over every account, issuers included. An *assertion*: it never
    // moves, because every movement nets to zero (invariant 1). If this drifts,
    // the plumbing is broken.
    double books_balance() const;
};
