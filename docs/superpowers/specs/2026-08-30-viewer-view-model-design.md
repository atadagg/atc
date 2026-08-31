# Viewer view-model layer

Date: 2026-08-30
Status: approved, not yet implemented

## Purpose

Separate *what the viewer shows* from *how it is drawn*, so the display
technology can be replaced without touching the simulation or re-deriving
what is worth showing.

The motivation is longevity, not a pending port. There is no second skin
today and none is planned. Ways of displaying things change — hand-drawn,
moving pictures, ASCII, 2D, 3D, whatever follows — and this project is
expected to outlive at least one of those transitions. The cost of the seam
is paid once now; the cost of not having it is paid at every transition.

`DESIGN.md`'s tycoon front-end would consume this same layer, asking for
"the columns describing a firm" rather than reaching into `Firm`. The seam
pays off twice.

## Current state

The separation is already most of the way there and must stay that way:

- `src/` contains zero ImGui and zero SDL references. The core does not
  know a UI exists (invariant 5).
- `viewer_core` (`sim`, `ring_recorder`, `histogram`) is also ImGui-free,
  which is why the main test binary links it.
- ImGui is confined to `viewer/main.cpp` and `viewer/panel_*.cpp`, and the
  whole viewer is optional: `ATC_BUILD_VIEWER` defaults off when SDL3
  is absent, so the simulation and tests build with no UI library present.

ImGui is a leaf, not a trunk. The problem is not contamination of the core.

## The actual gap

The panels do not only draw — they decide. Roughly 60% of `panel_*.cpp` is
display-independent logic that a second skin would have to reimplement, and
would then drift from:

- `firm_passes()` — a pure predicate over `Firm` and `Filters`.
- Filtering agents into a row-index vector.
- The sort comparator — pure, except that it keys off
  `ImGuiTableColumnSortSpecs::ColumnIndex`, an ImGui type standing in for
  what should be a column identifier.
- `firm_field(w, which)` — a field accessor by integer.
- The schema: which columns exist, their headers, their precision, which
  distributions are drawn, which ones a bin click can pin.
- The detail panel's link structure: "this employee id selects that
  household" is a graph of navigable references; only the button is ImGui.

Genuinely ImGui: `BeginTable`/`Text`/`Selectable`, bar geometry, tooltips,
and the list clipper.

## Design principle

The view model encodes **meaning, never pixels**. "Column `wage`, numeric,
two decimals, sortable, this is its value" survives ASCII, 2D and 3D. "An
80-pixel-tall bar" does not. Any field that only makes sense for one
display technology does not belong in this layer.

## Approach

Pull-based, with a materialized schema and row list.

Considered and rejected:

- **Fully materialized snapshot** (build `TableView`/`DetailView` structs
  each frame, panels only render). Simplest to test, but formats all 1000
  household rows every frame, defeating the `ImGuiListClipper` that exists
  precisely to avoid that, and worsening the per-frame allocation already
  flagged as a concern at the 100k-agent target.
- **Full MVVM** with change notification and a retained view tree. The
  right answer for a 30-year UI and the wrong answer for today. YAGNI.

Chosen: materialize the *column schema* and the *row list* — filtering and
sorting must build those anyway and they are small — but pull *cells* on
demand by `(agent, column)`. The clipper keeps working, allocation stays
proportional to filtered agents rather than columns x agents, and every
part is testable without a window.

## Module boundary

Four new files, all in `viewer_core`, which links `atc_core` and
contains no ImGui or SDL. That property is a build-enforced invariant of
this design.

| File | Responsibility |
|---|---|
| `viewer/cell.hpp` | `Cell`, `CellKind`, `ColumnSpec`, `Filters`, `SortSpec` |
| `viewer/model_agents.{hpp,cpp}` | Firm and household table models, detail model |
| `viewer/model_dist.{hpp,cpp}` | Which distributions exist; values behind each |
| `viewer/model_time.{hpp,cpp}` | Which series exist; scrub bounds |

`panel_*.cpp` stay in the `atc_viewer` target and shrink to rendering.

`Filters` moves out of `App` into `cell.hpp`. The model layer must not know
a running app exists — it reads a `World`, nothing more.

## The cell

```cpp
enum class CellKind { Number, Count, Flag, AgentRef, Missing };

struct Cell {
    CellKind kind;
    double   number;     // valid for EVERY kind -- the universal sort key
    uint32_t ref;        // AgentRef only
    int      precision;  // Number only
};
```

`number` is populated for every kind — a flag as 0/1, a count as a double,
a reference as its index — so one comparator sorts any column and no skin
ever sorts formatted text. Cells never carry pre-formatted strings:
formatting is the skin's job, and a numeric skin (a bar height, a colour
ramp) must not have to parse text back into a number.

`Missing` represents the unemployed household's employer — absence as a
value rather than a magic string. It sorts below every present value.

`AgentRef` is what makes a cell navigable: ImGui draws a button, an ASCII
skin prints an id, a 3D skin moves a camera.

## Tables

```cpp
enum class AgentKind { Firm, Household };

struct ColumnSpec {
    const char* header;
    CellKind    kind;
    int         precision;
    bool        sortable;
};

struct SortSpec {
    int  column = -1;      // -1 means unsorted
    bool ascending = true;
};

std::span<const ColumnSpec> firm_columns();
std::vector<uint32_t>       firm_rows(const World&, const Filters&, SortSpec);
Cell                        firm_cell(const World&, uint32_t firm, int col);
```

Households get the same three functions. `firm_rows` filters and sorts and
returns agent indices — the materialized part. Sorting is stable, matching
current behaviour.

Column identity is the index into `firm_columns()`. `SortSpec` replaces the
ImGui sort-spec type in the comparator.

## Detail, distributions, series

The detail panel becomes a `DetailModel`: a list of `{label, Cell}` fields
plus link groups. A link group is a label and a list of references, each
with an optional numeric annotation — `{"employees", [refs]}` for a firm,
`{"shops", [refs annotated with unsatisfied demand]}` for a household. The
link graph is data; only the button is ImGui.

Distributions become a `DistSpec` table (`name`, which agent kind, whether
a bin click can pin those agents) plus a value accessor. `bin()` already
lives in `viewer_core`, so this closes the loop with existing code.

Series become a `SeriesSpec` table (`name`, `double Metrics::*`). Already
free of ImGui in substance; this moves it.

## Testing

TDD. The payoff is that every part is testable with no window, in the
existing `atc_tests` binary:

- filters select exactly the right agents, including the pinned-firm set
- sorting is correct and stable in both directions
- `Missing` sorts predictably against present values
- a histogram bin's members match the set a pin would produce
- the schema and the cell accessor agree on column count for every table
- every column index in range yields a cell of the kind the schema declares

If any test needs a GPU or a window, the seam has leaked.

## Out of scope

- No second skin. This is the seam, not a port.
- No renderer registry, plugin system, or layout-description files.
- No changes to `src/`.
- No change to what appears on screen: same columns, same numbers, same
  behaviour. This is a refactor, and a visible difference is a bug.

Invariants 2 (determinism) and 5 (core I/O only through `Recorder`) are
untouched — the model layer only reads `World`.
