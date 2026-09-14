#pragma once

#include <cstdint>

#include "agents.hpp"
#include "rng.hpp"

class World;

namespace network {

// Employ `household` at `firm`, detaching it from any previous employer.
void hire(World& w, uint32_t household, uint32_t firm);

// Unemploy `household`. Safe to call on an already-unemployed household.
void fire(World& w, uint32_t household);

// Point shop slot `slot` at `new_firm`, clearing that slot's unmet demand.
void replace_shop(World& w, uint32_t household, uint32_t slot, uint32_t new_firm);

bool has_shop(const Household& h, uint32_t firm);

// The good a household shops for. This ASSUMES exactly one consumer good in
// the world, which is what the supply chain currently builds; the first one
// found wins, so a world with two consumer goods would route every household
// to whichever has the lower index. Households choosing between consumer
// goods is a later piece of work. NO_GOOD means "no consumer good declared",
// in which case a caller that filters on it should pass everything -- the
// pre-supply-chain behaviour.
uint32_t consumer_good(const World& w);

// Draw a firm the household does not already shop at, with probability
// proportional to firm size in employees. Firms with zero employees are
// still reachable (weight 1) so they are not permanently excluded.
// Returns NO_FIRM if every firm is already connected.
uint32_t random_unconnected_firm_by_size(const World& w, uint32_t household, Rng& rng);

// Draw a firm the household does not already shop at, with equal probability
// regardless of size. Used by the rationed-household rewiring path, which
// applies no price test -- see the comment on search_reliable_shop.
// Returns NO_FIRM if every firm is already connected.
uint32_t random_unconnected_firm(const World& w, uint32_t household, Rng& rng);

// Draw a firm that produces `good` and that `firm` does not already buy from,
// with probability proportional to size in employees -- the same visibility
// weighting the household price-comparison path uses, and for the same
// reason. Zero-employee producers keep weight 1 so they are not permanently
// excluded. Returns NO_FIRM if every producer is already connected.
//
// Deliberately NOT shared with random_unconnected_firm_by_size: that one
// searches for a household's shop, this one for a firm's supplier of one
// input. See CLAUDE.md on why the rewiring draws are kept apart.
uint32_t random_unconnected_producer(const World& w, uint32_t firm, uint32_t good, Rng& rng);

// Draw a producer of `good` that `firm` does not already buy from, with EQUAL
// probability regardless of size. This is the firm-side twin of
// random_unconnected_firm, and it is unweighted for exactly the same reason:
// the paths that use it -- filling an empty supplier slot, and replacing a
// supplier that rationed the firm -- apply no price test, so a size weighting
// there would be unopposed. Unopposed, it made a newly founded upstream firm
// a ~0.25% draw against five incumbents at ~80 employees, which left it
// permanently idle and unkillable. The weighting belongs only on the
// stickiness path, where an explicit price comparison decides the switch.
// Returns NO_FIRM if every living producer is already connected.
uint32_t random_unconnected_producer_unweighted(const World& w, uint32_t firm, uint32_t good,
                                                Rng& rng);

}  // namespace network
