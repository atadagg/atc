#include "recorder.hpp"

#include <algorithm>
#include <iomanip>
#include <stdexcept>
#include <vector>

#include "params.hpp"
#include "world.hpp"

Metrics compute_metrics(const World& w, int month) {
    Metrics m;
    m.month = month;

    uint32_t unemployed = 0;
    uint32_t employed = 0;
    for (const auto& h : w.households) {
        if (h.employed()) ++employed; else ++unemployed;
    }
    m.unemployment_rate =
        static_cast<double>(unemployed) / static_cast<double>(w.households.size());
    // Rate, not a monthly total: kProductivity is a per-day-per-worker
    // yield, so this is employed workers' capacity for a single day, not
    // the month's output. It is also measured after month_close::run --
    // specifically after apply_pending_fires -- so `employed` here already
    // reflects next month's headcount, not the headcount that produced
    // whatever inventory exists at this recording point.
    m.daily_capacity = params::kProductivity * static_cast<double>(employed);

    double price_sum = 0.0;
    double wage_sum = 0.0;
    for (const auto& f : w.firms) {
        price_sum += f.price;
        wage_sum += f.wage;
        if (f.has_open_vacancy) ++m.vacancies;
    }
    m.mean_price = price_sum / static_cast<double>(w.firms.size());
    m.mean_wage = wage_sum / static_cast<double>(w.firms.size());

    // Gini of household liquidity, on the sorted distribution.
    std::vector<double> x;
    x.reserve(w.households.size());
    for (const auto& h : w.households) x.push_back(std::max(0.0, h.liquidity));
    std::sort(x.begin(), x.end());

    double sum = 0.0;
    double weighted = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        sum += x[i];
        weighted += static_cast<double>(i + 1) * x[i];
    }
    const double n = static_cast<double>(x.size());
    m.gini = (sum > 0.0) ? (2.0 * weighted) / (n * sum) - (n + 1.0) / n : 0.0;

    m.total_money = w.total_money();
    return m;
}

CsvRecorder::CsvRecorder(const std::string& path) : out_(path) {
    if (!out_.is_open()) {
        throw std::runtime_error("CsvRecorder: cannot open output file: " + path);
    }
    out_ << std::setprecision(15);
    out_ << "month,unemployment_rate,mean_price,mean_wage,"
            "daily_capacity,vacancies,gini,total_money\n";
}

void CsvRecorder::record(const Metrics& m) {
    out_ << m.month << ',' << m.unemployment_rate << ',' << m.mean_price << ','
         << m.mean_wage << ',' << m.daily_capacity << ',' << m.vacancies << ','
         << m.gini << ',' << m.total_money << '\n';
}
