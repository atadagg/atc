#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "cell.hpp"

class World;

// One distribution the viewer can show. `pinnable` means a bin click can
// restrict the firm table to that bin's members.
struct DistSpec {
    const char* name;
    AgentKind over;
    bool pinnable;
};

std::span<const DistSpec> distributions();

// The values behind distributions()[which], in ascending agent-index order.
//
// A firm distribution excludes dead firms entirely: a corpse's price and
// wage are stale, not zero (see the same exclusion in
// recorder.cpp::compute_metrics), and letting a graveyard of them into the
// histogram is the "mean wage of 16 was 67 dead firms at zero" trap CLAUDE.md
// warns about, just moved from an average into a bin. Household
// distributions are unaffected -- households do not die.
//
// Because dead firms are skipped, this vector is no longer dense over
// [0, agent count): position i is NOT firm/household index i once any firm
// has died. Use dist_indices() to translate a Histogram's members (positions
// into this vector) back into real agent indices. Out-of-range `which` falls
// through to the last distribution rather than reading out of bounds,
// matching firm_cell/household_cell in model_agents.hpp.
std::vector<double> dist_values(const World& w, int which);

// The real agent index behind dist_values(w, which)[i], for every i -- same
// length, same order. A Histogram built from dist_values() records member
// positions into that vector; index through this to get back a firm or
// household index a Filters/DetailModel can use.
std::vector<uint32_t> dist_indices(const World& w, int which);
