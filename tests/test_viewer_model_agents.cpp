#include <vector>

#include "agents.hpp"
#include "doctest.h"
#include "model_agents.hpp"
#include "params.hpp"
#include "rng.hpp"
#include "world.hpp"

namespace {

// A world small enough to reason about, with hand-set firm fields.
World tiny_world() {
    // World has no default constructor -- it seeds its agents from an Rng.
    // Three firms is below kShopsPerHousehold, so household shops repeat;
    // that is the documented test-only configuration, and no firm test
    // reads shops.
    static Rng rng(1);
    World w(3, 3, rng);
    w.firms[0].wage = 10.0;
    w.firms[0].price = 1.0;
    w.firms[0].inventory = 5.0;
    w.firms[0].liquidity = 100.0;
    w.firms[0].has_open_vacancy = false;
    w.firms[0].demand_last_month = 7.0;

    w.firms[1].wage = params::kMinWage;
    w.firms[1].price = 2.0;
    w.firms[1].inventory = 0.0;
    w.firms[1].liquidity = 50.0;
    w.firms[1].has_open_vacancy = true;
    w.firms[1].demand_last_month = 9.0;

    w.firms[2].wage = 30.0;
    w.firms[2].price = 3.0;
    w.firms[2].inventory = 2.0;
    w.firms[2].liquidity = 25.0;
    w.firms[2].has_open_vacancy = true;
    w.firms[2].employees = {0u, 1u};
    return w;
}

}  // namespace

TEST_CASE("the schema and the cell accessor agree on column count") {
    const World w = tiny_world();
    const auto cols = firm_columns();
    REQUIRE(cols.size() == 8);
    for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
        const Cell cell = firm_cell(w, 0u, c);
        CHECK(cell.kind == cols[static_cast<size_t>(c)].kind);
    }
}

TEST_CASE("an unfiltered, unsorted table is every firm in index order") {
    const World w = tiny_world();
    const std::vector<uint32_t> rows = firm_rows(w, Filters{}, SortSpec{});
    REQUIRE(rows.size() == 3);
    CHECK(rows[0] == 0u);
    CHECK(rows[1] == 1u);
    CHECK(rows[2] == 2u);
}

TEST_CASE("each filter selects exactly the firms it names") {
    const World w = tiny_world();

    Filters at_floor;
    at_floor.wage_at_floor = true;
    const std::vector<uint32_t> floor_rows = firm_rows(w, at_floor, SortSpec{});
    REQUIRE(floor_rows.size() == 1);
    CHECK(floor_rows[0] == 1u);

    Filters vacancy;
    vacancy.vacancy_open = true;
    const std::vector<uint32_t> vacancy_rows = firm_rows(w, vacancy, SortSpec{});
    REQUIRE(vacancy_rows.size() == 2);
    CHECK(vacancy_rows[0] == 1u);
    CHECK(vacancy_rows[1] == 2u);

    Filters empty_shelf;
    empty_shelf.inventory_zero = true;
    const std::vector<uint32_t> empty_rows = firm_rows(w, empty_shelf, SortSpec{});
    REQUIRE(empty_rows.size() == 1);
    CHECK(empty_rows[0] == 1u);
}

TEST_CASE("filters intersect rather than union") {
    const World w = tiny_world();
    Filters both;
    both.vacancy_open = true;
    both.inventory_zero = true;
    const std::vector<uint32_t> rows = firm_rows(w, both, SortSpec{});
    REQUIRE(rows.size() == 1);
    CHECK(rows[0] == 1u);
}

TEST_CASE("pinned firms restrict the table to that set") {
    const World w = tiny_world();
    Filters pinned;
    pinned.pinned_firms = {2u, 0u};
    const std::vector<uint32_t> rows = firm_rows(w, pinned, SortSpec{});
    REQUIRE(rows.size() == 2);
    // Pinning restricts membership; it does not impose the pin's order.
    CHECK(rows[0] == 0u);
    CHECK(rows[1] == 2u);
}

TEST_CASE("sorting works in both directions on a numeric column") {
    const World w = tiny_world();
    const int wage_col = 1;

    const std::vector<uint32_t> asc = firm_rows(w, Filters{}, SortSpec{wage_col, true});
    REQUIRE(asc.size() == 3);
    CHECK(asc[0] == 1u);  // kMinWage
    CHECK(asc[2] == 2u);  // 30.0

    const std::vector<uint32_t> desc = firm_rows(w, Filters{}, SortSpec{wage_col, false});
    CHECK(desc[0] == 2u);
    CHECK(desc[2] == 1u);
}

TEST_CASE("sorting a flag column orders false before true") {
    const World w = tiny_world();
    const int vacancy_col = 6;
    const std::vector<uint32_t> rows = firm_rows(w, Filters{}, SortSpec{vacancy_col, true});
    CHECK(rows[0] == 0u);  // the only firm with no vacancy
}

TEST_CASE("staff is a count of employees") {
    const World w = tiny_world();
    const Cell staff = firm_cell(w, 2u, 4);
    CHECK(staff.kind == CellKind::Count);
    CHECK(staff.number == 2.0);
}

TEST_CASE("the firm id column is a navigable reference") {
    const World w = tiny_world();
    const Cell id = firm_cell(w, 2u, 0);
    CHECK(id.kind == CellKind::AgentRef);
    CHECK(id.ref == 2u);
}

namespace {

World household_world() {
    static Rng rng(2);
    World w(3, 2, rng);
    w.households[0].employer = 1u;
    w.households[0].liquidity = 10.0;
    w.households[0].reservation_wage = 5.0;
    w.households[0].planned_consumption = 2.0;

    w.households[1].employer = NO_FIRM;  // unemployed
    w.households[1].liquidity = 20.0;
    w.households[1].reservation_wage = 6.0;
    w.households[1].planned_consumption = 3.0;

    w.households[2].employer = 0u;
    w.households[2].liquidity = 30.0;
    w.households[2].reservation_wage = 7.0;
    w.households[2].planned_consumption = 4.0;
    return w;
}

}  // namespace

TEST_CASE("the household schema and cell accessor agree on column count") {
    const World w = household_world();
    const auto cols = household_columns();
    REQUIRE(cols.size() == 5);
    for (int c = 0; c < static_cast<int>(cols.size()); ++c) {
        const Cell cell = household_cell(w, 0u, c);
        CHECK(cell.kind == cols[static_cast<size_t>(c)].kind);
    }
}

TEST_CASE("an unemployed household's employer is Missing, not a sentinel") {
    const World w = household_world();
    const Cell employed = household_cell(w, 0u, 1);
    CHECK(employed.kind == CellKind::AgentRef);
    CHECK(employed.ref == 1u);

    const Cell jobless = household_cell(w, 1u, 1);
    CHECK(jobless.kind == CellKind::Missing);
}

TEST_CASE("the unemployed filter selects only the jobless") {
    const World w = household_world();
    Filters f;
    f.unemployed = true;
    const std::vector<uint32_t> rows = household_rows(w, f, SortSpec{});
    REQUIRE(rows.size() == 1);
    CHECK(rows[0] == 1u);
}

TEST_CASE("sorting households on employer puts the unemployed first") {
    // The point of Missing: 0xFFFFFFFF would otherwise sort the jobless to
    // the top as if they had the highest-numbered employer in the world.
    const World w = household_world();
    const std::vector<uint32_t> rows = household_rows(w, Filters{}, SortSpec{1, true});
    REQUIRE(rows.size() == 3);
    CHECK(rows[0] == 1u);  // unemployed
    CHECK(rows[1] == 2u);  // employer 0
    CHECK(rows[2] == 0u);  // employer 1
}
