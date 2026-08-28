#include "app.hpp"

#include "recorder.hpp"

void App::advance() {
    if (paused) return;
    for (int i = 0; i < months_per_frame; ++i) {
        sim.step();
        metrics.record(compute_metrics(sim.world(), sim.month()));
    }
}
