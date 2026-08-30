#include <vector>

#include "doctest.h"
#include "recorder.hpp"
#include "sim.hpp"
#include "world.hpp"

namespace {

// The metrics a fresh sim produces for months (from, to].
std::vector<Metrics> run_to(Sim& s, int to) {
    std::vector<Metrics> out;
    while (s.month() < to) {
        s.step();
        out.push_back(compute_metrics(s.world(), s.month()));
    }
    return out;
}

void require_identical(const std::vector<Metrics>& a, const std::vector<Metrics>& b) {
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].month == b[i].month);
        // Byte-exact, not approximate: determinism is the whole claim.
        CHECK(a[i].unemployment_rate == b[i].unemployment_rate);
        CHECK(a[i].mean_wage == b[i].mean_wage);
        CHECK(a[i].mean_price == b[i].mean_price);
        CHECK(a[i].gini == b[i].gini);
        CHECK(a[i].total_money == b[i].total_money);
    }
}

}  // namespace

TEST_CASE("stepping advances the month counter") {
    Sim s(1);
    CHECK(s.month() == 0);
    s.step();
    CHECK(s.month() == 1);
    s.step(5);
    CHECK(s.month() == 6);
}

TEST_CASE("restoring a snapshot reproduces the future exactly") {
    Sim s(1);
    s.step(200);
    const SimSnapshot snap = s.snapshot();
    CHECK(snap.month == 200);

    const std::vector<Metrics> first = run_to(s, 300);

    s.restore(snap);
    CHECK(s.month() == 200);
    const std::vector<Metrics> second = run_to(s, 300);

    require_identical(first, second);
}

TEST_CASE("seeking backward lands on the same state as running forward once") {
    Sim reference(1);
    reference.step(275);
    const Metrics expected = compute_metrics(reference.world(), 275);

    Sim s(1);
    s.step(400);
    s.seek(275);
    CHECK(s.month() == 275);
    const Metrics got = compute_metrics(s.world(), 275);

    CHECK(got.unemployment_rate == expected.unemployment_rate);
    CHECK(got.mean_wage == expected.mean_wage);
    CHECK(got.gini == expected.gini);
}

TEST_CASE("seeking forward is the same as stepping forward") {
    Sim s(1);
    s.seek(50);
    CHECK(s.month() == 50);
    const Metrics seeked = compute_metrics(s.world(), 50);

    Sim t(1);
    t.step(50);
    CHECK(compute_metrics(t.world(), 50).mean_wage == seeked.mean_wage);
}

TEST_CASE("money is conserved across a seek") {
    Sim s(1);
    const double start = s.world().total_money();
    s.step(300);
    s.seek(100);
    CHECK(s.world().total_money() == doctest::Approx(start).epsilon(1e-12));
}

TEST_CASE("parameters travel with a snapshot") {
    Sim sim(1);
    sim.step(3);
    CHECK(sim.world().params.productivity == doctest::Approx(3.0));

    SimSnapshot s = sim.snapshot();
    CHECK(s.world.params.productivity == doctest::Approx(3.0));

    s.world.params.productivity = 11.0;
    sim.restore(s);
    CHECK(sim.world().params.productivity == doctest::Approx(11.0));
}
