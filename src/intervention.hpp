#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ledger.hpp"
#include "params.hpp"

class World;

// The intervention log makes a run reproducible from its *inputs* rather than
// from its state: a run is (seed + log), not seed alone.
//
// That is what lets a parameter change mid-run without breaking invariant 2.
// It matters most for the viewer: scrubbing backward restores a snapshot and
// re-simulates forward, so a change made at month 300 must be reapplied at
// exactly month 300 on the way back through -- otherwise scrubbing back and
// forward silently yields a different history than the one just watched.
//
// The general name is event sourcing. `SimSnapshot` is already the cache of
// the fold; this is the log the fold runs over.
namespace intervention {

// A behavioural parameter, addressed for the log by a stable id that is an
// index into the one table below.
//
// The table is the single source of truth. There is deliberately no parallel
// enum of parameter ids: two lists of the same thing drift, and a drifted
// entry here would silently steer the wrong knob rather than fail to compile.
struct ParamSpec {
    std::string_view name;
    void (*set)(Params&, double);
    double (*get)(const Params&);
};

std::span<const ParamSpec> param_specs();

inline constexpr uint16_t kNoParam = 0xFFFFu;

// Index into param_specs(), or kNoParam. Names are the stable identity across
// save files; the index is the compact form used in the log.
uint16_t find_param(std::string_view name);

enum class Action : uint8_t {
    SetParam,             // param, value
    Issue,                // issuer -> household `target`, of `value`
    IssueToAllHouseholds, // issuer -> every household, `value` each
};

// A plain aggregate, so a log is trivially copyable and serializable.
struct Intervention {
    int month = 0;
    uint32_t seq = 0;  // total order within a month; load-bearing
    Action action = Action::SetParam;
    uint16_t param = kNoParam;
    uint32_t issuer = 0;
    uint32_t target = 0;
    double value = 0.0;
};

// Applies one intervention. Journaling is passed through because an issuance
// moves money and must appear in the book like any other movement.
void apply(World& w, const Intervention& i, ledger::Journal* journal = nullptr);

// Append-only, kept sorted by (month, seq). `seq` is assigned on insertion and
// is monotonic across the whole log, so two changes in the same month always
// have a defined order -- without it, nondeterminism returns at a finer grain
// than the one the log exists to fix.
class Log {
public:
    const Intervention& add(Intervention i);

    void set_param(int month, std::string_view name, double value);
    void issue(int month, uint32_t issuer, uint32_t household, double amount);
    void issue_to_all_households(int month, uint32_t issuer, double each);

    // Everything due at `month`, in seq order. Empty is the common case.
    std::span<const Intervention> due(int month) const;

    std::span<const Intervention> all() const { return entries_; }
    bool empty() const { return entries_.empty(); }

private:
    std::vector<Intervention> entries_;
    uint32_t next_seq_ = 0;
};

}  // namespace intervention
