#pragma once

#include <cstdint>
#include <vector>

#include <string>

#include "agents.hpp"
#include "cell.hpp"
#include "identity_lexicon.hpp"
#include "intervention.hpp"
#include "ring_recorder.hpp"
#include "sim.hpp"

// All viewer state. Holds no graphics types, so it can be reasoned about
// (and later tested) without a window.
struct App {
    explicit App(uint64_t seed) : sim(seed, &log) {}

    // Declared before `sim` on purpose: members initialize in declaration
    // order, and Sim is constructed with a pointer to this log.
    //
    // The log is what makes steering possible without breaking determinism --
    // a run is (seed + log), so a parameter changed from the UI is replayed
    // identically when the timeline is scrubbed back through it. Append only
    // between steps: `Log::due` hands out a span into its storage.
    intervention::Log log;

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

    Filters filters;

    // Which lexicon firm names are drawn from. A parameter of the name
    // function, never a field on Firm -- when regions arrive, the region
    // supplies it. Turkish because it is the only speaker-verified locale;
    // there is deliberately no control for this yet, since changing it
    // renames every firm on screen and means nothing to the economy.
    identity::Locale locale = identity::Locale::Turkish;

    // Where the steering panel writes a scenario. A save is (seed + log), so
    // it is a few lines of text rather than a state dump.
    std::string scenario_path = "output/scenario.txt";
    std::string scenario_status;

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
void draw_steering_panel(App& app);
