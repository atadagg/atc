#pragma once

#include <cstdint>
#include <vector>

#include "rng.hpp"
#include "world.hpp"

// A restorable point in a run. Copyable by value: World owns plain-struct
// vectors and Rng owns its engine state, so this is a deep copy today. If a
// future extension gives either a non-copyable member, this is the line that
// stops compiling -- fix it here rather than adding an interface to the core.
struct SimSnapshot {
    World world;
    Rng rng;
    int month = 0;
};

// Owns a run and can move it to any month exactly.
//
// Scheduler holds no state beyond its own month counter (see
// src/scheduler.hpp), so Sim keeps the authoritative counter and builds a
// throwaway Scheduler per step. That keeps Sim free of members holding
// references into itself, which is what makes restore() a plain assignment.
class Sim {
public:
    explicit Sim(uint64_t seed);

    Sim(const Sim&) = delete;
    Sim& operator=(const Sim&) = delete;

    void step();
    void step(int months);

    int month() const { return month_; }
    const World& world() const { return world_; }
    uint64_t seed() const { return seed_; }

    SimSnapshot snapshot() const;
    void restore(const SimSnapshot& s);

    // Moves to target_month exactly. Forward: steps. Backward: restores the
    // newest retained snapshot at or before the target, then re-simulates
    // forward. Determinism (invariant 2) is what makes this byte-exact.
    // Negative targets are clamped to 0.
    void seek(int target_month);

    // Months between retained snapshots. A snapshot is ~100 KB, so a
    // 6000-month run retains ~12 MB and never re-simulates more than
    // kSnapshotInterval - 1 months (~70 ms).
    static constexpr int kSnapshotInterval = 50;

private:
    void maybe_retain_snapshot();

    uint64_t seed_;
    Rng rng_;
    World world_;
    int month_ = 0;
    std::vector<SimSnapshot> snapshots_;  // ascending by month, month 0 first
};
