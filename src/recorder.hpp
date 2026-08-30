#pragma once

#include <fstream>
#include <string>

#include "ledger.hpp"

class World;

struct Metrics {
    int month = 0;
    double unemployment_rate = 0.0;
    double mean_price = 0.0;
    double mean_wage = 0.0;
    // Instantaneous daily output rate (lambda * employed), NOT a monthly
    // total. See compute_metrics() in recorder.cpp for the exact measurement
    // point relative to apply_pending_fires.
    double daily_capacity = 0.0;
    int vacancies = 0;
    double gini = 0.0;
    // Money in circulation: what non-issuer accounts hold. Identical to the
    // books total while the model has no issuers, which Lengnick v1 does not.
    // The CSV column keeps the name `total_money` -- scripts/ read it.
    double total_money = 0.0;

    // Demand side, all in goods units for the month just closed. `demand` is
    // demand *wanted* (what firms were asked for, including what they could
    // not serve), not units sold -- see the demand-recording note in CLAUDE.md.
    double demand = 0.0;
    double planned_consumption = 0.0;
    double inventory = 0.0;
    double household_liquidity = 0.0;
};

Metrics compute_metrics(const World& w, int month);

// The simulation core's only output path. The core never does I/O directly.
class Recorder {
public:
    virtual ~Recorder() = default;
    virtual void record(const Metrics& m) = 0;
};

class CsvRecorder : public Recorder {
public:
    explicit CsvRecorder(const std::string& path);
    void record(const Metrics& m) override;

private:
    std::ofstream out_;
};

// The journal's file sink: one row per movement of money. This is the
// I/O half of the ledger, so it lives here rather than in `ledger` --
// the core does I/O only through this file (invariant 5).
//
// A row per movement is a lot of rows: roughly a million a month at the
// reference population. Journal a window, not a 6000-month run.
class CsvJournal : public ledger::Journal {
public:
    explicit CsvJournal(const std::string& path);

protected:
    void append(const ledger::Entry& e) override;

private:
    std::ofstream out_;
};
