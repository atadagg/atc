#pragma once

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

// The values behind distributions()[which], one per agent, in agent-index
// order -- so a Histogram's members index straight back into the world.
// Out-of-range `which` falls through to the last distribution rather than
// reading out of bounds, matching firm_cell/household_cell in
// model_agents.hpp.
std::vector<double> dist_values(const World& w, int which);
