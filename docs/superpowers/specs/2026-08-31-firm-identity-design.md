# Firm identity: procedural marks and names

*2026-08-31*

Every firm gets a logo and a name. Not assets on disk — **pure functions of
the firm's seed and its locale**, evaluated on demand, identical in the
viewer, in an SVG export, and in whatever renders the economy next.

The target is a million firms. That is not the hard part: a hash is O(1) and
stores nothing. The hard parts are that a million generated identities must
still look designed, and that a firm's nationality must agree with where it
is.

This spec supersedes an earlier version that described a single abstract-mark
grammar with four primitives and deferred names. That design was prototyped,
measured, and found too narrow; see Part 9 for what the measurements were and
Part 10 for what is still unresolved.

## Part 0 — what belongs to the engine, and what belongs to taste

The engine's job is that identity **cannot perturb the simulation** (Part 1)
and **cannot disagree with geography** (Part 2). Those are invariants.

Everything else — primitives, palettes, lockups, lexicons — is taste, and is
expected to change. A change to the grammar is a redesign; a change to Parts 1
or 2 is a bug.

## Part 1 — the determinism rule

**`identity` never touches `Rng`.** It has its own stateless hash: a
splitmix64-style finalizer over a `uint64_t`, no engine, no state.

`Rng` is a stateful `mt19937_64` and every draw advances a stream the
simulation depends on. If generating a mark drew from it, *how many logos the
viewer happened to draw this frame* would change the economy — scrolling a
panel would alter unemployment. That is invariant 2 destroyed by a UI event,
and it would be near-impossible to diagnose, because the CSV would differ only
when someone had the window open.

Two consequences, on purpose:

- **Random access.** Firm 999,999's identity costs what firm 0's costs.
- **The test is cheap.** Run the reference months while generating a million
  identities; diff against a run that generates none. Byte-identical, or the
  rule is broken.

## Part 2 — zero bytes per firm, and locale as a parameter

`Firm` gains **no field**. Identity derives from `mix(run_seed, firm_index)`.

A `uint32_t logo` on `Firm` was considered and rejected: redundant state that
widens the hot agent array and must survive `SimSnapshot` forever. Invariants
3 and 4 keep agents trivially copyable so the snapshot is a plain assignment;
derivable bytes must not spend that budget.

**Locale is a parameter, never a field:**

```cpp
namespace identity {
uint64_t firm_seed(uint64_t run_seed, uint32_t firm_index);
Mark     mark(uint64_t seed);
Name     name(uint64_t seed, Locale locale);
}
```

Where the locale comes from is the caller's business. Today: a world-level
locale mix, hashed per firm. When regions arrive (extension 3):
`locale = locale_of(region_of(firm))`. The signature does not change.

This is what makes "a Swedish company in Vietnam" **structurally impossible**
rather than merely discouraged — locale flows from place, and nothing draws it
independently. Nationality is never stored and never lost: it is re-derivable
at any moment from `(region, seed)`, exactly like the mark.

Caching is permitted **only on the output side** — the viewer may hold a
`Mark` per visible row. `World` never does.

## Part 3 — the types

POD, trivially copyable, no allocation.

```cpp
enum class Prim : uint8_t { Disc, Ring, Square, Diamond, Bar, Cross,
                            Saltire, Roundel, Lozenge, Annulet };
enum class Sil  : uint8_t { Disc, Block, Square };
enum class Div  : uint8_t { Plain, Pale, Fess, Quarterly, Chief };
enum class Kind : uint8_t { Abstract, Lettermark, Wordmark, Combination };
enum class Lock : uint8_t { Horizontal, Stacked, MarkRight, Lettermark,
                            Wordmark, StackedWords, Enclosed };

struct Mark {
    Sil     sil;
    Div     div;
    Prim    charge;
    uint8_t charge_size;
    uint8_t field, field2, ink;   // palette indices, never colour literals
    Kind    kind;
    Lock    lock;
    uint8_t font;                 // index into the vendored font table
};
```

Geometry lives in a **fixed 0–255 unit box** so the SVG and ImGui backends
agree exactly rather than approximately.

