#include <cmath>

#include "doctest.h"
#include "ledger.hpp"
#include "ownership.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace {

// A firm with a hand-written register, so a payout test does not depend on
// what the dealer happened to draw.
void set_register(Firm& f, std::initializer_list<std::pair<uint32_t, double>> holdings) {
    f.holders.fill(NO_HOUSEHOLD);
    f.holder_shares.fill(0.0);
    std::size_t k = 0;
    for (auto [who, how_many] : holdings) {
        f.holders[k] = who;
        f.holder_shares[k] = how_many;
        ++k;
    }
}

}  // namespace

TEST_CASE("total_shares sums only occupied slots") {
    Firm f;
    set_register(f, {{3, 60.0}, {7, 40.0}});
    CHECK(ownership::total_shares(f) == doctest::Approx(100.0));
    CHECK(ownership::shares_held(f, 3) == doctest::Approx(60.0));
    CHECK(ownership::shares_held(f, 7) == doctest::Approx(40.0));
    CHECK(ownership::shares_held(f, 5) == 0.0);
}

TEST_CASE("the deal fills a register and it always sums to kSharesPerFirm") {
    Rng rng(1);
    Params p;
    for (int trial = 0; trial < 200; ++trial) {
        Firm f;
        ownership::deal(f, 1000, p, rng);
        CHECK(ownership::total_shares(f) == doctest::Approx(ownership::kSharesPerFirm));

        uint32_t count = 0;
        for (std::size_t k = 0; k < f.holders.size(); ++k) {
            if (f.holders[k] == NO_HOUSEHOLD) continue;
            ++count;
            CHECK(f.holder_shares[k] > 0.0);
        }
        CHECK(count >= 1);
        if (f.public_company) {
            CHECK(count >= params::kMinPublicHolders);
        } else {
            CHECK(count <= params::kMaxPrivateHolders);
        }
    }
}

TEST_CASE("the deal never gives one household two slots in one firm") {
    Rng rng(4);
    Params p;
    for (int trial = 0; trial < 200; ++trial) {
        Firm f;
        ownership::deal(f, 40, p, rng);
        for (std::size_t a = 0; a < f.holders.size(); ++a) {
            if (f.holders[a] == NO_HOUSEHOLD) continue;
            for (std::size_t b = a + 1; b < f.holders.size(); ++b) {
                CHECK(f.holders[a] != f.holders[b]);
            }
        }
    }
}

TEST_CASE("a world smaller than the register still deals a valid one") {
    Rng rng(9);
    Params p;
    for (int trial = 0; trial < 50; ++trial) {
        Firm f;
        ownership::deal(f, 3, p, rng);
        uint32_t count = 0;
        for (uint32_t holder : f.holders) {
            if (holder == NO_HOUSEHOLD) continue;
            CHECK(holder < 3u);
            ++count;
        }
        CHECK(count >= 1);
        CHECK(count <= 3u);
        CHECK(ownership::total_shares(f) == doctest::Approx(ownership::kSharesPerFirm));
    }
}

TEST_CASE("share_skew 1.0 deals an equal split") {
    Rng rng(2);
    Params p;
    p.share_skew = 1.0;
    p.public_firm_fraction = 1.0;
    Firm f;
    ownership::deal(f, 1000, p, rng);
    double first = ownership::shares_held(f, f.holders[0]);
    for (uint32_t holder : f.holders) {
        if (holder == NO_HOUSEHOLD) continue;
        CHECK(ownership::shares_held(f, holder) == doctest::Approx(first));
    }
}

TEST_CASE("pay_holders splits a pot per share and lands on target") {
    Rng rng(1);
    World w(10, 2, rng);
    set_register(w.firms[0], {{1, 75.0}, {4, 25.0}});
    w.firms[0].liquidity = 250.0;
    const double before_1 = w.households[1].liquidity;
    const double before_4 = w.households[4].liquidity;
    const double books = w.books_balance();

    const double paid = ownership::pay_holders(w, 0, 200.0, ledger::Reason::Profit, nullptr);

    CHECK(paid == doctest::Approx(200.0));
    CHECK(w.firms[0].liquidity == doctest::Approx(50.0));
    CHECK(w.households[1].liquidity - before_1 == doctest::Approx(150.0));
    CHECK(w.households[4].liquidity - before_4 == doctest::Approx(50.0));
    CHECK(w.books_balance() == doctest::Approx(books));
}

TEST_CASE("pay_holders pays a non-holder nothing, however rich it is") {
    Rng rng(1);
    World w(10, 2, rng);
    set_register(w.firms[0], {{1, 100.0}});
    w.firms[0].liquidity = 100.0;
    w.households[2].liquidity = 1e6;
    const double before_2 = w.households[2].liquidity;

    ownership::pay_holders(w, 0, 100.0, ledger::Reason::Profit, nullptr);

    CHECK(w.households[2].liquidity == doctest::Approx(before_2));
    CHECK(w.firms[0].liquidity == doctest::Approx(0.0));
}

TEST_CASE("a firm with no holders keeps its pot rather than destroying it") {
    Rng rng(1);
    World w(10, 2, rng);
    set_register(w.firms[0], {});
    w.firms[0].liquidity = 100.0;
    const double books = w.books_balance();

    CHECK(ownership::pay_holders(w, 0, 100.0, ledger::Reason::Profit, nullptr) == 0.0);

    CHECK(w.firms[0].liquidity == doctest::Approx(100.0));
    CHECK(w.books_balance() == doctest::Approx(books));
}

TEST_CASE("pay_holders refuses a pot the firm cannot cover") {
    Rng rng(1);
    World w(10, 2, rng);
    set_register(w.firms[0], {{1, 100.0}});
    w.firms[0].liquidity = 10.0;
    CHECK_THROWS_AS(ownership::pay_holders(w, 0, 20.0, ledger::Reason::Profit, nullptr),
                    std::logic_error);
}

TEST_CASE("spending money does not shrink a stake") {
    // The whole point of the module: the old rule weighted profits by cash,
    // so a household that spent was paid less next month. Now it is not.
    Rng rng(1);
    World w(10, 2, rng);
    set_register(w.firms[0], {{1, 50.0}, {4, 50.0}});

    w.firms[0].liquidity = 100.0;
    ownership::pay_holders(w, 0, 100.0, ledger::Reason::Profit, nullptr);
    const double first_round = w.households[1].liquidity;

    // Household 1 spends everything it has; household 4 sits on its cash.
    ledger::transfer(w, ledger::household(1), ledger::firm(1), w.households[1].liquidity,
                     ledger::Reason::Purchase, nullptr);

    w.firms[0].liquidity = 100.0;
    const double before_1 = w.households[1].liquidity;
    const double before_4 = w.households[4].liquidity;
    ownership::pay_holders(w, 0, 100.0, ledger::Reason::Profit, nullptr);

    CHECK(first_round > 0.0);
    CHECK(w.households[1].liquidity - before_1 == doctest::Approx(50.0));
    CHECK(w.households[4].liquidity - before_4 == doctest::Approx(50.0));
}

TEST_CASE("a constructed world deals every firm a register") {
    Rng rng(3);
    World w(params::kNumHouseholds, params::kNumFirms, rng);
    for (const Firm& f : w.firms) {
        CHECK(ownership::total_shares(f) == doctest::Approx(ownership::kSharesPerFirm));
    }
}
