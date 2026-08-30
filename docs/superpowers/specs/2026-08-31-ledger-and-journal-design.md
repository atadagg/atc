# The financial substrate: ledger, journal, and intervention log

Status: **Parts 1 and 3 are implemented and merged. Parts 2 and 4 are design.**

## Read this first, in plain language

This document is about money plumbing, not economics. Three ideas, stated
without jargon:

1. **Money only ever moves; it never appears.** Every movement takes from
   somewhere and gives to somewhere, and the two amounts are the same number.
   One function does this, so it cannot be got wrong.
2. **Every movement can be written down.** Who paid whom, how much, when, and
   why. That list is the book. It is optional, because writing down all of it
   is expensive.
3. **A run is defined by what went into it, not by where it ended up.** A seed
   plus a list of things the operator did. Replay the inputs, get the identical
   run back — which is what lets you scrub a live simulation backward, save it,
   or hand someone a scenario as a file.

None of that is specific to the model currently running. It is what a bank
core or an exchange matching engine has had for decades.

**What about banks creating money?** A bank that lends you 1000 does not break
its books. It writes two lines at once: *we are owed 1000* and *you hold 1000*.
The books still balance; the balance sheet just got bigger on both sides. So
the rule that survives a bank is not "the total never changes" — it is
**"every movement balances."** That distinction is the whole of Part 2.

The framing to avoid: this is not blockchain. Blockchain solves append-only
tamper-evidence *without a trusted operator*. There is a trusted operator here
— the process. No hashing, no Merkle trees, no immutability-everywhere.

---

## Part 0 — the engine and the model are not the same thing

The substrate documents were originally written against Lengnick v1, and
several of that model's properties leaked into them as if they were properties
of the machinery. They are not. Anything in the right column is temporary.

| Property | Belongs to |
|---|---|
| Every movement has a source and a sink and nets to zero | **the engine** |
| Balances are only ever written by one function | **the engine** |
| Same inputs produce a byte-identical run | **the engine** |
| The tick is data (days, months), not wall-clock | **the engine** |
| A run is (seed + intervention log) | **the engine** |
| **Total money is constant** | Lengnick v1 only |
| Only two kinds of account exist (firm, household) | Lengnick v1 only |
| No account may ever go negative | Lengnick v1 only |
| Money enters nowhere and leaves nowhere | Lengnick v1 only |

The rule for writing anything into this substrate: **state it at the level
that survives the next extension.** If a real bank core would not recognise
the rule as stated, it is stated at the wrong level.

---

## Part 1 — `transfer`, the single money chokepoint

**Implemented.** `src/ledger.{hpp,cpp}`.

### The problem it solved

Money moved as two independent mutations at three sites (`daily.cpp`,
`month_close.cpp` twice). Conservation held by convention and careful review;
it was *checked* afterwards by summing every balance, not *guaranteed*. Every
new module that moves money would add another site where the convention could
break, and the failure mode is a tiny drift that surfaces thousands of months
later with no indication of which function caused it.

### The design as built

```cpp
void transfer(World& w, Account from, Account to, double amount,
              Reason reason = Reason::Unspecified, Journal* journal = nullptr);
```

`Account` is a tagged index — `{Kind, uint32_t}` — following invariant 3
(indices between agents, never pointers). One subtraction and one addition of
the same `double`, in one place.

### What was learned building it

**The byte-identical acceptance test failed, and it was right to accept that.**
Paying N workers used to be one debit of the aggregate payroll followed by N
credits. True double-entry is N paired debit/credits. Those differ in the last
bits of a `double`, and the system is chaotic, so a ~3e-9 difference at month 0
becomes a visibly different CSV at month 6000. The economic results were
statistically unchanged (the comparison table is in CLAUDE.md). The baseline
was re-recorded.

**Generalise the lesson:** a mechanical refactor of floating-point arithmetic
is *not* required to be bit-identical, and demanding it can push you toward the
less correct structure. Require that the *distributions* are unchanged, and
that determinism still holds.

**A rounding remainder now exists where there was none.** `pay_wages` in the
unaffordable-payroll branch computes a per-worker share and pays it N times;
N shares need not sum to exactly what the firm held. The firm can be left
holding a machine-epsilon residue instead of exactly zero. **This matters for
Part 2** — see the risk section there.

