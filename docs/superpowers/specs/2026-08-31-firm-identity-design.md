# Firm identity: procedural marks

*2026-08-31*

Every firm gets a logo. Not an asset on disk — a **pure function of the
firm's seed**, evaluated on demand, identical in the viewer, in an SVG
export, and in whatever renders the economy next.

The target is a million firms. That number turns out not to be the hard
part: a hash is O(1) and stores nothing. The hard part is that a million
seeded draws must still look like a directory of institutions rather than
a bag of shapes, and that is a curation problem, not a throughput one.

## Part 0 — what belongs to the engine, and what belongs to this

The engine's job is that identity **cannot perturb the simulation**. That
is Part 1, it is an invariant, and it holds no matter what the marks look
like.

Everything else here — the primitives, the palette, the decision tree — is
taste, and is expected to change. Keep the two apart when editing this
file. A change to the grammar is a redesign; a change to Part 1 is a bug.

## Part 1 — the determinism rule

**`identity` never touches `Rng`.** It has its own stateless hash: a
splitmix64-style finalizer over a `uint64_t`, no engine, no state.

This is not a preference. `Rng` is a stateful `mt19937_64`, and every draw
advances a stream the simulation depends on. If generating a mark drew
from it, then *how many logos the viewer happened to draw this frame*
would change the economy — scrolling a panel would alter unemployment.
That is invariant 2 destroyed by a UI event, and it would be maddening to
diagnose, because the CSV would differ only when someone had the window
open.

Two consequences worth having on purpose:

- **Random access.** Firm 999,999's mark costs exactly what firm 0's
  costs. No iteration, no warm-up, no ordering effect.
- **The test is cheap.** Run the reference months while generating a
  million marks; diff against a run that generates none. Byte-identical,
  or the rule is broken. See Part 6.

## Part 2 — zero bytes per firm

`Firm` gains **no field**. A mark is derived from
`mix(run_seed, firm_index)` whenever someone wants one.

The alternative — a `uint32_t logo` on `Firm` — was considered and
rejected. It is redundant state (the hash already determines it), it
widens the hot agent array, and it would have to survive `SimSnapshot`
correctly forever. Invariants 3 and 4 exist to keep agents trivially
copyable so the snapshot stays a plain assignment; adding derivable bytes
to `Firm` spends that budget for nothing.

Caching is allowed, but **only on the output side** — the viewer may keep
a `Mark` per visible row. `World` never holds one.

`World` does not currently store its seed. The core stays agnostic:

```cpp
namespace identity {
uint64_t firm_seed(uint64_t run_seed, uint32_t firm_index);
Mark     mark(uint64_t seed);
}
```

The caller supplies the run seed — the viewer's `App` and `Sim` both know
it. Folding the run seed in means different runs are different *companies*,
which is what a game wants. A caller that wants firm 7 to be the same firm
in every run can pass a constant; that is policy, and it stays at the call
site rather than being baked in here.

## Part 3 — the `Mark` type

POD, trivially copyable, no allocation, ~32 bytes:

```cpp
enum class Prim : uint8_t { Disc, Ring, Square, Diamond, Bar, Notch };

struct Shape {
    Prim    prim;
    uint8_t cx, cy;    // centre, in the unit box
    uint8_t size;
    uint8_t rot;       // quarter/eighth turns; not free rotation
    uint8_t ink;       // index into the palette, never a literal colour
};

struct Mark {
    Shape   ops[4];   // capacity; the grammar in Part 4 emits at most 3
    uint8_t count;
    uint8_t palette;
};
```

The array holds four and the grammar fills at most three. The spare slot is
padding the struct would have taken anyway, and it means a fourth element
does not change the type's size or its `static_assert`. `count` is the
authority on how many are live; nothing may read past it.

Coordinates live in a **fixed 0–255 unit box**, so the two backends agree
exactly rather than approximately. A float box would let SVG and ImGui
round differently and produce marks that are subtly not the same logo.

**No boolean geometry.** Shapes paint in order; later ones cover earlier
ones. An aperture is a shape painted in the field colour, not a subtraction.
This is what keeps both renderers to ordered fills — which SVG and ImGui
each do natively — instead of needing a path-boolean library on both sides.

`ink` is a palette index, never a colour literal. A mark is therefore
re-skinnable (dark plate, light plate, a future theme) without regenerating
it, and the palette stays a single table to edit.

## Part 4 — the grammar

