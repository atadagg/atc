# Bank and credit in agent-based macro: what the literature actually does

*Research note, 2026-09-01. No design decisions here — this exists so the
design that follows is borrowed rather than invented.*

We reached this question from a specific finding: criterion 1 (endogenous
unemployment fluctuation) fails, unemployment sits at 0.33% with sd 0.4pp
across 20 seeds, and the standing explanation is that Lengnick v1 is a closed
circular flow with a fixed money stock, so aggregate demand cannot diverge
from output. Credit was the nominated fix. This note asks whether the
literature supports that, and what a bank in these models is actually made of.

## Sources read

| Source | What it gave |
|---|---|
| Lengnick, Krug & Wohltmann, *Money Creation and Financial Instability: An Agent-Based Credit Network Approach* (ch. 5 of Lengnick's Kiel dissertation) | A bank by the author of our own baseline model. Read in full. |
| Delli Gatti, Gallegati, Greenwald, Russo & Stiglitz, *Business fluctuations in a credit-network economy* (arXiv 1006.3521) | The compact canonical firm–bank ruleset. Read in full. |
| Caiani, Godin, Caverzasi, Gallegati, Kinsella & Stiglitz, *Agent based-stock flow consistent macroeconomics: towards a benchmark model* (JEDC 69, 2016) | The maximal, fully stock-flow-consistent treatment. Credit sections read. |
| *A basic macroeconomic agent-based model for analyzing monetary regime shifts* (PLOS One, 2022) | A Lengnick-derived model with a Basel III bank. Read via summary. |
| Assenza, Delli Gatti & Grazzini (CATS), Riccetti/Russo/Gallegati, Dosi et al. | Rationing rules, at citation level only. |

**Not read**: the Assenza slides and the CATS papers in the original. Their
rationing rules below are quoted secondhand through Caiani et al.'s survey of
them, and are labelled as such.

## Finding 1: Lengnick did not put a bank in this model, and that is informative

The most directly relevant paper is Lengnick's own — same author, same
dissertation, credit as its subject. It does **not** extend the baseline
model. It builds a separate model in which households *are* the whole real
sector ("they buy goods but also produce them, they save but also take
loans"), with zero-intelligence agents, no firms, no labour market, and no
prices. The real side is deliberately erased so the monetary side can be seen.

So there is no authoritative "Lengnick + bank". Whatever we build is a graft
from a different lineage, and the graft is ours to justify. That is worth
knowing before we start quoting a paper as cover.

What that chapter *does* establish, and it transfers:

- **Money is created by lending and destroyed by repayment**, modelled as
  ordinary balance-sheet movements with no exceptions — precisely the shape
  our `transfer` chokepoint and `Kind::Issuer` already assume.
- Crises there are **liquidity crises, not solvency crises**. Their banks have
  zero equity throughout; a bank fails because cash inflows and outflows are
  asynchronous ("maturity mismatch"), not because it lost money. Insolvency
  arrives when a bank cannot roll its position over.
- Contagion is real without an interbank market and worse with one. The
  probability of a bankruptcy conditional on prior bankruptcies rises from
  baseline to 0.73 — bankruptcies cluster, they do not arrive independently.

## Finding 2: the minimal firm–bank ruleset is four equations

Delli Gatti et al. (2010) is the compact version, and it is compact enough to
quote whole:

- Firms **borrow to finance the wage bill**. That is the only reason to borrow.
- Bank credit supply is capped by bank net worth: `L_supply = E_bank / α`,
  with `α < 1` a prudential ratio. Supply is more than proportional to bank
  capital and is a hard ceiling.
- The interest rate on a loan is `r = k / (A_firm / Ã)^k`, where `Ã` is the
  **median** firm net worth. A firm poorer than the median pays more. This one
  line is the entire financial accelerator: weakness is expensive, and expense
  causes weakness.
- Firm net worth evolves `A_t = A_{t-1} + π_t`, and the firm **goes bankrupt
  when `A_t < 0`**. Bankrupt firms are replaced by new entrants.

That last clause matters for us specifically: entry-and-exit *with
replacement* keeps the firm count constant. Bankruptcy in this literature does
not mean a shrinking population — it means a firm's balance sheet is reset and
its network links are cut. That is compatible with our fixed-size agent arrays
and with invariant 3, and it is a much smaller change than "add firm
demography".

## Finding 3: the richer models differ in rationing, not in structure

Caiani et al. survey how banks decide whom to refuse, and the spread is wide:

- Riccetti et al.: a cap on any single borrower as a share of total loans.
- Dosi et al.: rank borrowers, serve in order until credit is exhausted.
- van der Hoog & Dawid, Raberto et al.: lend until capital requirements bind.
- Assenza et al. (CATS): a maximum admissible loss per loan, from an estimated
  probability of default.
- Caiani et al. themselves: a per-application expected-return calculation using
  a logistic default probability driven by operating cash flow versus debt
  service, plus collateral recovery.

The observation Caiani et al. make in passing is the one to keep: **banks
discriminate mainly by rationing, not by price.** A model where the bank always
lends and only varies the rate is the one shape the empirical literature
singles out as unrealistic.

The PLOS One model sits at the far end — Basel III capital adequacy,
countercyclical buffers, 18-period amortisation, interbank market, central bank
standing facilities, government bank bailouts. It is a reminder of how much
apparatus is available, and how little of it is load-bearing for a first step.

## Finding 4: the fluctuation comes from the constraint, not from the lending

This is the finding that changes the framing we started with.

The chain the literature reports for why credit generates business cycles runs
through the *refusal*, not the loan: a firm's net worth falls → it borrows on
worse terms or is rationed → it cannot fund the payroll it wanted → it hires
less → income falls → demand falls → more firms' net worth falls. Caiani et
al. and the financial-accelerator literature both describe it this way, and
the monetary-policy variants report the extreme case explicitly: above a
critical interest rate, firms cannot borrow enough to hire the workers they
want, and unemployment does not drift upward — it jumps.

The implication for us is uncomfortable and worth stating plainly:

> A bank that lends whenever asked, with no net worth, no rationing and no
> default, would most likely make criterion 1 **worse**, not better. It is a
> shock absorber: a firm short of cash on payday borrows instead of cutting
> wages or firing. Our unemployment is already almost entirely frictional
> churn from exactly those events. Smoothing them smooths the little
> fluctuation we have.

So "add credit" and "fix criterion 1" are not the same instruction. The
demand-leakage story is correct as far as it goes — credit does break the
fixed-money closed loop, which v1 genuinely cannot do — but the mechanism that
produces *fluctuation* is the balance-sheet constraint biting, and that
requires firm net worth, a rationing rule, and default. The three arrive
together or none of them do their job.

## Finding 5: three things in our codebase are already the literature's answer

Worth recording because it means the substrate work was not wasted:

- `ledger::transfer` as the sole writer of balances is the same discipline
  Lengnick et al. call stock-flow consistency, enforced by construction rather
  than checked afterwards. They verify `Σ dE = 0` each step; our
  `books_balance()` is that assertion, and `check_flows` is the stronger
  sector-level version they do not have.
- `Kind::Issuer` with `may_issue` is exactly "banks create deposits ex nihilo,
  and only banks may". A bank becomes an issuer with behaviour attached.
- Bankruptcy-with-replacement keeps the agent count fixed, so the CLAUDE.md
  rule "do not add bankruptcy" is a rule about *v1's* no-bankruptcy wage-cut
  mechanism, not a structural obstacle to the extension.

## The open questions this leaves for design

Stated, not answered.

1. **Does the wage-cut rule survive?** Lengnick's firms cut wages rather than
   fail. The credit literature's firms borrow and then fail. Running both
   means a firm cuts wages *and* borrows *and* can still die; the interaction
   is not something either source has tested.
2. **How many banks?** One (a "giant bank", as several models use) is far
   simpler and removes the whole interbank contagion layer. Multiple banks are
   where Lengnick's cascade result comes from — but that result is about
   *bank* fragility, which may not be the phenomenon we are after.
3. **Loan duration.** One period (borrow at payday, repay at revenue) is the
   older convention. Caiani et al. argue at length that this misses the
   maturity mismatch that causes crises, and use 20 periods with constant
   amortisation. Ours would be months.
4. **Do households borrow?** No source read here lets them meaningfully. If
   the target is demand exceeding output, household credit is the direct
   route, and it is the least supported by the literature.
5. **What replaces a bankrupt firm?** Net worth reset to what, links cut how,
   employees released when.
6. **Interest income has to go somewhere.** Bank profit accrues to bank net
   worth; if it never leaves, the bank drains the economy monotonically. The
   models handle this with bank dividends to households — which is our
   existing profit-distribution proxy, pointed at a new payer.

## What I would want measured before believing any of it

Our own history is the argument for this section: the rewiring bug made
criteria pass for the wrong reason, and one seed hid a crash that twenty seeds
found. A credit extension is far more capable of producing plausible-looking
numbers for wrong reasons than anything we have added so far.

- The fluctuation must be attributable. If unemployment starts varying, the
  journal has to show it varying *with* credit rationing events, not merely at
  the same time as them.
- Run it with the accelerator switched off (flat interest rate, no rationing)
  as a control. If criterion 1 improves in the control too, the mechanism is
  not what we think.
- Twenty seeds before any claim, per the standing rule.

---

## Experiment 1: money is neutral in v1, to four significant figures

*Run 2026-09-01. 20 seeds x 6000 post-burn-in months per arm.
Treatment: `scenarios/money_plus50.scenario` — every household handed 50 at
month 200, inside the burn-in, raising money in circulation from 100,000 to
150,000 and leaving 800 months to re-equilibrate before measurement.*

Paired by seed, treatment minus baseline:

| Quantity | mean diff | t | higher on |
|---|---|---|---|
| unemployment mean | −0.00002 | −0.23 | 6/20 |
| unemployment sd | −0.00004 | −0.42 | 6/20 |
| unemployment max | +0.00080 | +0.67 | 10/20 |
| Phillips | +0.0034 | +0.38 | 13/20 |
| Beveridge | +0.0062 | +1.26 | 15/20 |
| Gini mean | −0.00008 | −0.37 | 7/20 |
| output growth sd | −0.00001 | −0.47 | 6/20 |

Nothing real moved. Every nominal quantity moved by exactly the money stock:

| Ratio, treatment / baseline | Measured |
|---|---|
| price level | 1.499969 |
| wage level | 1.500562 |
| nominal GDP | 1.499942 |
| velocity | 0.999961 |

A 50% increase in the money stock buys a 50% increase in the price level and
nothing else. `MV = PY` holds as an identity here, with `V` and `Y` pinned.

**What this settles.** A bank whose contribution is *money creation* cannot fix
criterion 1. Not "probably won't" — the level of the money stock is provably
inert in this model, so the naive chain (more money → more demand → more
employment) is dead. Finding 4 above was right, and understated.

**What this does not settle, and the distinction matters.** This tests a
permanent, one-off change in the *level*. Credit does not do that. Credit makes
the money stock a *time-varying quantity correlated with firms' condition* —
expanding when firms borrow, contracting when they repay or fail. Neutrality of
the level says nothing about the effect of a stock that moves with the cycle
against nominal quantities that adjust slowly. The two are different claims and
only the first is now measured.

**The lead this opens.** If real outcomes are invariant to the money stock, the
only way a nominal quantity can bite is if it is *prevented from scaling with
it*. v1 has exactly one such quantity: `min_wage`, a hard nominal floor,
currently 1.0 against an equilibrium wage of ~48 — three orders of magnitude
from binding. That floor is the model's only nominal rigidity, and nominal
rigidity is the standard reason demand deficiency has real effects. Testing it
needs no new code: `min_wage` is a behavioural parameter, so `set_param` can
sweep it.

---

## Experiment 2: the nominal wage floor is a switch, and it is currently off

*Run 2026-09-01. `min_wage` set at month 200 via `set_param`, 20 seeds x 6000
post-burn-in months per value. Equilibrium nominal wage is ~48.*

| min_wage | u mean | u sd | u max | autocorr 6mo | Phillips | Beveridge | growth sd | Gini early → late |
|---|---|---|---|---|---|---|---|---|
| 1 (baseline) | 0.0034 | 0.0041 | 0.029 | **+0.72** | −0.44 | −0.62 | 0.0017 | 0.0137 → 0.0151 |
| 20 | 0.0034 | 0.0040 | 0.029 | +0.72 | −0.43 | −0.61 | 0.0017 | 0.0142 → 0.0138 |
| 35 | 0.0032 | 0.0038 | 0.027 | +0.71 | −0.35 | −0.53 | 0.0017 | 0.0133 → 0.0135 |
| 45 | 0.0029 | 0.0034 | 0.028 | +0.68 | −0.26 | −0.49 | 0.0016 | 0.0112 → 0.0117 |
| 48 | 0.0028 | 0.0033 | 0.024 | +0.65 | −0.25 | −0.48 | 0.0016 | 0.0106 → 0.0104 |
| 52 | 0.0069 | 0.0029 | 0.020 | −0.05 | −0.06 | −0.16 | 0.0022 | 0.0101 → 0.0101 |
| **56** | **0.0571** | **0.0091** | **0.093** | **−0.31** | −0.14 | −0.31 | 0.0048 | 0.0546 → 0.0544 |
| 60 | 0.0774 | 0.0134 | 0.126 | −0.30 | −0.13 | −0.28 | 0.0061 | 0.1278 → 0.3278 |
| 70 | 0.0967 | 0.0152 | 0.153 | −0.36 | −0.11 | −0.21 | 0.0071 | 0.2181 → 0.5395 |
| 100 | 0.1441 | 0.0195 | 0.221 | −0.32 | −0.14 | −0.11 | 0.0093 | 0.3299 → 0.6365 |

*Harness caveat: 3 of 20 seeds at `min_wage = 1` failed with "scenario: empty
or missing header" — the throwaway sweep script rewrote one scenario file from
several threads at once. A harness race, not a model failure; that row's 17
surviving seeds reproduce the known baseline exactly (0.0034 / 0.0041), so the
row stands.*

### What the table says

**There is a threshold, and it is sharp.** Below the equilibrium wage the floor
is invisible — 1, 20, 35 and 45 are the same economy. Between 52 and 56 the
model changes character entirely: unemployment goes from 0.7% to 5.7%.

**At `min_wage = 56` the model produces Lengnick's number.** 5.7% mean
unemployment against his ~5%, with sd 0.91pp — more than double the baseline's
0.41pp — and a maximum of 9.3%.

**The fluctuation becomes a cycle, not just more noise.** The six-month
autocorrelation of unemployment flips sign, +0.72 → −0.31. Baseline
unemployment is smoothly persistent: high months follow high months and it
drifts. Above the threshold it *oscillates* — a high month is followed by a low
one about six months later. Real output volatility triples (growth sd 0.0017 →
0.0048). That is a business cycle in the shape criterion 1 was written to look
for, and the baseline model has never produced one.

**Criterion 4 moves with it.** Wealth dispersion rises from 0.015 to 0.054, and
above 60 it widens *within the run* rather than sitting flat (0.128 → 0.328 at
`min_wage = 60`). The dispersion criterion stops being the marginal, arguable
pass it is today.

**And criteria 2 and 3 get worse.** Phillips weakens −0.44 → −0.14, Beveridge
−0.62 → −0.31. Both stay negative on 20/20 seeds, so neither *fails*, but the
correlations that are currently our strongest results are substantially
diluted. This is a trade-off, not a free win, and any future change that
improves criterion 1 has to be checked against these two.

### What this is, and what it is not

It is **not** a fix, and it must not be reported as one. We turned a knob by
hand to a value nothing in the model chooses. `min_wage` exists only to stop
the zero-wage absorbing state; setting it near the equilibrium wage is us
imposing the answer.

What it **is** is the identification of the transmission channel, and that is
the thing the bank design needed:

> Experiment 1 showed that the *level* of the money stock has no real effects
> whatsoever. Experiment 2 shows why, and what the exception is: real outcomes
> are invariant to nominal conditions *because every nominal quantity is free
> to scale*. Introduce one nominal quantity that cannot scale, and nominal
> conditions acquire real effects immediately — a cycle, higher unemployment,
> and widening inequality all at once.

So the question for the credit extension is no longer "does credit create
money" (it does, and money is inert) but **"does credit make the money stock
move against something that cannot move with it?"** A credit contraction
against a downwardly rigid wage is exactly that configuration. A credit
contraction in a fully flexible economy is another 50% injection: nothing.

### The lead this opens in turn

v1 already contains an *asymmetric* rigidity, and the asymmetry may be
accidental. A household's reservation wage never falls when its employer cuts
its pay (it searches harder instead) — a genuine downward rigidity on the
worker's side. But `pay_wages` lets the firm cut the posted wage by up to 50% in
one month regardless. The worker's floor is real; the firm's is not.

Whether closing that gap — making the *posted* wage downwardly rigid the way
the reservation wage already is — produces the same regime endogenously, with
no hand-set parameter, is untested and cheap to test. If it does, criterion 1
may have a much smaller answer than a banking sector.

---

## Experiment 3: endogenous wage rigidity gives a third of the effect

*Run 2026-09-01. `max_emergency_wage_cut` set at month 200 via `set_param`,
20 seeds x 6000 post-burn-in months per value, 140 runs, none failed.*

This tests the lead from Experiment 2 without inventing anything:
`max_emergency_wage_cut` already bounds how far a firm that cannot afford its
payroll may cut the posted wage in one month. Driving it to zero is downward
nominal wage rigidity derived from the firm's own history — it scales with the
price level, where `min_wage` does not, so it is not us imposing the answer.

| max cut | u mean | u sd | u max | ac 1mo | ac 6mo | Phillips | Beveridge | growth sd | wage | Gini |
|---|---|---|---|---|---|---|---|---|---|---|
| 0.5 (default) | 0.0034 | 0.0041 | 0.0295 | +0.91 | +0.72 | −0.44 | −0.62 | 0.0017 | 48.37 | 0.015 |
| 0.25 | 0.0033 | 0.0039 | 0.0297 | +0.91 | +0.71 | −0.42 | −0.58 | 0.0017 | 48.78 | 0.014 |
| 0.10 | 0.0030 | 0.0036 | 0.0264 | +0.89 | +0.69 | −0.42 | −0.45 | 0.0017 | 49.93 | 0.012 |
| 0.05 | 0.0031 | 0.0035 | 0.0251 | +0.89 | +0.67 | −0.43 | −0.36 | 0.0017 | 50.83 | 0.011 |
| 0.02 | 0.0029 | 0.0029 | 0.0204 | +0.84 | +0.54 | −0.39 | −0.30 | 0.0016 | 51.58 | 0.008 |
| 0.01 | 0.0033 | 0.0022 | 0.0165 | +0.72 | +0.18 | −0.21 | −0.25 | 0.0017 | 51.88 | 0.008 |
| **0.00** | **0.0213** | **0.0049** | **0.0419** | +0.78 | **−0.06** | −0.15 | −0.28 | 0.0033 | 53.28 | 0.021 |

Phillips and Beveridge stay negative on 20/20 seeds at every value.

**Verdict: the direction is confirmed, the magnitude is not.** Endogenous
rigidity raises unemployment 0.34% → 2.13%, doubles real output volatility and
lifts wealth dispersion, with no hand-set constant. But the cycle barely
appears — six-month autocorrelation reaches only −0.06 against −0.31 under
`min_wage = 56` — and fluctuation rises 20% where the absolute floor doubled
it. Criterion 1 improves; it is not solved.

**Why the two differ, which is the mechanism worth keeping.** `min_wage = 56`
holds wages *above* market-clearing for every firm, permanently. Stopping cuts
only prevents falls: healthy firms still raise wages, the wage level drifts up
just 10% (48.4 → 53.3), and the constraint binds on a minority of firms in a
minority of months. **Preventing a fall is much weaker than imposing a floor.**

**A non-monotonicity, and do not interpolate across it.** Fluctuation *falls*
below baseline as the bound tightens (0.0041 → 0.0022 at cut = 0.01) and then
jumps at exactly 0.0. A 1%-per-month cut still compounds geometrically; zero is
a different regime, not the limit of the others. The u_sd column is
non-monotone in the parameter and a two-point sweep would have read it
backwards.

## Where this leaves the bank extension

Three measured results, in order:

1. The **level** of the money stock has no real effects at all (E1).
2. A nominal quantity that cannot scale gives cycles, 5.7% unemployment and
   widening inequality — but only when it binds broadly (E2).
3. The model's own downward wage rigidity binds too narrowly to do that on its
   own (E3).

So the bank is **not** displaced by a cheaper fix, and it is no longer
motivated by "money creation". Its motivation is now specific and testable:

> Credit is the mechanism that makes a *broad, time-varying* constraint out of
> a narrow one. A firm's wage bill is not the only nominal commitment it can
> fail to meet — a debt repayment is fixed in nominal terms too, does not
> shrink when revenue does, and lands on every borrower at once when credit
> tightens. That is the "binds broadly" condition E2 identified, arrived at
> endogenously.

The design should therefore be judged on whether it produces a broad
simultaneous constraint, not on whether it creates money. Concretely, that
argues for keeping **firm net worth, rationing, and default** (they are what
makes the constraint bind) and treating loan duration and bank count as
secondary.

## Handoff — state as of end of session, 2026-09-01

Nothing is committed. In the working tree:

- `docs/superpowers/research/2026-09-01-bank-credit-literature.md` — this file.
- `scripts/ensemble.py` — new `--scenario` flag, applying one intervention log
  across all seeds; three new reported rows (money stock, price level, wage
  level).
- `scripts/validate.py` — `compute` gains `price_mean`, `wage_mean`,
  `money_mean`. Additive; no existing key changed.
- `scenarios/money_plus50.scenario` — the E1 treatment.

Throwaway harnesses (not in the repo, in the session scratchpad):
`sweep_minwage.py`, `sweep_sticky.py`. The first has a known race — it wrote
one scenario file from several threads; the second fixes it by writing the
files up front. Reuse the second as the template.

**Not done:** no design, no plan, no engine change. The six open design
questions above are unanswered; questions 1 (does the wage-cut rule survive)
and 2 (one bank or many) are the ones the next session should settle first.

**The one instruction to carry forward:** do not report `min_wage = 56` as a
result. It is a knob set by hand to a value nothing in the model chooses, and
it belongs in this note as a diagnostic, not in CLAUDE.md as a finding.
