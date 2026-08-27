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
- No supply-chain simulation — doesn't fit the spirit of an
  invention/innovation-focused game.
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
- No separate faction/diplomacy system — this stays mechanically the
  same tree + event system as everything else.

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
Instead: a clean, chart/ledger-forward home screen — your financial
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

## Open Questions
- What engine/language to prototype in (not yet discussed).
- Exact shape of the lean-testing loop for segment targeting.
- Exact shape of the Stonks-9800-style hiring mechanic.
