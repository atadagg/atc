# Ledger and journal: the transfer chokepoint and the intervention log

Status: **design, not yet implemented.** Promoted to the top of the next-work
order (see CLAUDE.md) ahead of the spec §8 extensions.

## Why these two, and why first

The v1 core already has three of the four properties a financial engine needs.
This document specifies the fourth, and hardens the first.

| Property | State |
|---|---|
| Double-entry ledger — every movement has a source and a sink, and the sum is invariant | **Partial.** Invariant 1 holds, but only balances exist. No entries are recorded, so conservation is verified empirically after the fact rather than being true by construction. |
| Deterministic replay — same inputs, same history, byte-for-byte | Invariant 2. Holds, including the hand-rolled RNG conversions. |
| Explicit time — the tick is data, not wall-clock | `Scheduler::run_month`, day/month phases. |
| Input journal — the run is defined by its inputs, not by its state | **Missing.** This is the gap. |

Neither piece is an economics change. Both are substrate, both are small, and
everything downstream — steering, save games, a stock market, an ERP layer,
firm-to-firm trade — is easier on top of them than beside them. That is the
argument for doing them before extension 1, despite the dependency order.

The framing to avoid: this is not blockchain. Blockchain solves append-only
tamper-evidence *without a trusted operator*. There is a trusted operator —
the process. What is wanted here is what exchange matching engines and bank
cores have had for decades: a journal of inputs plus deterministic replay.
No hashing, no Merkle trees, no immutability-everywhere.

---

## Part 1 — `transfer` as the single money chokepoint

### The problem

Money moves today as two independent mutations. All the sites:

| Site | Movement |
|---|---|
| `daily.cpp:55-56` | household → firm (a purchase) |
| `month_close.cpp:41-42` | firm → employees (wages) |
| `month_close.cpp:63,68,71` | firm → households (profits, both branches) |

Conservation holds by convention and careful review. It is *checked* by
`World::total_money()` afterwards, not *guaranteed*. Every new module that
moves money — a stock market, a bank, firm-to-firm trade — adds sites where
the convention can be broken, and the failure mode is a small drift that
surfaces thousands of months later with no indication of which function
caused it. CLAUDE.md's invariant 1 already lists the three likeliest culprits
by name, which is exactly the diagnostic burden this removes.

### The design

One function, and it is the only code permitted to write `liquidity`:

```cpp
// The only place money moves. Debits `from`, credits `to`, by construction.
void transfer(Account from, Account to, double amount);
```

`Account` is a tagged index — `{Kind::Firm | Kind::Household, uint32_t}` —
following invariant 3. Rules:

- `amount` must be finite and non-negative; a negative transfer is a caller
  bug, not a reversed direction.
- Overdraft is a policy question the *caller* answers, not `transfer`. Today
  no caller may overdraw; `month_close::pay_wages` already handles the
  unaffordable-payroll case by cutting the posted wage (the no-bankruptcy
  rule). Credit (extension 7) will introduce accounts that legitimately go
  negative, which is a change to the account type, not to `transfer`.
- Conservation becomes a tautology: one subtraction and one addition of the
  same `double`, in one place. The existing total-money test stays, now as a
  regression guard on the callers rather than the arithmetic.

