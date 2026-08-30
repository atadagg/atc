#include <vector>

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