---

## Part 2 — balanced events, issuers, and enforced solvency

**Design. Not implemented.**

### The rule

> **Invariant 1 — every event balances.** Every movement of money has a source
> and a sink and nets to exactly zero. `ledger::transfer` is the only code that
> writes a balance, so this is true by construction. Money is never created by
> a movement. It is created by *issuance*, and issuance is itself a balanced
> movement out of an issuer account.
>
> **This invariant has no exceptions.** Not for money injections, not for
> interventions, not for bank lending. If something appears to need one, it is
> modelled wrongly.

The old wording — "the sum of all balances is constant" — is demoted to a
*corollary that holds only while the model has no issuers*. Lengnick v1 has
none, so the number is still constant and every existing test still passes
unchanged. Nothing about today's behaviour moves.

### Two numbers, and only one of them is a rule

| Name | Meaning | Kind of thing |
|---|---|---|
| `books_balance()` | the sum over **all** accounts, issuers included | an **assertion** — never moves, ever |
| `money_in_circulation()` | the sum over **non-issuer** accounts only | a **measurement** — moves when money is issued or repaid |

Today these are the same number, which is why the change is free. Once a bank
exists, the second is the money supply and is an interesting recorded series;
the first stays flat forever and is the thing that fails loudly if the plumbing
is wrong.

`World::total_money()` becomes `money_in_circulation()`; `books_balance()` is
new. Keeping both names distinct matters — the bug this prevents is someone
later asserting that the *measurement* is constant.

### Issuers are a kind of account, not a permission

`Kind` grows to admit issuer kinds (`Bank` first; a government or mint later).
Whether an account may go negative is a property **of its kind**, decided by
one function:

```cpp
constexpr bool may_issue(Kind k);   // true only for issuer kinds
```

Not a per-account boolean. "Which accounts can create money" must be
answerable by reading one function, not by auditing a thousand flags. An
issuer is a *type of thing*, not a permission that can be granted to an
ordinary household by a stray line of code.

An issuer's negative balance is not a deficit or an error. **It is the money
it has issued**, and it is exactly the right number for that.

### `transfer` enforces solvency

Four rejections, all of them caller bugs of the same class:

| Condition | Why |
|---|---|
| amount is not finite | a NaN would silently poison every downstream balance |
| amount is negative | a negative transfer is a bug, not a reversed direction |
| account index out of range | no such agent |
| **would push a non-issuer below zero** | **new** — an ordinary account cannot create money |

This is the "seriousness of a bank" clause. It converts an impossible state
from *unwritten* into *unrepresentable*.

**It also dissolves a problem the earlier draft of this spec had.** That draft
said some interventions — "give every household 100" — break invariant 1 on
purpose, so the conservation check would have to consult the intervention log
for exceptions. It does not. An injection is a transfer out of an issuer; it
balances like everything else; there is no carve-out and no special case
anywhere. If a future feature seems to need one, that is the signal it is
missing an issuer account, not that the invariant needs loosening.

### The risk that gates this, and it must be measured

Enforcement cannot be switched on until one question is answered with a
measurement, not an argument:

> **Does any existing caller already push a balance below zero by a rounding
> remainder?**

Two known suspects, both pre-existing:

1. **`month_close::pay_wages`** — the residue described in Part 1. If N shares
   sum to marginally *more* than the firm held, the last transfer overdraws by
   ~1e-16 and enforcement would reject a movement that happens every month in
   a perfectly healthy run.
2. **`daily::shop`** — its cost clamp (`std::min(bought * f.price, h.liquidity)`)
   exists precisely to stop floating-point division pushing a household
   slightly negative. That clamp is evidence the problem is real, and it is
   currently the only thing preventing it.

**The fix, named in advance so it is not improvised:** make the *final*
recipient in a distribution take the exact remaining balance rather than a
separately computed share. That is both what removes the overdraft and what
removes the residue — one change, two problems.

**Measured, 2026-08-31 — the risk is real.** Temporary instrumentation on
`transfer` over the reference run (seed 1, burn-in 1000, 6000 months):

| Quantity | Measured |
|---|---|
| Writes that left a balance below zero | **16,953** (~2.4 per month) |
| Worst balance seen | **−1.435e-12** |
| On households | **0** — `daily::shop`'s clamp holds |
| On firms, via wage payments | 5,838 |
| On firms, via profit distribution | 11,115 |

