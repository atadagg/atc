#include "scheduler.hpp"

#include "daily.hpp"
#include "firm_decisions.hpp"
#include "household_decisions.hpp"
#include "month_close.hpp"
#include "params.hpp"
#include "world.hpp"

void Scheduler::settle_vacancy_bookkeeping() {
    // Called after job search: a vacancy still open was not filled.
    for (auto& f : world_.firms) {
        f.vacancy_went_unfilled = f.has_open_vacancy;
        if (f.has_open_vacancy) {
            f.months_fully_staffed = 0;
        } else {
            ++f.months_fully_staffed;
        }
    }
}

void Scheduler::run_month() {
    firm_decisions::run_monthly(world_, rng_);
    household_decisions::run_monthly(world_, rng_);
    settle_vacancy_bookkeeping();

    for (int day = 0; day < params::kDaysPerMonth; ++day) {
        if (journal_) journal_->set_time(months_elapsed_, day);
        daily::run_day(world_, rng_, journal_);
    }

    if (journal_) journal_->set_time(months_elapsed_, ledger::kMonthClose);
    month_close::run(world_, journal_);
    ++months_elapsed_;
}
