#pragma once

#include <cstdint>
#include <iosfwd>

#include "intervention.hpp"

// A save file. The run is a fold over its inputs, so the inputs *are* the save:
// a seed and the log of what the operator did. Kilobytes, not a state dump, and
// it stays readable a year later because parameters are addressed by name.
struct Scenario {
    uint64_t seed = 1;
    intervention::Log log;
};

// Reading and writing work on streams, never on paths. The core does not open
// files (invariant 5) -- `main` and the viewer own the file handle and hand a
// stream in, which is also what lets these be tested without touching disk.
namespace scenario {

// Format, one record per line, blank lines and `#` comments ignored:
//
//   atc-scenario 1
//   seed 42
//   60  set_param  min_wage 12.5
//   100 issue_all  <issuer> <amount each>
//   120 issue      <issuer> <household> <amount>
//
inline constexpr int kFormatVersion = 1;

void write(const Scenario& s, std::ostream& out);

// Throws std::runtime_error on anything malformed. Deliberately strict: a
// scenario silently half-read is a run that is not the run anyone asked for,
// and this is a reproducibility tool.
//
// `seq` is renumbered on read. The file is written in (month, seq) order and
// re-read in that order, so *intra-month* order -- the only kind that decides
// anything -- is preserved, while the absolute values need not match the log
// that was written.
Scenario read(std::istream& in);

}  // namespace scenario
