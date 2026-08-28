#pragma once

#include <cstdint>

#include "agents.hpp"
#include "ring_recorder.hpp"
#include "sim.hpp"

// All viewer state. Holds no graphics types, so it can be reasoned about
// (and later tested) without a window.
struct App {
    explicit App(uint64_t seed) : sim(seed) {}

    Sim sim;
    RingRecorder metrics{4096};

    bool paused = true;
    int months_per_frame = 1;

    uint32_t selected_firm = NO_FIRM;
    uint32_t selected_household = NO_HOUSEHOLD;

    // Steps the simulation if running, recording each month's metrics.
    void advance();
};

void draw_agents_panel(App& app);
void draw_dist_panel(App& app);
void draw_time_panel(App& app);
