#pragma once

#include "ledger.hpp"
#include "rng.hpp"

class World;

// Drives the day/month tick. Holds no state beyond the month counter;
// all simulation state lives in World.
class Scheduler {
public:
    // `journal` is optional and defaults to off: see ledger.hpp on why
    // recording every movement is opt-in. A non-null journal changes nothing
    // about the simulation -- it only observes.
    Scheduler(World& world, Rng& rng, ledger::Journal* journal = nullptr)
        : world_(world), rng_(rng), journal_(journal) {}

    // Journaling can be turned on and off between months -- the reference
    // run journals a window, never the whole thing.
    void set_journal(ledger::Journal* journal) { journal_ = journal; }

    void run_month();
    int months_elapsed() const { return months_elapsed_; }

private:
    void settle_vacancy_bookkeeping();

    World& world_;
    Rng& rng_;
    ledger::Journal* journal_ = nullptr;
    int months_elapsed_ = 0;
};
