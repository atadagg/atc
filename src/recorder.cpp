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
    // Rate, not a monthly total: productivity is a per-day-per-worker
    // yield, so this is employed workers' capacity for a single day, not
    // the month's output. It is also measured after month_close::run --
    // specifically after apply_pending_fires -- so `employed` here already
    // reflects next month's headcount, not the headcount that produced
    // whatever inventory exists at this recording point.
    m.daily_capacity = w.params.productivity * static_cast<double>(employed);

    double price_sum = 0.0;
    double wage_sum = 0.0;
    for (const auto& f : w.firms) {
        price_sum += f.price;
        wage_sum += f.wage;
        if (f.has_open_vacancy) ++m.vacancies;
        // month_close::roll_demand ran last, so demand_last_month is the
        // demand faced during the month just closed.
        m.demand += f.demand_last_month;
        m.inventory += f.inventory;
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

    for (const auto& h : w.households) {
        m.planned_consumption += h.planned_consumption;
        m.household_liquidity += h.liquidity;
    }

    m.total_money = w.money_in_circulation();
    return m;
}

void record_flows(Metrics& m, const ledger::MonthFlows& f, double discrepancy) {
    m.flow_purchase = f.by_reason[ledger::reason_index(ledger::Reason::Purchase)];
    m.flow_wage = f.by_reason[ledger::reason_index(ledger::Reason::Wage)];
    m.flow_profit = f.by_reason[ledger::reason_index(ledger::Reason::Profit)];
    m.flow_issuance = f.by_reason[ledger::reason_index(ledger::Reason::Issuance)];
    m.flow_discrepancy = discrepancy;
}

CsvRecorder::CsvRecorder(const std::string& path) : out_(path) {
    if (!out_.is_open()) {
        throw std::runtime_error("CsvRecorder: cannot open output file: " + path);
    }
    out_ << std::setprecision(15);
    out_ << "month,unemployment_rate,mean_price,mean_wage,"
            "daily_capacity,vacancies,gini,total_money,"
            "demand,planned_consumption,inventory,household_liquidity,"
            "flow_purchase,flow_wage,flow_profit,flow_issuance,flow_discrepancy\n";
}

void CsvRecorder::record(const Metrics& m) {
    out_ << m.month << ',' << m.unemployment_rate << ',' << m.mean_price << ','
         << m.mean_wage << ',' << m.daily_capacity << ',' << m.vacancies << ','
         << m.gini << ',' << m.total_money << ',' << m.demand << ','
         << m.planned_consumption << ',' << m.inventory << ','
         << m.household_liquidity << ',' << m.flow_purchase << ','
         << m.flow_wage << ',' << m.flow_profit << ',' << m.flow_issuance << ','
         << m.flow_discrepancy << '\n';
}

namespace {

const char* kind_name(ledger::Kind k) {
    switch (k) {
        case ledger::Kind::Firm:      return "firm";
        case ledger::Kind::Household: return "household";
        case ledger::Kind::Issuer:    break;
    }
    return "issuer";
}

const char* reason_name(ledger::Reason r) {
    switch (r) {
        case ledger::Reason::Purchase: return "purchase";
        case ledger::Reason::Wage:     return "wage";
        case ledger::Reason::Profit:   return "profit";
        case ledger::Reason::Issuance: return "issuance";
        case ledger::Reason::TradeCreditSettlement: return "trade_credit_settlement";
        case ledger::Reason::FirmLiquidation: return "firm_liquidation";
        case ledger::Reason::Unspecified: break;
        case ledger::Reason::Count: break;  // sentinel, never a real entry
    }
    return "unspecified";
}

}  // namespace

CsvJournal::CsvJournal(const std::string& path) : ledger::Journal(true), out_(path) {
    if (!out_.is_open()) {
        throw std::runtime_error("CsvJournal: cannot open output file: " + path);
    }
    out_ << std::setprecision(15);
    out_ << "month,day,seq,from_kind,from_index,to_kind,to_index,amount,reason\n";
}

void CsvJournal::append(const ledger::Entry& e) {
    out_ << e.month << ',' << e.day << ',' << e.seq << ','
         << kind_name(e.from.kind) << ',' << e.from.index << ','
         << kind_name(e.to.kind) << ',' << e.to.index << ','
         << e.amount << ',' << reason_name(e.reason) << '\n';
}
