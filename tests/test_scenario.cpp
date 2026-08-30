#include "doctest.h"

#include "intervention.hpp"
#include "scenario.hpp"
#include "scheduler.hpp"
#include "world.hpp"

#include <sstream>
#include <stdexcept>

// A save file is (seed + intervention log) -- kilobytes, not a state dump,
// because the run is a fold over its inputs. Parsing lives on streams, not
// files: the core does not open files (invariant 5), so main and the viewer
// hand it a stream.

static Scenario round_trip(const Scenario& in) {
    std::ostringstream out;
    scenario::write(in, out);
    std::istringstream back(out.str());
    return scenario::read(back);
}

TEST_CASE("a scenario survives a write/read round trip") {
    Scenario s;
    s.seed = 42;
    s.log.set_param(60, "min_wage", 12.5);
    s.log.set_param(60, "productivity", 3.5);
    s.log.issue_to_all_households(100, 0, 250.0);
    s.log.issue(120, 0, 7, 999.0);

    Scenario back = round_trip(s);

    CHECK(back.seed == 42);
    auto a = s.log.all();
    auto b = back.log.all();
    REQUIRE(b.size() == a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(b[i].month == a[i].month);
        CHECK(b[i].seq == a[i].seq);
        CHECK(b[i].action == a[i].action);
        CHECK(b[i].param == a[i].param);
        CHECK(b[i].issuer == a[i].issuer);
        CHECK(b[i].target == a[i].target);
        CHECK(b[i].value == doctest::Approx(a[i].value));
    }
}

TEST_CASE("an empty scenario round trips") {
    Scenario s;
    s.seed = 7;
    Scenario back = round_trip(s);
    CHECK(back.seed == 7);
    CHECK(back.log.empty());
}

TEST_CASE("blank lines and comments are ignored") {
    std::istringstream in(
        "economy-scenario 1\n"
        "\n"
        "# a comment\n"
        "seed 3\n"
        "   \n"
        "60 set_param min_wage 12.5\n");
    Scenario s = scenario::read(in);
    CHECK(s.seed == 3);
    REQUIRE(s.log.all().size() == 1);
    CHECK(s.log.all()[0].value == doctest::Approx(12.5));
}

TEST_CASE("a scenario replays to the same world as the in-memory log") {
    intervention::Log direct;
    direct.set_param(2, "productivity", 6.0);
    direct.issue_to_all_households(3, 0, 10.0);

    Scenario s;
    s.seed = 5;
    s.log.set_param(2, "productivity", 6.0);
    s.log.issue_to_all_households(3, 0, 10.0);
    Scenario replayed = round_trip(s);

    auto run = [](const intervention::Log& log) {
        Rng rng(5);
        World w(50, 4, rng);
        Scheduler sch(w, rng);
        sch.set_interventions(&log);
        for (int m = 0; m < 6; ++m) sch.run_month();
        return w;
    };

    World a = run(direct);
    World b = run(replayed.log);
    CHECK(a.params.productivity == b.params.productivity);
    CHECK(a.issuers[0] == b.issuers[0]);
    for (size_t i = 0; i < a.households.size(); ++i) {
        CHECK(a.households[i].liquidity == b.households[i].liquidity);
    }
}

TEST_CASE("a malformed scenario is rejected rather than half-read") {
    auto rejects = [](const char* text) {
        std::istringstream in(text);
        CHECK_THROWS_AS(scenario::read(in), std::runtime_error);
    };
    rejects("not a scenario file\n");
    rejects("economy-scenario 1\nseed 1\n60 set_param no_such_knob 1\n");
    rejects("economy-scenario 1\nseed 1\n60 explode 1 2\n");
    rejects("economy-scenario 1\nseed 1\n60 set_param min_wage\n");
    rejects("economy-scenario 1\nseed 1\nsixty set_param min_wage 1\n");
    rejects("economy-scenario 1\nseed notanumber\n");
    rejects("economy-scenario 99\nseed 1\n");  // unknown format version
}

TEST_CASE("a scenario built out of month order round trips by meaning") {
    // `seq` is renumbered on read, because the file is written in (month, seq)
    // order and re-read in that order. Only *intra-month* order is meaningful,
    // and that is what must survive -- not the absolute seq values.
    Scenario s;
    s.seed = 1;
    s.log.set_param(9, "min_wage", 3.0);   // later month, added first
    s.log.set_param(2, "productivity", 4.0);
    s.log.set_param(9, "min_wage", 5.0);   // same month as the first

    Scenario back = round_trip(s);
    auto e = back.log.all();
    REQUIRE(e.size() == 3);

    CHECK(e[0].month == 2);
    CHECK(e[1].month == 9);
    CHECK(e[2].month == 9);
    // The two month-9 entries keep their relative order, so the later value wins.
    CHECK(e[1].value == doctest::Approx(3.0));
    CHECK(e[2].value == doctest::Approx(5.0));
    CHECK(e[1].seq < e[2].seq);
}
