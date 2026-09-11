# The merchant: rung 1 of the institutional ladder

*Design agreed in conversation, 2026-09-11. Not yet a full spec — the
mechanism below is approved in shape; the numbered open items at the end are
not settled. No code has been written.*

## What changed about the project's direction

This started as "add a bank to fix criterion 1" and ended somewhere else. Three
corrections, all from the user, all of which stand:

1. **A bank is a firm with special abilities, not a new agent type.** Invariant
   4 already says variation arrives as fields and enums, never subclasses. The
   original design invented a parallel agent type and was wrong to.
2. **We are not adding banks to fix things.** Designing toward criterion 1 is
   the mistake this project has already made twice — the rewiring bug made
   criteria pass for the wrong reason. The honest success criterion is
   *attribution*: does the institution behave like the thing it models, and can
   we trace what it causes. Stability, if it arrives, is a finding.
3. **You cannot define a bank without its history.** A modern credit bank has a
   thousand things holding it up. So we build the sequence in order, each rung
   its own spec, plan and measurement:

   > merchant → warehouse receipts → receipts circulating as money →
   > lending more receipts than goods held → interest → capital → growth

   Each rung is allowed to change what the model *is*. CLAUDE.md's
   baseline-preservation habit becomes a per-rung control, not a permanent
   constraint.

## Why a merchant and not a warehouse

The first idea was a grain bank: goods spoil, a warehouse spoils them less, the
fee is paid in kind. The user pushed back with cotton — which barely spoils, and
was warehoused at enormous scale anyway. The real reasons cotton was warehoused
are timing (harvested at once, consumed all year), the receipt being more liquid
than the bale, grading, and location.

Two of those already exist in our model. A firm sells only to the ~7 households
wired to it, so goods sit where the buyers aren't. **Measured, seed 1, 6000
months, with throwaway instrumentation in `daily::shop` (since reverted):**

| Quantity | Measured |
|---|---|
| household-days ending short of goods | 27,256 of ~126,000,000 (**0.02%**) |
| inventory at the household's own 7 shops, then | **0.000 units — cleaned out, every time** |
| inventory at firms it is not wired to, then | **7,381 units** |
| unconnected firms holding >1 unit, then | **38.8** |
| total unmet demand over the whole run | 66,843 units, against ~63,000 consumed *per month* |

So the misallocation is **textbook in shape and negligible in size**. The goods
exist, in quantity, at dozens of sellers, and the buyer has money and no way to
reach them — but it happens rarely, and five centuries of it adds up to about
one month's groceries. The cause is the one already in CLAUDE.md: the economy
sits at capacity (planned consumption 62,972 vs output 62,794), so there is no
surplus to store. Shop rewiring is also already a slow market-maker.

**A warehouse therefore has no job, because storage needs a surplus.** A
*merchant* does not — it **buys** the goods. That is the service: the producer
sells its whole output now, in one transaction, instead of waiting for
households to trickle in. It is also the historically correct object, because
merchants are where bankers come from. The cotton factor who buys your crop
before you have sold it is already advancing you money, and that advance is the
first loan.

## The design

A `Firm` with `Role::Merchant` — a field, not a subclass. It reuses liquidity,
employees, wage, price and inventory. Three differences, all role checks:

- **It does not produce.** Skipped in `daily::produce`; inventory arrives by
  purchase.
- **It buys.** Each month it offers to buy from the producers it is linked to,
  paying cash on the spot at a bid price.
- **Its marginal cost is what it paid**, not wage ÷ productivity, so
  `marginal_cost` gets a role branch. Everything downstream — the price band,
  the inventory band, hire/fire — works unchanged.

It sells to households through ordinary shopping: a merchant can occupy one of a
household's seven shop slots and is reached by the existing rewiring. It hires
and fires on the existing demand rule, so **staff are its capacity** — market-
making becomes a job that must be paid for, which is extension 5 arriving early
and for free.

**What it does not have:** no receipts, no credit, no advances, no bankruptcy.
It pays cash on delivery. If it cannot make payroll it cuts wages like any other
firm. Rung 1 touches no invariant and needs no issuer — every movement is an
ordinary `transfer`, with one new `Reason::Wholesale`.

Paying *before* delivery is rung 2, and that is where credit starts.

**Off by default.** Merchant count is a parameter, default zero, so the
6000-month reference run stays byte-identical and every claim is a treatment
measured against the same seeds without it — the discipline the neutrality
experiment used.

## Predictions, recorded before building

Written down now so a story cannot be retrofitted afterwards.

1. **The macro numbers do not move.** Unemployment, prices, Gini, all four
   criteria — within seed noise. The misallocation a merchant fixes is 0.02% of
   household-days. **If they do move, that is a bug hunt, not a result.**
2. **Merchants concentrate.** Those wired to many households cover their wage
   bill and grow; those that are not drift to the wage floor. Firm-size
   dispersion widens among merchants specifically.
3. **Producers linked to a merchant hold less, and less variable, inventory.**
   That is the service. If it does not appear, the merchant is doing nothing.

## Open items — not settled, settle before writing the plan

1. **The bid price.** Retail minus a fixed margin is the obvious first cut, but
   nothing has been decided, and the merchant's price rule needs a real answer:
   `adjust_price`'s band is built on `marginal_cost`, which for a merchant is an
   average purchase cost that changes as it buys.
2. **Which producers a merchant is linked to**, and whether those links rewire.
   A new firm→firm link set, or reuse of the shop-link machinery.
3. **How much a producer sells** — everything above its own inventory band, or a
   fraction.
4. **How many merchants**, and whether they are drawn from the existing 100
   firms or added to them (the latter changes `kNumFirms`'s meaning).
5. **Merchant capacity from headcount** — the throughput-per-worker constant has
   no value chosen.
6. **`month_close::run` takes no `Rng&`** and the merchant's buying probably
   needs one. Already a known deferred item in CLAUDE.md.

## Process state

The brainstorming skill's architectural path is at step 5 (design sections
presented and approved in shape). Not done: the remaining design sections, the
spec self-review, the user's review of the written spec, and `writing-plans`.
