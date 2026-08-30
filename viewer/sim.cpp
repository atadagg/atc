#include "sim.hpp"

#include <algorithm>

#include "params.hpp"
#include "scheduler.hpp"

Sim::Sim(uint64_t seed, const intervention::Log* log)
    : seed_(seed), interventions_(log), rng_(seed),
      world_(params::kNumHouseholds, params::kNumFirms, rng_) {
    snapshots_.push_back(SimSnapshot{world_, rng_, 0});
}

void Sim::step() {
    Scheduler s(world_, rng_);
    s.set_month(month_);
    s.set_interventions(interventions_);
    s.run_month();
    ++month_;
    maybe_retain_snapshot();
}

void Sim::step(int months) {
    for (int i = 0; i < months; ++i) step();
}

void Sim::maybe_retain_snapshot() {
    if (month_ % kSnapshotInterval != 0) return;
    if (!snapshots_.empty() && snapshots_.back().month >= month_) return;
    snapshots_.push_back(SimSnapshot{world_, rng_, month_});
}

SimSnapshot Sim::snapshot() const { return SimSnapshot{world_, rng_, month_}; }

void Sim::restore(const SimSnapshot& s) {
    world_ = s.world;
    rng_ = s.rng;
    month_ = s.month;
}

void Sim::seek(int target_month) {
    const int target = std::max(0, target_month);
    if (target < month_) {
        // Newest retained snapshot at or before the target. snapshots_ is
        // ascending and always holds month 0, so this always finds one.
        auto it = std::upper_bound(
            snapshots_.begin(), snapshots_.end(), target,
            [](int m, const SimSnapshot& s) { return m < s.month; });
        restore(*(it - 1));
    }
    while (month_ < target) step();
}
