#pragma once

#include "intervention.hpp"
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

    // The log is an *input* to the run, so it is const and outlives the
    // scheduler. Everything due is applied at month open, before any agent
    // acts, which is what makes a re-simulation reproduce the same history.
    void set_interventions(const intervention::Log* log) { interventions_ = log; }

    // Sim builds a throwaway Scheduler per step, so it must say which month
    // this is -- otherwise the log and the journal both see month 0 forever.
    void set_month(int month) { months_elapsed_ = month; }

    void run_month();
    int months_elapsed() const { return months_elapsed_; }

private:
    void settle_vacancy_bookkeeping();

    World& world_;
    Rng& rng_;
    ledger::Journal* journal_ = nullptr;
    const intervention::Log* interventions_ = nullptr;
    int months_elapsed_ = 0;
};
