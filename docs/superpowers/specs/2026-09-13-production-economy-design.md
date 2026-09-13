# Piece 1: the production economy

Implementation reference, not a plan document. Decisions only; the code is
the description. Pieces 2 (ownership, shares, valuation, acquisition) and 3
(innovation, player seat, SAP terminal, segments, chronicle) are out of scope.

## What changes

A firm makes one good out of other goods. Today it makes the only good out of
labour. Everything below follows from that.

## Goods

`World` gains `std::vector<Good>`. Fixed-capacity members only -- `Good` and
`Firm` stay trivially copyable, because `SimSnapshot` is a plain assignment
(invariants 3 and 4).

    struct Good {
        std::array<uint32_t, kMaxInputs> inputs;    // good indices, NO_GOOD padded
        std::array<double,   kMaxInputs> input_qty; // per unit of output
        double   labour_per_unit;
        bool     consumer;
        uint8_t  remove;          // computed; see below
        double   commoditization; // computed; see below
    };

The graph is a DAG. Assert acyclicity whenever it changes (nothing changes it
in this piece; innovation does, in piece 3).

**Exactly one consumer good in this piece.** The chain grows upstream only.
Multiple consumer goods means households choosing between categories -- a
design of its own, deferred to piece 3. Household code stays nearly as-is.

## Remove, and commoditization

`remove` = longest path from this good to a consumer good. Consumer goods are
0. Ore feeding a 4-deep chain is 3.

Counted from the consumer end, not the raw end, for two reasons: it is the
causal story (distance from the consumer is the distance over which a
producer's choices stop being visible), and it makes today's single good
remove 0 -> commoditization 0 -> current behaviour, with no exception carved.

`commoditization` in [0,1], rising with `remove`. Exact curve is a behavioural
`Params` field (a slider could move it), default `1 - exp(-k * remove)`.

It does three things and nothing else:

1. **Price.** A seller's price is pulled toward the good's reference price,
   weighted by commoditization. At 1 the firm is a price taker; at 0 the
   existing gated price rule runs unchanged (see CLAUDE.md -- gated, not
   clamped).
2. **Spread.** How many alternative sellers a buyer compares.
3. **Stickiness.** How readily a buyer abandons a supplier.

**Reference price** per good = volume-weighted mean of last month's actual
trades, published at month open. A measurement, not a clearing price: no
auctioneer, no guarantee an order fills, free to spike and crash. Published
per market; there is exactly one market (countries are deferred, and adding
them later means more markets, not a rewrite).

## The unified buyer

Generalise `daily::shop` and the `network` rewiring so the buyer is a
household or a firm. Same random visit order, same "record what was *wanted*,
not bought" (CLAUDE.md), same telescoping across retries, same rewire on price
or on being rationed.

Keep the two rewiring draws distinct -- size-weighted for the price-comparison
path, unweighted for the rationed path. CLAUDE.md: unifying those once
concentrated two thirds of the economy into dead firms. This generalisation is
a different move and must not become that one.

A firm holds supplier links per input good, fixed capacity.

## Production

Leontief, no substitution:

    output = min(labour capacity, min over inputs of stock_i / qty_i)

Short of iron, extra fibre does not help. This is what makes a stockout
travel up the chain.

## Balance sheet

Assets: cash (today's `liquidity`), input stock, output stock, receivables.
Liabilities: payables. **Equity is computed, never stored** -- the same
discipline as `books_balance()`, so it cannot silently drift.

Inventory valued at cost. `ledger::transfer` remains the only writer of cash
(invariant 1); stock and payables are firm state, not accounts.

Equipment and depreciation are deferred -- nothing here needs them, and they
belong with innovation, when there is a reason to want better machines.

## Trade credit

One month. Inputs are bought on credit through the month; all payables settle
at month close. No per-invoice tracking -- same behaviour, far less machinery.
Banks compete with this later; trade credit ships first because it is nearly
free and it is what delivers contagion.

`month_close::run` must now take an `Rng&` -- settlement order has to be
randomised or low-indexed firms are paid first forever. CLAUDE.md lists this
as a deferred item; it comes due here.

## Birth and death

**Death:** equity < 0 at month close. Employees fired, payables defaulted.
Creditors write off what they are owed, and a write-off can push them under.
That cascade is the point of the piece.

Note the interaction with the existing rule that firms cut wages rather than
go bankrupt (bounded by `kMaxEmergencyWageCut`, `kMinWage`). That rule stays;
insolvency now bounds it from the other side.

**Birth:** a firm is founded when a good's reference price exceeds its cost of
production by a margin and producers are scarce.

**Placeholder, named as one:** both ends need an owner, and owners arrive in
piece 2. Until then a dead firm's residual cash leaves circulation through the
issuer account and a new firm's starting cash enters the same way. Symmetric
and conservation-safe. Piece 2 replaces both ends with shareholders.

## Tests

The gate, before any new behaviour is tuned:

- **One good, no inputs, commoditization 0 reproduces today's runs
  byte-for-byte.** Identical, not close. If it fails, the generalisation
  changed something it should not have.

Then:

- Books balance with trade credit outstanding (invariant 1).
- Same seed, byte-identical run (invariant 2).
- A forced supplier failure cascades to a solvent-but-exposed customer.
- A stockout upstream reduces output downstream, with the expected lag.
- Per-firm distributions checked, not means (CLAUDE.md), across an ensemble.
  One seed is not a sample.

## Deferred, recorded so it is not lost

- **Escaping commoditization.** A firm deliberately differentiating a
  commodity good -- branding, quality grades, guaranteed supply -- to drag its
  own product down the scale and stop being a price taker. One of the best
  verbs in business; wants its own piece.
- Supply contracts (volume, term, breach, renegotiation) as a layer over spot
  buying.
- Multiple consumer goods.
- Equipment and depreciation.
- Countries as multiple markets.