So solvency enforcement, added naively, would reject ~17,000 movements per
reference run — every one of them a rounding crumb rather than real money.
**The fix is a prerequisite, not a follow-up.**

Note *why* profit is the larger half. A firm with no employees has a zero wage
bill, so its retained buffer (`buffer_factor * wage_bill`) is also zero, so it
distributes its entire balance — and a full payout is exactly the case where N
computed shares can sum to marginally more than the pot. Roughly 16% of firms
are empty in any given month.

Both sites take the same fix: **the last recipient in a distribution takes the
exact remaining balance rather than a separately computed share.** This also
removes the machine-epsilon residue described in Part 1 — one change, three
problems.

The instrumentation was reverted and is not in the tree; re-derive it from this
table rather than trusting it, if it matters again.

### Testing

- `books_balance()` is unchanged by every operation, including issuance.
- An issuer may be driven negative; a non-issuer may not, and the attempt
  throws rather than succeeding quietly.
- `money_in_circulation()` rises by exactly the amount issued.
- The reference run's economic results are unchanged (distributions, not bits).
- Determinism still holds: same inputs, byte-identical repeat.

---

## Part 3 — the journal

**Implemented.** `ledger::Journal` / `Entry` / `Reason`, with `VectorJournal`
in `ledger` and `CsvJournal` in `recorder`.

Every movement can optionally be written down: `{month, day, seq, from, to,
amount, reason}`, recorded *after* the movement succeeds, so a rejected
transfer leaves no trace. `seq` is monotonic across the whole run and gives
every entry a total order — without it, two movements in the same month have
no defined order and determinism has been reintroduced at a finer grain.

The scheduler stamps the clock (`Journal::set_time`); `transfer` does not know
what day it is and should not. `Scheduler::set_journal` attaches and detaches
between months.

`Journal` is deliberately **not** part of `World`, and therefore not part of
`SimSnapshot`: a journal is an output, and the core's outputs go through a sink
it does not own (invariant 5).

### What the book immediately revealed

One journaled month of the reference population: **122,174 movements, of which
100,000 were profit distributions.** `distribute_profits` pays every firm's
profit to every household, so it is O(firms x households) — 100 x 1000 — against
~21,000 purchases and ~1,000 wage payments.

That is a finding, not a curiosity: **~82% of all money movement in this
economy is the ownership proxy, not trade.** It is also why journaling defaults
to a one-month window and never covers the burn-in — the full reference run
would be some 855 million entries.

---

## Part 4 — the intervention log

**Design. Not implemented.**

### The problem

A run is currently reproducible from its seed *because the seed is the only
input* — every parameter was compiled in. The moment a slider changes a
parameter mid-run, that stops being true, which is why live steering is
unimplemented.

For the viewer the failure is sharper than "not reproducible". Scrubbing
backward restores the nearest snapshot and re-simulates forward. If a parameter
changed at month 300 and the user scrubs to 250 and forward again, the
re-simulation must reapply that change at exactly month 300 — otherwise
scrubbing back and forward silently yields a different history than the one
just watched. That destroys the property the whole viewer rests on.

### The design

```cpp
// `Action` rather than `Kind`: `ledger::Kind` already means "what sort of
// account this is", and two different Kinds in one substrate is a name
// collision waiting to be misread.
struct Intervention {
    int      month;    // when it takes effect
    uint32_t seq;      // tie-break within a month; gives a total order
    Action   action;   // SetParam, Issue, ForceFire, ...
    Payload  payload;
};
```

A run is then defined by **(seed, intervention log)**. `Scheduler::run_month`
consults the log at month open and applies everything due, in `seq` order,
before any agent acts. Replay is exact again, and `seek` needs no special
handling: the log reads the same on the way back as on the way forward.

### Details that are easy to get wrong

1. **`Params` must live in the snapshot.** A snapshot at month 400 must carry
   the parameters *as of* month 400, or restoring it replays interventions on
   top of a state that already absorbed them. **Already satisfied** — `Params`
   lives on `World` and `SimSnapshot` copies the whole `World`, so this is
   structural rather than something to remember. `Params` must stay a plain
   aggregate for the same reason `SimSnapshot` must stay a plain assignment.

