#pragma once

// Renders a Mark and a Name to SVG. Opens no files: every function returns a
// string and the caller owns the handle (invariant 5).
//
// The output contains no <text> element and names no font family. A <text>
// element resolves a font against whoever is viewing, so the same firm would
// render differently per machine; every glyph here is an outlined <path>.

#include <cstdint>
#include <string>

#include "identity.hpp"
#include "identity_palette.hpp"

namespace identity {

// The emblem alone, drawn into a `size` box at (x, y). `uid` makes the clip
// path id unique -- two marks sharing an id on one sheet would clip each
// other's geometry.
std::string svg_mark(const Mark& m, double x, double y, double size, uint32_t uid);

// One line of outlined text with its left edge at (x, y) on the baseline.
// `width` receives the measured advance: lockups centre against it rather
// than against a guessed offset.
std::string svg_text(const char* s, uint8_t font, double x, double y, double size,
                     double tracking, Rgb colour, double* width = nullptr);

double text_width(const char* s, uint8_t font, double size, double tracking);

// Mark and name arranged by Mark::lock inside a w x h cell.
std::string svg_identity(const Mark& m, const Name& n, double w, double h,
                         uint32_t uid);

// A contact sheet of `count` identities generated from one run seed.
std::string svg_sheet(uint64_t run_seed, uint32_t count, Locale locale);

}  // namespace identity
