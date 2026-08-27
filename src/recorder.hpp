#pragma once

#include <fstream>
#include <string>

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
    double total_money = 0.0;
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
