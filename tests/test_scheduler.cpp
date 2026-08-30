#include <algorithm>
#include <cmath>
#include <vector>

#include "doctest.h"
#include "ledger.hpp"
#include "scheduler.hpp"
#include "world.hpp"
#include "params.hpp"

TEST_CASE("a month advances the counter") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    CHECK(s.months_elapsed() == 0);
    s.run_month();
    CHECK(s.months_elapsed() == 1);
}

TEST_CASE("money is conserved across many months") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    double before = w.total_money();
    for (int i = 0; i < 120; ++i) s.run_month();
    CHECK(w.total_money() == doctest::Approx(before).epsilon(1e-9));
}

TEST_CASE("no liquidity ever goes negative") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 120; ++i) {
        s.run_month();
        for (const auto& h : w.households) CHECK(h.liquidity >= -1e-9);
        for (const auto& f : w.firms) CHECK(f.liquidity >= -1e-9);
    }
}

TEST_CASE("inventories never go negative") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 120; ++i) {
        s.run_month();
        for (const auto& f : w.firms) CHECK(f.inventory >= -1e-9);
    }
}

TEST_CASE("employment links stay consistent over time") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 60; ++i) s.run_month();

    for (uint32_t f = 0; f < w.firms.size(); ++f) {
        for (uint32_t h : w.firms[f].employees) {
            CHECK(w.households[h].employer == f);
        }
    }
    for (uint32_t h = 0; h < w.households.size(); ++h) {
        uint32_t f = w.households[h].employer;
        if (f == NO_FIRM) continue;
        const auto& e = w.firms[f].employees;
        CHECK(std::find(e.begin(), e.end(), h) != e.end());
    }
}

TEST_CASE("no household is employed by two firms") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    Scheduler s(w, rng);
    for (int i = 0; i < 60; ++i) s.run_month();

    std::vector<int> employer_count(w.households.size(), 0);
    for (const auto& f : w.firms) {
        for (uint32_t h : f.employees) employer_count[h]++;
    }
    for (int c : employer_count) CHECK(c <= 1);
}

TEST_CASE("identical seeds produce identical runs") {
    auto run = [](uint64_t seed) {
        Rng rng(seed);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler s(w, rng);
        for (int i = 0; i < 60; ++i) s.run_month();
        std::vector<double> snapshot;
        for (const auto& f : w.firms) {
            snapshot.push_back(f.price);
            snapshot.push_back(f.wage);
            snapshot.push_back(f.inventory);
        }
        for (const auto& h : w.households) snapshot.push_back(h.liquidity);
        return snapshot;
    };
    CHECK(run(777) == run(777));
}

TEST_CASE("different seeds produce different runs") {
    auto price_of_firm_zero = [](uint64_t seed) {
        Rng rng(seed);
        World w(params::kNumHouseholds, params::kNumFirms, rng);
        Scheduler s(w, rng);
        for (int i = 0; i < 60; ++i) s.run_month();
        return w.firms[0].price;
    };
    CHECK(price_of_firm_zero(1) != price_of_firm_zero(2));
}

TEST_CASE("post-burn-in behaviour does not depend on the initial network") {
    // Two runs differing ONLY in the initial random network (different seeds
    // for construction, same seed for the dynamics) should reach statistically
    // similar unemployment after burn-in. This is a weak but real check that
    // initial conditions wash out.
    auto mean_unemployment_after_burn_in = [](uint64_t init_seed) {
        Rng init_rng(init_seed);
        World w(params::kNumHouseholds, params::kNumFirms, init_rng);
        Rng dyn_rng(4242);
        Scheduler s(w, dyn_rng);
        for (int i = 0; i < 300; ++i) s.run_month();

        double total = 0.0;
        for (int i = 0; i < 100; ++i) {
            s.run_month();
            uint32_t unemployed = 0;
            for (const auto& h : w.households) if (!h.employed()) ++unemployed;
            total += static_cast<double>(unemployed) / w.households.size();
        }
        return total / 100.0;
    };

    double a = mean_unemployment_after_burn_in(11);
    double b = mean_unemployment_after_burn_in(999);
    // Tightened from 0.15 (15pp): the observed unemployment range across a
    // 6000-month run is only ~0-2.8pp, so a 15pp tolerance could never be
    // violated by any real initial-condition dependence. 0.01 (1pp) is still
    // comfortably above the ~0.05pp gap actually observed, but is now within
    // the variable's real range, so a genuine dependence would trip it.
    CHECK(std::abs(a - b) < 0.01);
}

TEST_CASE("a journaled month records purchases, wages and profits") {
    Rng rng(1);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    ledger::VectorJournal j;
    Scheduler s(w, rng, &j);
    s.run_month();

    CHECK(!j.entries.empty());
    bool saw_purchase = false, saw_wage = false, saw_profit = false;
    for (const auto& e : j.entries) {
        CHECK(e.amount >= 0.0);
        if (e.reason == ledger::Reason::Purchase) {
            saw_purchase = true;
            CHECK(e.day >= 0);
        }
        if (e.reason == ledger::Reason::Wage) {
            saw_wage = true;
            CHECK(e.day == ledger::kMonthClose);
        }
        if (e.reason == ledger::Reason::Profit) saw_profit = true;
    }
    CHECK(saw_purchase);
    CHECK(saw_wage);
    CHECK(saw_profit);
}

TEST_CASE("the journal accounts for every change in a household's balance") {
    Rng rng(7);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    ledger::VectorJournal j;
    Scheduler s(w, rng, &j);

    const uint32_t who = 42;
    double opening = w.households[who].liquidity;
    s.run_month();

    double net = 0.0;
    for (const auto& e : j.entries) {
        if (e.from.kind == ledger::Kind::Household && e.from.index == who) net -= e.amount;
        if (e.to.kind == ledger::Kind::Household && e.to.index == who) net += e.amount;
    }
    CHECK(opening + net == doctest::Approx(w.households[who].liquidity));
}

TEST_CASE("journaling does not change the simulation") {
    Rng r1(3);
    World plain(params::kNumHouseholds, params::kNumFirms, r1);
    Scheduler s1(plain, r1);

    Rng r2(3);
    World booked(params::kNumHouseholds, params::kNumFirms, r2);
    ledger::VectorJournal j;
    Scheduler s2(booked, r2, &j);

    for (int i = 0; i < 12; ++i) { s1.run_month(); s2.run_month(); }

    for (size_t i = 0; i < plain.households.size(); ++i) {
        CHECK(plain.households[i].liquidity == booked.households[i].liquidity);
    }
    for (size_t i = 0; i < plain.firms.size(); ++i) {
        CHECK(plain.firms[i].liquidity == booked.firms[i].liquidity);
        CHECK(plain.firms[i].wage == booked.firms[i].wage);
        CHECK(plain.firms[i].price == booked.firms[i].price);
    }
}

TEST_CASE("the journal can be attached and detached mid-run") {
    Rng rng(9);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    ledger::VectorJournal j;
    Scheduler s(w, rng);

    s.run_month();
    CHECK(j.entries.empty());

    s.set_journal(&j);
    s.run_month();
    size_t after_one = j.entries.size();
    CHECK(after_one > 0);

    s.set_journal(nullptr);
    s.run_month();
    CHECK(j.entries.size() == after_one);
}
