#include "doctest.h"
#include "balance_sheet.hpp"
#include "world.hpp"

TEST_CASE("equity is assets minus liabilities") {
    Rng rng(1);
    World w(10, 3, rng);
    Firm& f = w.firms[0];
    f.liquidity = 100.0;
    f.inventory = 10.0;
    f.output_cost_basis = 2.0;   // 10 units held at cost 2 = 20
    f.receivables = 5.0;
    f.payables = 30.0;

    balance_sheet::Sheet s = balance_sheet::of(w, 0);

    CHECK(s.assets() == doctest::Approx(100.0 + 20.0 + 5.0));
    CHECK(s.liabilities() == doctest::Approx(30.0));
    CHECK(s.equity() == doctest::Approx(95.0));
    CHECK(s.insolvent() == false);
}

TEST_CASE("a firm owing more than it holds is insolvent") {
    Rng rng(1);
    World w(10, 3, rng);
    Firm& f = w.firms[0];
    f.liquidity = 1.0;
    f.inventory = 0.0;
    f.payables = 50.0;

    CHECK(balance_sheet::of(w, 0).insolvent() == true);
}

TEST_CASE("a firm with no debt can never be insolvent") {
    Rng rng(1);
    World w(10, 3, rng);
    for (uint32_t i = 0; i < w.firms.size(); ++i) {
        CHECK(balance_sheet::of(w, i).insolvent() == false);
    }
}
