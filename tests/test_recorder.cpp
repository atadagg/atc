#include "doctest.h"
#include "recorder.hpp"
#include "network.hpp"
#include "world.hpp"
#include "params.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

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
        m.mean_wage = 20.0; m.daily_capacity = 30.0; m.vacancies = 2;
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

TEST_CASE("field round-trip serialization with distinct values") {
    const std::string path = "test_roundtrip.csv";
    std::remove(path.c_str());
    {
        CsvRecorder rec(path);
        Metrics m{};
        m.month = 42;
        m.unemployment_rate = 0.123456789;
        m.mean_price = 1.987654321;
        m.mean_wage = 25.5;
        m.daily_capacity = 123.456;
        m.vacancies = 7;
        m.gini = 0.333333333;
        m.total_money = 5000.111;
        rec.record(m);
    }

    std::ifstream in(path);
    REQUIRE(in.good());
    std::string header;
    std::getline(in, header);

    std::string line;
    REQUIRE(std::getline(in, line));

    // Parse CSV: month,unemployment_rate,mean_price,mean_wage,daily_capacity,vacancies,gini,total_money
    std::stringstream ss(line);
    int month;
    double unemployment_rate, mean_price, mean_wage, daily_capacity, gini, total_money;
    int vacancies;
    char comma;

    ss >> month >> comma >> unemployment_rate >> comma >> mean_price >> comma
       >> mean_wage >> comma >> daily_capacity >> comma >> vacancies >> comma
       >> gini >> comma >> total_money;

    CHECK(month == 42);
    CHECK(unemployment_rate == doctest::Approx(0.123456789).epsilon(1e-10));
    CHECK(mean_price == doctest::Approx(1.987654321).epsilon(1e-10));
    CHECK(mean_wage == doctest::Approx(25.5).epsilon(1e-10));
    CHECK(daily_capacity == doctest::Approx(123.456).epsilon(1e-10));
    CHECK(vacancies == 7);
    CHECK(gini == doctest::Approx(0.333333333).epsilon(1e-10));
    CHECK(total_money == doctest::Approx(5000.111).epsilon(1e-10));

    std::remove(path.c_str());
}

TEST_CASE("precision retention for high-precision values") {
    const std::string path = "test_precision.csv";
    std::remove(path.c_str());
    {
        CsvRecorder rec(path);
        Metrics m{};
        m.month = 1;
        m.unemployment_rate = 0.1;
        m.mean_price = 1.0;
        m.mean_wage = 30.0;
        m.daily_capacity = 100.0;
        m.vacancies = 0;
        m.gini = 0.5;
        m.total_money = 123456.789012345;  // Value where truncation error is unmistakable
        rec.record(m);
    }

    std::ifstream in(path);
    REQUIRE(in.good());
    std::string header;
    std::getline(in, header);

    std::string line;
    REQUIRE(std::getline(in, line));

    // Parse the total_money field (last field)
    std::stringstream ss(line);
    int month;
    double unemployment_rate, mean_price, mean_wage, daily_capacity, gini, total_money;
    int vacancies;
    char comma;

    ss >> month >> comma >> unemployment_rate >> comma >> mean_price >> comma
       >> mean_wage >> comma >> daily_capacity >> comma >> vacancies >> comma
       >> gini >> comma >> total_money;

    // Assert on absolute difference: at 6 significant digits, 123456.789012345 becomes 123457 (error ~0.211)
    // With setprecision(15), this passes comfortably. Without it, fails by five orders of magnitude.
    const double expected = 123456.789012345;
    CHECK(std::abs(total_money - expected) < 1e-6);

    std::remove(path.c_str());
}

TEST_CASE("csv recorder throws when output directory does not exist") {
    CHECK_THROWS_AS([]{ CsvRecorder("no_such_dir_xyz/out.csv"); }(), std::runtime_error);
}
