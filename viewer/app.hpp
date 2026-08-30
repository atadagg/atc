#pragma once

#include <cstdint>
#include <vector>

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

    // The furthest month the simulation has ever reached. Distinct from
    // sim.month(), which is whatever month is currently displayed and can
    // move backward on a seek. The scrub bar's upper bound is this, not
    // sim.month() -- otherwise one backward seek would shrink the slider's
    // own maximum and the run could never be scrubbed forward again.
    int max_month = 0;

    uint32_t selected_firm = NO_FIRM;
    uint32_t selected_household = NO_HOUSEHOLD;

    struct Filters {
        bool wage_at_floor = false;
        bool vacancy_open = false;
        bool inventory_zero = false;
        bool unemployed = false;
        // Non-empty means: show only these firms. Written by a histogram
        // bin click, cleared by the "clear" button.
        std::vector<uint32_t> pinned_firms;
    } filters;

    // Steps the simulation if running, recording each month's metrics.
    void advance();

    // Moves the displayed state to `month`. The metrics ring is cleared
    // because its contents describe months that are no longer the ones being
    // shown; it refills as the run continues. Does not touch max_month --
    // seeking never extends how far the run has actually reached.
    void seek_to(int month);
};

void draw_agents_panel(App& app);
void draw_dist_panel(App& app);
void draw_time_panel(App& app);