**No boolean geometry.** Shapes paint in order; later ones cover earlier ones.
An aperture is a shape painted in the field colour, not a subtraction. Both
backends then need only ordered fills, which each does natively.

`field`/`ink` are palette *indices*. A mark is re-skinnable without
regeneration, and the palette stays one table.

## Part 4 — the mark grammar

Three axes, and keeping them separate is what stops variety becoming a
clip-art set:

| Axis | What it varies | Why it matters |
|---|---|---|
| **Type** | abstract / lettermark / wordmark / combination | the conventional brand-name taxonomy; abstract alone is the *least* distinguishable at small sizes |
| **Lockup** | where text sits relative to the mark | changes the silhouette of the whole identity; contributes more perceived variety than geometry does |
| **Style** | palette, silhouette, typeface, weight | shared across all types — this is what makes the population one world |

The emblem grammar is **heraldic in structure, corporate in treatment**:
field divisions and ordinaries from heraldry, with the shield silhouette, the
outline stroke, diagonal partitions and the star all removed. Straight
heraldry was prototyped and reads as football crests.

**The rule of tincture is load-bearing.** Heraldry forbids colour on colour
and metal on metal. Adopting it gives a **structural contrast guarantee**: the
grammar cannot emit a low-contrast pairing, so legibility needs no per-mark
check. It is the constraint we would otherwise have had to invent.

Constraints that are not negotiable without a redesign:

- **Palette is a fixed sanctioned set**, never a sampled hue.
- **Enforced symmetry** — centred or mirrored; rotation quantised.
- **At most three shapes** — a mark that must survive 16px cannot afford four.
- **A mark never spells anything.** Letters belong in the wordmark or the
  lettermark, never hidden in the emblem. Four attempts at hiding "AT" in the
  masthead emblem all read as a keyhole.

### The invisibility rule

**No element may be painted the same colour as what is behind it.** This
defect appeared three times in prototyping, each time looking like a different
bug: a field-coloured aperture inside a ring (invisible — the ring's centre is
already field), two palette entries too close to distinguish, and an argent
field on a white card. The grammar must know its ground, and the palette must
be **perceptually spaced**, not merely composed of distinct hex values.

## Part 5 — names

A name is a **strategy applied to a locale's lexicons**. Company names are
built from meaning, not from sound; a character model over place names
generates geography, which is the wrong thing.

Strategies, from the naming literature:

| Strategy | Construction | Example |
|---|---|---|
| Founder | surname + sector | Koç Holding |
| Descriptive | qualifier + sector | İş Bankası |
| Toponymic | place + sector | Anadolu Sigorta |
| Compound | two morphemes joined | Şişecam (*şişe*+*cam*) |
| Coined | truncate-and-join | Petkim (*pet*rol+*kim*ya) |
| Acronym | initials of a longer descriptive name | THY |
| Evocative | nature/abstract word + sector | Yıldız Holding |

Each `Locale` supplies: surnames, materials, nature words, qualifiers, places,
sectors, legal forms — and its own **coining rule**, because truncate-and-join
gives Turkish the right answer and Japanese garbage (`TETKOG`, `DOBOEK`) from
the same inputs. Japanese coins from qualifier+sector (*Nip*pon *San*gyo →
Nissan), not material+sector.

### Two rules learned the hard way

**Minimum pool size per slot.** Repetition is set by the *smallest* pool in a
template, not by the size of the product. This surfaced three times — a
suffix list that made five names in 36 end in `VOLD`, an 80-word Markov corpus
that emitted duplicates and fragments, and a 9-word qualifier list that
produced `ALLGEMEINE` three times in 14. Every slot needs a floor, and
coverage must be measured **per slot**, never in aggregate.

**No real companies in the lexicons.** Seeding surnames with conglomerate
names produced `KOÇ HOLDING` and `RHEINSTAHL`, which are real firms. Lexicons
carry ordinary surnames and common words; generated names are checked against
a list of known real companies and rejected on a hit.

## Part 6 — typography

**Fonts are vendored, and all text is outlined to paths.** Both halves are
correctness requirements, not preferences.

A system font referenced by name resolves against whoever is *viewing* — so
the same firm would have a different identity on a different machine, and a
README banner renders in whatever GitHub happens to have. This bug was
documented for the masthead, then reintroduced in every prototype sheet after
it; the sheets looked correct locally and fell back to a default font for the
user. Outlining removes font resolution from the rendering path entirely.

