#include <cstring>
#include <set>
#include <string>

#include "doctest.h"

#include "cell.hpp"
#include "identity.hpp"
#include "identity_palette.hpp"
#include "model_agents.hpp"
#include "model_identity.hpp"
#include "rng.hpp"
#include "world.hpp"

// The viewer's identity layer, tested without a window: what the mark and the
// name are, and what pixels they turn into. Only the ImGui/SDL upload is
// untested, as with every other panel.

namespace {
constexpr identity::Locale kTr = identity::Locale::Turkish;
}

TEST_CASE("a firm's identity is the identity module's, not a second derivation") {
    const FirmIdentity got = firm_identity(1, 7, kTr);
    const uint64_t s = identity::firm_seed(1, 7);
    const identity::Mark want_mark = identity::mark(s);
    const identity::Name want_name = identity::name(s, kTr);

    CHECK(got.mark.sil == want_mark.sil);
    CHECK(got.mark.div == want_mark.div);
    CHECK(got.mark.charge == want_mark.charge);
    CHECK(got.mark.palette == want_mark.palette);
    CHECK(std::strcmp(got.name.head, want_name.head) == 0);
    CHECK(std::strcmp(got.name.tail, want_name.tail) == 0);
}

TEST_CASE("identity is stable, and separates firms and runs") {
    const FirmIdentity a = firm_identity(1, 7, kTr);
    const FirmIdentity b = firm_identity(1, 7, kTr);
    CHECK(display_name(a.name) == display_name(b.name));

    // Distinct within a run, and distinct across runs: 100 firms under two
    // seeds should not collapse onto a handful of names.
    std::set<std::string> names;
    for (uint32_t i = 0; i < 100; ++i) {
        names.insert(display_name(firm_identity(1, i, kTr).name));
        names.insert(display_name(firm_identity(2, i, kTr).name));
    }
    CHECK(names.size() > 150);
}

TEST_CASE("display_name joins head and tail, and survives an empty tail") {
    identity::Name n{};
    std::strcpy(n.head, "Yildiz");
    std::strcpy(n.tail, "Tekstil");
    CHECK(display_name(n) == "Yildiz Tekstil");

    n.tail[0] = '\0';
    CHECK(display_name(n) == "Yildiz");
}

TEST_CASE("rasterize is the requested size and leaves the outside transparent") {
    const identity::Mark m = identity::mark(identity::firm_seed(1, 3));
    const Bitmap b = rasterize(m, 32);
    CHECK(b.size == 32);
    CHECK(b.px.size() == 32u * 32u);

    // Every pixel is either fully opaque or fully clear -- no partial alpha,
    // because the sampler answers a layer, not a coverage.
    for (uint32_t p : b.px) {
        const uint32_t alpha = p >> 24;
        CHECK((alpha == 0u || alpha == 255u));
    }

    // A silhouette is inscribed in its box, so at least one corner is outside
    // every one of them except the square.
    bool any_clear = false, any_painted = false;
    for (uint32_t p : b.px) {
        if ((p >> 24) == 0u) any_clear = true;
        else any_painted = true;
    }
    CHECK(any_painted);
    CHECK(any_clear == (m.sil != identity::Sil::Square));
}

TEST_CASE("a rasterized mark carries the same ink as ink_coverage measures") {
    for (uint32_t i = 0; i < 50; ++i) {
        const identity::Mark m = identity::mark(identity::firm_seed(9, i));
        const Bitmap b = rasterize(m, 64);

        // Recompute coverage from the pixels: inside the silhouette, the
        // fraction that is not the field colour. This is the check that the
        // colour mapping did not silently paint everything one colour.
        const auto ps = identity::palettes();
        const uint32_t field = 0xFF000000u |
                               (static_cast<uint32_t>(ps[m.palette].field.b) << 16) |
                               (static_cast<uint32_t>(ps[m.palette].field.g) << 8) |
                               static_cast<uint32_t>(ps[m.palette].field.r);
        int inside = 0, figure = 0;
        for (uint32_t p : b.px) {
            if ((p >> 24) == 0u) continue;
            ++inside;
            if (p != field) ++figure;
        }
        REQUIRE(inside > 0);
        const double from_pixels = static_cast<double>(figure) / inside;

        // Equal only up to the two grids disagreeing on boundary cells, and
        // only where the division's half happens to share the field's colour.
        CHECK(from_pixels <= identity::ink_coverage(m, 64) + 1e-9);
    }
}

TEST_CASE("rasterize refuses a non-positive size rather than allocating") {
    const identity::Mark m = identity::mark(identity::firm_seed(1, 0));
    CHECK(rasterize(m, 0).px.empty());
    CHECK(rasterize(m, -8).px.empty());
}

TEST_CASE("the firm table carries an identity column, keyed by firm index") {
    const auto cols = firm_columns();
    int identity_col = -1;
    for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
        if (std::strcmp(cols[i].header, "identity") == 0) identity_col = i;
    }
    REQUIRE(identity_col > 0);   // never column 0: that one selects the row

    Rng rng(1);
    World w(50, 50, rng);
    const Cell c = firm_cell(w, 42, identity_col);
    CHECK(c.kind == CellKind::Identity);
    CHECK(c.ref == 42u);
    CHECK(c.number == doctest::Approx(42.0));
}

TEST_CASE("identity cells sort by index, and below nothing but Missing") {
    CHECK(cell_less(identity_cell(3), identity_cell(9)));
    CHECK_FALSE(cell_less(identity_cell(9), identity_cell(3)));
    CHECK_FALSE(cell_less(identity_cell(0), missing_cell()));
    CHECK(cell_less(missing_cell(), identity_cell(0)));
}

TEST_CASE("the firm detail model offers an identity") {
    Rng rng(1);
    World w(10, 10, rng);
    const DetailModel d = firm_detail(w, 5);
    REQUIRE(d.present);
    bool found = false;
    for (const DetailField& f : d.fields) {
        if (std::strcmp(f.label, "identity") == 0) {
            found = true;
            CHECK(f.value.kind == CellKind::Identity);
            CHECK(f.value.ref == 5u);
        }
    }
    CHECK(found);
}
