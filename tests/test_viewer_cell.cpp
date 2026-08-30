#include "cell.hpp"
#include "doctest.h"

TEST_CASE("every cell kind carries a sort key in number") {
    CHECK(number_cell(2.5, 2).number == 2.5);
    CHECK(count_cell(7).number == 7.0);
    CHECK(flag_cell(true).number == 1.0);
    CHECK(flag_cell(false).number == 0.0);
    CHECK(ref_cell(42u).number == 42.0);
    CHECK(ref_cell(42u).ref == 42u);
}

TEST_CASE("cells compare on their numeric key") {
    CHECK(cell_less(number_cell(1.0, 2), number_cell(2.0, 2)));
    CHECK_FALSE(cell_less(number_cell(2.0, 2), number_cell(1.0, 2)));
    CHECK_FALSE(cell_less(number_cell(1.0, 2), number_cell(1.0, 2)));
}

TEST_CASE("missing sorts below every present value, including negatives") {
    // An unemployed household has no employer. Absence is a value, and it
    // must not be ordered by whatever number happens to sit in the struct.
    CHECK(cell_less(missing_cell(), number_cell(-1000.0, 2)));
    CHECK_FALSE(cell_less(number_cell(-1000.0, 2), missing_cell()));
}

TEST_CASE("two missing cells are equivalent, so sorting them is stable") {
    CHECK_FALSE(cell_less(missing_cell(), missing_cell()));
}