Vendored fonts must be OFL. Width variation within one family (DINish
Condensed / Regular / Expanded) carries most of the typographic variety while
keeping the population coherent; a serif is the one genuine change of voice,
and is what makes a firm read as an old institution.

## Part 7 — backends

| Unit | Input | Output | Depends on |
|---|---|---|---|
| `src/identity.{hpp,cpp}` | `uint64_t`, `Locale` | `Mark`, `Name` | nothing |
| `src/identity_lexicon.{hpp,cpp}` | — | locale tables | nothing |
| `src/identity_svg.{hpp,cpp}` | `Mark`, `Name` | `std::string` | `identity`, font outlines |
| `viewer/mark_draw.{hpp,cpp}` | `Mark` | `ImDrawList` calls | `identity`, ImGui |

`identity_svg` **returns a string and opens no file**, following
`scenario.{hpp,cpp}`: the core does I/O only through `Recorder` (invariant 5).

`mark_draw` is the only piece needing a window, and the only one not unit
tested — the same split the viewer already makes.

**The iteration loop**, which is how the lexicons and palettes actually get
tuned:

```
./build/atc --identity-sheet output/marks.svg --identity-count 1000
```

## Part 8 — testing

| Test | Asserts |
|---|---|
| purity | same seed → identical `Mark` and `Name`, independent of call order |
| **non-perturbation** | reference months generating 1M identities vs none → byte-identical CSV |
| layout | `static_assert(std::is_trivially_copyable_v<Mark>)` |
| well-formedness | over 1M seeds: every enum in range, every palette index valid |
| **per-slot coverage** | every lexicon slot meets its minimum pool size, and no entry exceeds its share bound |
| contrast | no generated mark pairs two palette entries below the perceptual distance floor |
| legibility | at 16×16, ink coverage within [0.15, 0.85] |
| real-company | no generated name appears in the known-companies list |
| locale agreement | a firm's name locale always equals its region's locale |
| svg | golden strings for fixed seeds; **zero `<text>` elements in output** |

The non-perturbation test is the one with teeth: the others check the
generator works, that one checks it cannot reach into the simulation.

Coverage is two-sided on purpose. "A million distinct names" is satisfied by a
generator that also emits one name 40% of the time; the share bound is what
catches an unbalanced template.

## Part 9 — what the prototypes measured

Worth keeping, because it is why this design is shaped as it is.

| Finding | Number |
|---|---|
| First grammar's perceptually distinct marks | **78** (not the 10⁷ estimated) |
| Wide grammar, distinguishable at 16px / 32px | ~2,300 / ~5,800 |
| Free-form (rejected) illegible at 16px | 29.9% |

**Perceptual distinctness saturates far below parameter entropy.** The wide
grammar's parameter space is ~3.5M, but only ~5,800 marks are distinguishable
at panel size: small differences in size and thickness vanish when downscaled.
A million *unique* logos is therefore not achievable, and is not the goal —
marks repeat, roughly one duplicate pair per 100-firm screen, and the name
disambiguates. Colour is the cheapest entropy, because it is the one
difference that survives downscaling.

## Part 10 — scope and open risks

**Not in this spec:** sector or live-state coupling. A mark is *pure
identity*, fixed for the firm's life. Encoding size or distress would make it
a chart rather than a brand. Revisit when output classes exist, and then as a
shape *family*, never as live state.

**Locale verification is an open risk and must be stated, not implied.**
Turkish was reviewed by a native speaker and passes. Every other locale is
**unverified**: an earlier phonotactic generator was judged plausible by the
author and was wrong, and the author cannot evaluate output in languages he
does not read. The lexicon table must carry a `verified` flag per locale, and
unverified locales ship marked as such or do not ship. "Looks plausible to a
non-speaker" is a standard native speakers will find insulting.

**A wall of a thousand marks may still look samey.** The mitigation is the
contact sheet, early. If the population is too uniform the fix is wider
parameter ranges and more sanctioned palettes — not a second grammar, which
is what turns a coherent world into a clip-art set.
