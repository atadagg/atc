# Working Title: Shadow Wizard Money Gang (placeholder)

## Pitch
Ever wondered how capitalism would work in a world with magic?

An open-ended tycoon / grand-strategy hybrid — Victoria 2's systemic depth
married to a business sim. No win condition. You write your own story;
the game just tracks what you did.

## Theme & Setting
- Magic is a metaphor for revolutionary technology.
- Magic is scarce because *access* to it is scarce — education, capital,
  guild licensing — same as real-world deep tech. Not a chosen-one myth,
  not a species trait. This one line replaces most worldbuilding.
- Time advances in discrete steps (e.g. quarters). The world evolves with
  or without the player: ambient magic progresses, rivals research,
  segments shift.

## Player Role
A wizard-founder who owns a holding company with subsidiary companies
across sectors. Early game: personally hands-on (you pick the product,
you run sales). Late game: delegate via hired executives to scale.

### Attention (the scaling resource)
- Attention is scarce. Early on you personally run one company's Product
  and Sales dials.
- Hiring an executive (Head of Product, Head of Sales, COO) delegates a
  dial-cluster to an NPC with skill/trait stats. You set a target/strategic
  lean; they execute with variance based on competence.
- Delegating frees attention to found more companies, or spend on
  research/politics instead.
- Tradeoff: hands-on = tighter control, better execution, capped scale.
  Delegated = more reach, more risk (bad hires tank segments; disgruntled
  executives can leak research or defect to rivals — feeds the news feed
  and the politics layer).
- Sales stays the deepest hands-on layer even after delegation exists —
  it's the star mechanic, not something to abstract away.

## Companies
- **Supply chain is the game.** Firms make goods out of other goods. A
  good's distance from the consumer sets how commodity-like it is: iron
  ore has something resembling a central market and one visible price,
  a house does not. That is a sliding scale, not two systems — there is
  never an auctioneer, only a published average of what actually traded.
  The graph grows as Innovations are researched, so today's exotic
  product becomes tomorrow's commodity input, and two runs diverge.
  See `docs/superpowers/specs/2026-09-13-production-economy-design.md`.
- Product development = pick which researched Innovation a product line
  embodies, plus a quality/investment dial. Not an engineering minigame.
- Operational dials per company: pricing, which segments to target,
  marketing spend, wage/labor policy, expand vs. hold.
