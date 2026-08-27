#include <algorithm>
#include <cmath>
#include <vector>

#include "doctest.h"
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
    CHECK(std::abs(a - b) < 0.15);
}