Note the machine-epsilon caveat already recorded in CLAUDE.md's deferred
items (`daily.cpp`'s cost clamp does not reduce `bought`). `transfer` does
not fix that — it is a goods-side discrepancy, not a money-side one — but it
does mean the money side stops being a place the problem could hide.

### What it buys, beyond conservation

If `transfer` optionally appends an entry — `{month, day, seq, from, to,
amount, Reason}` — the model gains a real ledger, and with it:

- per-agent transaction history ("where did this household's money come from")
- flow-of-funds between sectors, for free, as a recorded series
- the audit trail that makes a stock market or an ERP layer legible instead
  of guesswork
- a demand-leakage measurement that does not depend on inferring flows from
  balance differences — directly relevant to the open criterion 1

Journaling must be **opt-in**. The reference run is ~147k days over 6000
months; recording every entry unconditionally would dominate both runtime and
memory. Default off, enabled by the viewer and by targeted analysis runs.
`Recorder` is the natural place for the sink (invariant 5: the core does I/O
only through `Recorder`), which also keeps the entry stream out of `World`
and therefore out of `SimSnapshot`.

### Migration

Mechanical, and the three call sites above are the whole of it. Do it as one
commit with no behaviour change: the total-money test and the recorded CSV
must both be byte-identical before and after. If they are not, the refactor
found a real bug — that is a result, not a setback.

---

## Part 2 — the intervention log

### The problem

Invariant 2 says seed → byte-identical run. That holds today because the seed
is the *only* input: `params.hpp` is `constexpr`, so every parameter is
compiled in. This is why steering is currently unimplemented (CLAUDE.md,
Viewer section) — the moment a slider changes a parameter mid-run, the run is
no longer reproducible from its seed.

For the viewer the failure is sharper than "not reproducible". `Sim::seek`
scrubs backward by restoring the nearest snapshot and *re-simulating forward*
(`sim.hpp`, `kSnapshotInterval = 50`). If a parameter changed at month 300
and the user scrubs to 250 and forward again, the re-simulation must reapply
that change at exactly month 300. Otherwise scrubbing back and forward yields
a different history than the one just watched — silently. That destroys the
property the whole viewer rests on.

### The design

Move parameters from `constexpr` into a runtime `Params` struct, and make
every change to it go through an append-only log:

```cpp
struct Intervention {
    int      month;    // when it takes effect
    uint32_t seq;      // tie-break within a month; gives a total order
    Kind     kind;     // SetParam, InjectMoney, ForceFire, ...
    Payload  payload;
};
```

A run is then defined by **(seed, intervention log)** rather than by the seed
alone. `Scheduler::run_month` consults the log at month open and applies
everything due, in `seq` order, before any agent acts. Replay is exact again;
`seek` needs no special handling, because the log reads the same on the way
back as on the way forward.

### Three details that are easy to get wrong

1. **`Params` must live in the snapshot.** A snapshot at month 400 has to
   carry the parameter values *as of* month 400, or restoring it and
   replaying re-applies interventions 1–7 on top of a state that already
   absorbed them. `SimSnapshot` gains a `Params` member; the log is then
   consulted only for months strictly after the snapshot's. Note the existing
   comment on `SimSnapshot` — it warns that a non-copyable member is the line
   that stops compiling. `Params` must stay a plain aggregate.

2. **`seq` is load-bearing.** Two changes in the same month need a defined
   order, or nondeterminism has been reintroduced at a finer grain than the
   one that was fixed.

3. **Some interventions break invariant 1 on purpose.** "Give every household
   100" is a money injection, not a bug. Invariant 1 becomes *money is
   constant except across logged injections*, and the assert must consult the
   log. Decide this once, deliberately; discovering it later as unexplained
   drift is the expensive path.

### Cost of the `constexpr` removal

Not free, and worth stating plainly. `params::kNumHouseholds` and friends are
currently compile-time constants used as array bounds — `Household::shops` is
`std::array<uint32_t, kShopsPerHousehold>`. Structural parameters (population
sizes, `kShopsPerHousehold`, `kDaysPerMonth`) should **stay** `constexpr`;
only *behavioural* parameters (thresholds, probabilities, rates — the
Lengnick Table 1 values and the invented ones alike) move to `Params`. That
split maps onto the boundary `params.hpp` already documents between published
calibration and invented initial conditions, though it is not the same cut,
and drawing it is the first implementation task.

### What one mechanism unlocks

- live steering from the viewer without breaking invariant 2
- save games: seed + log is kilobytes, not a state dump
- replays, and a readable audit trail of how a given economy got where it is
- scenario tests as data ("run 200 months, cut `kMinWage`, assert
  unemployment rises") rather than as bespoke test harnesses
- lockstep networking, if multiplayer is ever wanted — ship inputs, not state

The general name is event sourcing: state is a fold over an append-only log,
and any point in history is reconstructible. `SimSnapshot` is already the
cache of that fold; the log is the half that is missing.

---

## Order of work

1. `transfer` chokepoint, no journaling, no behaviour change. Byte-identical
   output is the acceptance test.
2. Optional journaling behind `Recorder`.
3. Split `params.hpp` into structural (`constexpr`) and behavioural
   (`Params`).
4. `Intervention`, the log, and scheduler application at month open.
5. `Params` into `SimSnapshot`; a scrub-across-an-intervention test.
6. Viewer steering UI — the first consumer, and the reason the rest exists.

Steps 1 and 3 are independent and can proceed in either order.
