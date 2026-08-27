#include "doctest.h"
#include "recorder.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

TEST_CASE("unemployment rate counts unemployed households") {
    Rng rng(1);
    World w(10, 2, rng);
    for (uint32_t h = 0; h < 4; ++h) network::fire(w, h);
    Metrics m = compute_metrics(w, 3);
    CHECK(m.month == 3);
    CHECK(m.unemployment_rate == doctest::Approx(0.4));
}

TEST_CASE("mean price and wage average across firms") {
    Rng rng(1);
    World w(10, 2, rng);
    w.firms[0].price = 2.0;  w.firms[1].price = 4.0;
    w.firms[0].wage  = 10.0; w.firms[1].wage  = 30.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.mean_price == doctest::Approx(3.0));
    CHECK(m.mean_wage == doctest::Approx(20.0));
}

TEST_CASE("gini is zero for a perfectly equal distribution") {
    Rng rng(1);
    World w(100, 2, rng);
    for (auto& h : w.households) h.liquidity = 50.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.gini == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("gini approaches one when one household holds everything") {
    Rng rng(1);
    World w(100, 2, rng);
    for (auto& h : w.households) h.liquidity = 0.0;
    w.households[0].liquidity = 1000.0;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.gini > 0.95);
}

TEST_CASE("vacancies are counted") {
    Rng rng(1);
    World w(10, 4, rng);
    w.firms[0].has_open_vacancy = true;
    w.firms[2].has_open_vacancy = true;
    Metrics m = compute_metrics(w, 0);
    CHECK(m.vacancies == 2);
}

TEST_CASE("csv recorder writes a header and one row per record") {
    const std::string path = "test_output.csv";
    std::remove(path.c_str());
    {
        CsvRecorder rec(path);
        Metrics m{};
        m.month = 1; m.unemployment_rate = 0.5; m.mean_price = 2.0;
        m.mean_wage = 20.0; m.total_production = 30.0; m.vacancies = 2;
        m.gini = 0.25; m.total_money = 1000.0;
        rec.record(m);
        m.month = 2;
        rec.record(m);
    }

    std::ifstream in(path);
    REQUIRE(in.good());
    std::string line;
    std::getline(in, line);
    CHECK(line.find("month") != std::string::npos);
    CHECK(line.find("unemployment_rate") != std::string::npos);
    CHECK(line.find("gini") != std::string::npos);

    int rows = 0;
    while (std::getline(in, line)) if (!line.empty()) ++rows;
    CHECK(rows == 2);

    std::remove(path.c_str());
}