Generation is a **short decision tree**, not free layering. From the seed:

1. **Ground** — disc, ring, or square. Always centred, always full-size.
2. **Aperture** — square, diamond, circle, or none. Size from the seed,
   centred.
3. **Accent** — at most one: a bar, a notch, or a second aperture.
   This is the only element permitted the accent ink.

Three constraints do all the work, and none is negotiable without
redesigning the look:

- **Fixed palette.** The house set — navy, red, paperwhite — plus a small
  number of sanctioned variants. Never a sampled hue. This alone is most
  of why output reads as institutional.
- **Enforced symmetry.** Every element centred or mirrored; rotation
  quantised to eighth turns. Asymmetry is what makes generated marks look
  accidental.
- **At most three shapes.** A mark that must survive a 16px favicon cannot
  afford a fourth.

The reference is East Asian bank identity read structurally — ICBC and
Bank of China's coin-with-a-square-aperture, SMBC, Bank of the Ryukyus.
What is borrowed is the *construction*: an emblem rather than a letterform,
one saturated ink, heavy negative space. The repo's own masthead
(`docs/img/gen_banner.py`) is the same grammar with `count == 2`, and is
the worked example.

**A mark never spells anything.** The masthead went through four attempts
at hiding an "A" and a "T" in the emblem; every one read as a keyhole or a
machine part, and none survived being shrunk. Letters belong in a wordmark.

## Part 5 — backends

| Unit | Input | Output | Depends on |
|---|---|---|---|
| `src/identity.{hpp,cpp}` | `uint64_t` | `Mark` | nothing |
| `src/identity_svg.{hpp,cpp}` | `Mark` | `std::string` | `identity` |
| `viewer/mark_draw.{hpp,cpp}` | `Mark` | `ImDrawList` calls | `identity`, ImGui |

`identity_svg` **returns a string and opens no file**, following
`scenario.{hpp,cpp}`: the core does I/O only through `Recorder`
(invariant 5), so the caller owns the handle and the writer is testable
without disk.

`mark_draw` is the only piece that needs a window, and it is the only
piece that is not unit-tested — the same split the viewer already makes
between `viewer_core` and the ImGui panels.

**The iteration loop.** `main.cpp` gains:

```
./build/atc --identity-sheet output/marks.svg --identity-count 1000
```

A contact sheet of N marks in one SVG. This is how the grammar gets tuned:
render it, look at a thousand marks at once, and widen or narrow the
parameter ranges. Guessing at the parameter ranges without looking at the
population is how this ends up samey.

## Part 6 — testing

| Test | Asserts |
|---|---|
| purity | same seed → identical `Mark`, independent of call order and count |
| **non-perturbation** | reference months generating 1M marks vs generating none → byte-identical CSV |
| layout | `static_assert(std::is_trivially_copyable_v<Mark>)` |
| well-formedness | over 1M seeds: `count` in range, every shape inside the unit box, every `ink` a valid palette index |
| coverage | over 1M seeds: distinct marks above a floor, and no single mark over-represented |
| svg | golden strings for a handful of fixed seeds |

The non-perturbation test is the one with teeth. The others check that the
generator works; that one checks it cannot reach into the simulation, which
is the only way this subsystem can cause a bug that matters.

The coverage test is deliberately two-sided. "A million distinct marks"
is satisfied by a generator that also emits one mark 40% of the time; the
over-representation bound is what actually catches a decision tree whose
branches are unbalanced.

## Part 7 — scope

**Not in this spec, by decision:**

- **The name generator.** It slots beside the mark off the same seed —
  `identity::name(seed)` next to `identity::mark(seed)` — and is why this
  module is called `identity` rather than `logo`. Its own spec.
- **Multilinguality.** After names exist.
- **Sector or live-state coupling.** A mark is *pure identity*: fixed for
  the firm's life, whatever happens to it. Encoding size or distress would
  make it a chart rather than a brand, and would mean recomputing per firm
  per frame. Revisit only when output classes (extension 4) exist, and then
  as a shape *family*, never as live state.

## Open risk

**A wall of a thousand marks may look samey.** That is the honest cost of
choosing one house grammar over several families, and it will not be
visible until the contact sheet exists. The mitigation is the sheet itself,
early: if the population is too uniform, the fix is wider parameter ranges
and more sanctioned palette variants, not a second grammar. Adding
grammars is the thing that turns a coherent world into a clip-art set.
