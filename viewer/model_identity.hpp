#pragma once

// The viewer's half of firm identity. Display-independent: no ImGui, no SDL,
// no texture. A skin decides how to put these pixels on screen; this decides
// what the pixels are.
//
// Identity is a pure function of (run seed, firm index), so nothing here is
// cached, stored, or written back into World -- and, as in `identity` itself,
// no Rng is touched. Drawing a logo must not be able to move the economy.

#include <cstdint>
#include <string>
#include <vector>

#include "identity.hpp"
#include "identity_lexicon.hpp"

struct FirmIdentity {
    identity::Mark mark;
    identity::Name name;
};

FirmIdentity firm_identity(uint64_t run_seed, uint32_t firm, identity::Locale locale);

// "Head Tail" -- the name as one line, which is what a table row has room
// for. The lockups (Mark::lock) are an SVG concern; a row is not a lockup.
std::string display_name(const identity::Name& n);

const char* strategy_name(identity::Strategy s);

// An RGBA8 image of the mark alone, `size` x `size`. Pixels outside the
// silhouette are fully transparent, so a row draws the emblem and not a box.
//
// Every pixel comes from identity::paint_at -- the same sampler ink_coverage
// trusts -- rather than from a reimplementation of the shapes. That is the
// whole reason this rasterizes instead of issuing draw commands: the mark
// grammar already lives in two translation units, and a third would drift.
struct Bitmap {
    int size = 0;
    std::vector<uint32_t> px;   // 0xAABBGGRR, row-major
};

Bitmap rasterize(const identity::Mark& m, int size);