- Ownership is via equity/shares. Player holds stock in own companies
  (and potentially others').
- Acquisition: simple "Buy Company" action. Valuation calculated
  real-world-style — revenue × multiplier + capital. Acquiring a company
  inherits its researched tech/innovations. This is the (hard-won)
  counterweight to losing an invention race to a rival.

## Segments (the population)
Demographic cohorts, not individuals — occupation × class × age ×
political lean × region. Each segment is simultaneously:
- Your labor pool
- Your customer base
- The electorate

Every invention/product you apply reshuffles who's employed, who's
wealthy, who's angry. This is the emotional core of the "customers are
simulated humans" pitch.

## Research (Victoria 2-style, two layers)
- **Technology**: deterministic investment across branches (Industrial,
  Medical, Information/Scrying, Martial, Forbidden), fed by an
  Academy/education stat.
- **Innovations**: the actual spells/products. Unlock via a weighted
  roll once a technology threshold is crossed, influenced by world
  conditions (segment desperation, current events, rival breakthroughs).
- This means two playthroughs with identical investment diverge —
  and rivals can beat the player to their own invention. Missing an
  invention window should sting; acquisition is the recovery path, not
  a guaranteed one.

## Competitors
Symmetric AI wizard-founders running the identical system — same
companies, same research, same dials — differentiated only by strategy
and specialization. No special-cased AI logic; one system serves both
player and rivals, which keeps balance and scope sane.

## Politics & Regulation (folded into one tree, not a separate game)
- "Forbidden magic" is simply the top of the tech tree. Researching it
  triggers events — e.g. a demand to install surveillance charms in
  your products — that hit standing meters.
- Meters: Public Goodwill, Ministry/Government Standing, Council
  Relationship.
- The public can't stop you directly (same as real capitalism). The
  government, rivals, and the council can: spell approval can be
  denied, monopolies risk antitrust breakup, bad standing risks
  bankruptcy.

## Win/Loss
No win condition — open-ended, sandbox, legacy-driven (Rimworld/CK
style: the story is whatever happened). Optional milestones the game
can notice and log: bankrupting every competitor, "owning" the shadow
council.

Loss conditions: bankruptcy, forced breakup, being crushed by a rival
or the council.

## UI
Not a dashboard-of-tabs, not an explorable room (room UI adds spatial-
mapping friction with no mechanical payoff — see Gear City).
Instead: an **ERP terminal** — SAP as the reference point, which fits
because ERP is literally the software you run a firm's operations and
books on. A single diegetic frame is allowed and wanted: a wizard tower
room with a scrying terminal you click once to go full-screen. That is
a frame, not navigation — one click deep, then the real UI — which is
why it does not reintroduce the Gear City problem.

Behind that frame: a clean, chart/ledger-forward home screen — your financial
situation and performance charts, Stonks-9800-style — with the map,
research tree, segments view, and chronicle/news feed as switchable
panels.

The **chronicle/news feed** carries the real narrative weight: with no
map-drama or cutscenes, it's how an abstracted number-sim reads as a
living world (e.g. "Voss Tower's desperate textile workers accelerate
research into Automaton Looms — they beat you to it by two quarters").
Its writing/tone deserves real craft.

## Hiring / Delegation — approach
Hiring should stay simple and gradual, not a deep staff-management
minigame — reference point is Stonks 9800's approach (simple, low
friction). The player is a wizard first, not an HR manager; staff
exist to let attention scale, not to become their own system with
depth. Details of the exact mechanic TBD (to be worked out later,
modeled closely on Stonks 9800's version).

## Sales — approach
Core sales verb is segment targeting, run through a lean-methodology
lens: quickly test a product/pitch against a segment, read the
result, iterate. Mirrors real startup practice (build-measure-learn)
rather than a traditional marketing-mix simulation. Exact mechanic
(what a "test" looks like, what signal it returns, cost/speed of
iterating) TBD.

## Player Seats — how the game connects to the simulation

The simulation (ATC, this repo) is the world. The game is not a separate
program that reads its output; it is a set of **seats** inside it.

**Any agent can be promoted from rule-driven to player-driven.** Firms,
households and (once credit exists) banks all run on rules by default. Taking
one over means its decisions come from a person instead of from
`firm_decisions` / `household_decisions`; every other agent keeps running as an
NPC around it. That is one mechanic, not one per sector — and it is what makes
"play as a firm" and "play as a bank" the same feature.

**The API for a seat already exists: the intervention log.** A run is
`(seed + intervention log)`, applied at month open before any agent acts. A
player action is an intervention. Consequences that follow:

- A played session **is** a scenario file — save games are free, and readable.
- The viewer's byte-exact backward scrub means you can rewind a decision and
  replay it differently, with no save-state machinery.
- Determinism (invariant 2) survives play, because play is recorded rather
  than improvised into the world's state.

Building a seat therefore means adding *intervention kinds*, not a parallel
game state. Today there are three (`SetParam`, `Issue`, `IssueToAllHouseholds`).

### The bank seat, and Apache Fineract

Playing a bank is the most interesting seat, because a bank's decisions are
where the macro consequences are largest — credit is the first real demand
leakage this model has (see extension 7), and the leading explanation for why
criterion 1 fails.

**Apache Fineract** (the open-source core-banking platform for microfinance) is
worth reading as a *domain textbook* for that seat — never as a foundation.
What to take:

- How a loan product decomposes: principal, repayment schedule, rate method,
  fees, penalties. That is the menu of dials a player-bank sets.
- What a bank operator actually needs on screen: portfolio at risk, delinquency
  buckets, liquidity position. A ready-made panel list.
- Its chart of accounts, as a skeleton for a bank agent's own balance sheet.

**Why not build on it.** Fineract is a database application that happens to
model money; this is closer to a physics engine that happens to model money.
Three hard blockers, in order of severity:

1. *Speed.* The reference run does ~855M transfers in 14 s — a transfer here is
   one subtraction and one addition in cache. In Fineract each is a durable DB
   transaction. Six orders of magnitude, and the profit distribution (82% of
   movements, 100 x 1000 per month) is the exact shape a real bank never sees.
2. *Determinism.* Thread pools, ORM iteration order, timestamps and
   auto-increment ids would break invariant 2 *quietly* — runs would mostly
   match, which is worse than obviously not matching.
3. *Snapshots.* Backward scrub is a plain assignment of `World` (invariants 3
   and 4). The equivalent is snapshotting a relational database every 50
   simulated months and restoring it on a slider drag.

The only sensible plumbing in that direction is an **export**: write the
journal in a chart-of-accounts format real banking tooling can read. A
converter, not a dependency.

### Ordering

Credit has to exist as a *model* first — households and firms borrowing by
rule — before the bank is lifted out as a playable seat. Designing the seat's
UI first means designing for mechanics that do not exist yet.

### Scale mismatch to design around

Fineract assumes a loan is a considered object with a human in the loop. This
world has 1000 households and 21 days a month going past. A player-bank cannot
approve loans one at a time: it needs **policy** (rules the sim executes on the
player's behalf) with individual override for the interesting cases. That
distinction — policy plus override, not per-item approval — is the main thing
to redesign rather than copy, and it generalises to every seat, including the
ERP-style firm seat.

## Open Questions
- ~~What engine/language to prototype in~~ — answered: C++ inside this
  repo, as seats in the running simulation. See Player Seats.
- Exact shape of the lean-testing loop for segment targeting.
- Exact shape of the Stonks-9800-style hiring mechanic.
- Which agent seats ship first, and what a firm (ERP-style) seat
  actually exposes.
