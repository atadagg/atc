#include "model_identity.hpp"

#include "identity_palette.hpp"

FirmIdentity firm_identity(uint64_t run_seed, uint32_t firm, identity::Locale locale) {
    const uint64_t s = identity::firm_seed(run_seed, firm);
    return FirmIdentity{identity::mark(s), identity::name(s, locale)};
}

std::string display_name(const identity::Name& n) {
    std::string s = n.head;
    if (n.tail[0] != '\0') {
        if (!s.empty()) s += ' ';
        s += n.tail;
    }
    return s;
}

const char* strategy_name(identity::Strategy s) {
    switch (s) {
    case identity::Strategy::Founder:     return "founder";
    case identity::Strategy::Descriptive: return "descriptive";
    case identity::Strategy::Toponymic:   return "toponymic";
    case identity::Strategy::Compound:    return "compound";
    case identity::Strategy::Coined:      return "coined";
    case identity::Strategy::Acronym:     return "acronym";
    default:                              return "evocative";
    }
}

Bitmap rasterize(const identity::Mark& m, int size) {
    Bitmap b;
    if (size <= 0) return b;
    b.size = size;
    b.px.assign(static_cast<size_t>(size) * static_cast<size_t>(size), 0u);

    const auto ps = identity::palettes();
    const identity::Palette& p = ps[m.palette];
    const identity::Palette& q = ps[m.palette2];

    // Layer -> colour, in the same order svg_mark emits its fills: field,
    // then the divided half, then the charge.
    const identity::Rgb colour[4] = {p.field, p.field, q.field, p.ink};

    for (int j = 0; j < size; ++j) {
        for (int i = 0; i < size; ++i) {
            const double x = (i + 0.5) * 100.0 / size;
            const double y = (j + 0.5) * 100.0 / size;
            const uint8_t layer = identity::paint_at(m, x, y);
            if (layer == 0) continue;   // outside the silhouette: transparent
            const identity::Rgb c = colour[layer];
            b.px[static_cast<size_t>(j) * static_cast<size_t>(size) +
                 static_cast<size_t>(i)] =
                0xFF000000u | (static_cast<uint32_t>(c.b) << 16) |
                (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.r);
        }
    }
    return b;
}
