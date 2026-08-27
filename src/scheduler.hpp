#pragma once

#include "rng.hpp"

class World;

// Drives the day/month tick. Holds no state beyond the month counter;
// all simulation state lives in World.
class Scheduler {
public:
    Scheduler(World& world, Rng& rng) : world_(world), rng_(rng) {}

    void run_month();
    int months_elapsed() const { return months_elapsed_; }

private:
    void settle_vacancy_bookkeeping();

    World& world_;
    Rng& rng_;
    int months_elapsed_ = 0;
};