2. **`seq` is load-bearing.** Two changes in the same month need a defined
   order, or nondeterminism has been reintroduced at a finer grain than the one
   that was fixed.

3. **~~Some interventions break invariant 1 on purpose.~~** **Withdrawn.** This
   was true only of the old fixed-money-stock framing. Under Part 2 an
   injection is a transfer out of an issuer account and balances like anything
   else. If a proposed intervention appears to need an exception, the model is
   missing an issuer.

### The prerequisite, now done

Parameters had to stop being compile-time constants. `params.hpp` is now split:

| Pile | Form | Why |
|---|---|---|
| Structural | `constexpr` | defines memory layout — `kShopsPerHousehold` is the size of an array inside every `Household` |
| Initial conditions | `constexpr` | consumed once at construction; "change it at month 300" is meaningless |
| Run configuration | `constexpr` | CLI defaults, not model parameters |
| **Behavioural** | **`struct Params` on `World`** | every rate, threshold and probability consulted while running |

The test for the boundary: **could a UI slider move it mid-run?**

One behavioural parameter needed a guard: `max_shop_visits_per_day` is a loop
bound over a fixed-size array of shop slots, so it is clamped to
`kShopsPerHousehold`. A raised value must not visit shops a household does not
have. At the calibrated value the clamp is a no-op.

### What one mechanism unlocks

- live steering from the viewer without breaking determinism
- save games: seed + log is kilobytes, not a state dump
- replays, and a readable trail of how an economy reached its state
- scenario tests as data ("run 200 months, cut the wage floor, assert
  unemployment rises") rather than as bespoke test harnesses
- lockstep networking, if multiplayer is ever wanted — ship inputs, not state

The general name is event sourcing: state is a fold over an append-only log,
and any point in history is reconstructible. `SimSnapshot` is already the cache
of that fold; the log is the half that is missing.

---

## Part 5 — performance

The substrate sits on the hottest path in the program, so its cost is a
requirement, not an afterthought.

### Measured baseline

| Quantity | Measured |
|---|---|
| Reference run (7000 months, seed 1) | **14.3 s** (14.14 / 14.48 / 14.32) |
| Money movements per month | **122,174** |
| ⇒ transfers per reference run | **~855 million** |
| Of which: profit distribution | **~82%** |

At that volume a per-transfer branch is not free by assumption, which is
exactly why Part 2's solvency check needs a number rather than a shrug.

### Budget

- **Solvency enforcement must be measured on the reference run before and
  after.** Three runs each, same machine. Over ~2% wall-clock needs
  justification or a cheaper formulation.
- **Journaling stays opt-in and defaults to a one-month window.** Recording
  855M entries is not a mode anyone wants by accident.
- **The intervention log is consulted once per month**, not per day and not per
  transfer. Its cost is structurally negligible; keep it that way.
- **`books_balance()` is O(accounts) and must not be called per transfer.** It
  is a test-time and month-close check.

### If transfer cost becomes a problem

The obvious target is the profit distribution — 82% of all movements, and
O(firms x households) by construction. It could be restructured to move one
aggregate per firm rather than one transfer per household pair.

**But do not do that on the strength of the count alone.** The count says it is
82% of *transfers*; it does not say it is 82% of *runtime*, and this project has
been wrong twice by trusting a plausible aggregate over a measurement (see
CLAUDE.md's averaging-artifact and rewiring-bug notes). Profile first. Note
also that any such restructuring re-opens the rounding-remainder question in
Part 2 — the two are the same piece of arithmetic.

---

## Order of work

1. ~~`transfer` chokepoint~~ — **done**
2. ~~Optional journaling behind `Recorder`~~ — **done**
3. ~~Split `params.hpp` into structural and behavioural~~ — **done**
4. **Part 2**: measure the rounding remainder; fix the distribution tail;
   `Kind` gains issuer kinds; `may_issue`; solvency enforcement in `transfer`;
   `books_balance()` / `money_in_circulation()` split. Measure the cost.
5. **Part 4**: `Intervention`, the log, and application at month open.
6. A scrub-across-an-intervention test (the `Params` half is already done).
7. Viewer steering UI — the first consumer, and the reason the rest exists.

Step 4 is a prerequisite for any bank extension and independent of steps 5–7.
