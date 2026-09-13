#include "scheduler.hpp"

#include "daily.hpp"
#include "intervention.hpp"
#include "firm_decisions.hpp"
#include "household_decisions.hpp"
#include "market.hpp"
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
    // Month open: everything the operator did takes effect before any agent
    // acts, in seq order. Applying them mid-month would make the result depend
    // on where in the tick the change landed, which is exactly the
    // nondeterminism the log exists to remove.
    if (interventions_) {
        if (journal_) journal_->set_time(months_elapsed_, ledger::kMonthClose);
        for (const auto& i : interventions_->due(months_elapsed_)) {
            intervention::apply(world_, i, journal_);
        }
    }

    // Publish last month's reference prices before any agent decides anything
    // this month -- firms and households that read it (later work) should see
    // the same number all month, not one that moves as trades happen.
    market::publish(world_);

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
