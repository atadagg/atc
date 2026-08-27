"""A single-page, plain-English review of a run.

Written for someone who wants to know whether the simulation is behaving, not
for someone who already knows the vocabulary. Each check is stated as a claim
about the simulated world -- "when jobs are scarce, pay stops rising" -- with
the academic name and the measured number kept as supporting detail. The raw
numbers and a glossary sit at the bottom for when they are wanted.

Stdlib only -- no matplotlib, no pandas, no virtual environment. The numbers
come from `scripts/validate.py`'s `compute`, so this page and that script's
terminal output cannot disagree. The figures are the PNGs `scripts/plot.py`
writes; a figure that has not been generated is named rather than shown as a
broken image.

Usage:

    python3 scripts/report.py [path/to/run.csv] [--out output]

Writes `<out>/index.html`. Open it straight from the filesystem
(`xdg-open output/index.html`); copying the whole `<out>` directory copies the
whole review, page and figures together.

`scripts/plot.py` calls `write` at the end of its own run, so the existing
plotting command produces this page as well.
"""

import datetime
import html
import os
import re
import sys

import validate

# Same hues as plot.py: indigo = labour, teal = goods, ochre = money,
# plum = consumption.
C = {"u": "#3E5AD4", "inv": "#0F8A6A", "wage": "#C77A08", "cons": "#A83480",
     "ink": "#191D1C", "mute": "#6F7B76", "rule": "#D8DDD8", "fault": "#B4453C",
     "pass": "#0F8A6A", "wash": "#F7F8F7"}

# --- The five checks, in plain language --------------------------------------
#
# Each entry is (key, plain title, what it means, why it matters, academic name).
# `verdict` and `detail` are filled in per-run by `check_rows` below; the
# thresholds live there too, next to the numbers they judge.

CHECKS = [
    ("cycle", "Booms and slumps happen on their own",
     "Nobody wrote a recession into this code. There is no government, no "
     "crash, no outside shock, no script — only households shopping and firms "
     "hiring. The share of people out of work still rises and falls in long "
     "waves anyway.",
     "This is the whole point of the model. A cycle nobody designed, emerging "
     "from a thousand small decisions, is the thing being demonstrated. A flat "
     "line would mean the economy had gone inert.",
     "endogenous unemployment fluctuation"),
    ("phillips", "When jobs are scarce, pay stops rising",
     "In the months when more people are out of work, wages grow more slowly. "
     "When almost everyone has a job, firms have to compete for staff and bid "
     "pay upward.",
     "Real economies behave this way, and it was never programmed in — firms "
     "here only look at whether their own last vacancy went unfilled. The "
     "pattern has to assemble itself out of a hundred separate decisions.",
     "the Phillips curve"),
    ("beveridge", "Empty jobs and jobless people don't pile up together",
     "When lots of people are looking for work, there are few openings going "
     "spare — and when openings are plentiful, few people are looking.",
     "If both were high at the same time it would mean workers and jobs simply "
     "weren't finding each other, which would point at a broken job search "
     "rather than a working economy.",
     "the Beveridge curve"),
    ("gini", "A gap between rich and poor opens up, and stays open",
     "Every household starts with exactly the same money. Some end up much "
     "wealthier than others regardless. The score above is 0 if everyone holds "
     "an identical amount and 1 if a single household owns everything.",
     "Nobody is given an advantage at the start; the inequality is produced by "
     "the economy running. It also has to <em>persist</em> — if the gap closed "
     "back to zero, the mechanism creating it would have stalled.",
     "the Gini coefficient of household wealth"),
    ("money", "No money appears or disappears",
     "A check on the code rather than on the economics. Money changed hands "
     "millions of times, across every wallet in the economy, and the total in "
     "the system never moved by more than a rounding error.",
     "If this one ever fails, nothing else on this page is worth reading — the "
     "numbers would be measuring a bookkeeping bug rather than an economy.",
     "money conservation"),
]


def check_rows(c):
    """Per-run verdicts, thresholds and supporting numbers for each check.

    The thresholds for checks 1 and 4 are judgement calls, stated here rather
    than buried: the run CLAUDE.md records as a failure had an unemployment
    spread of 0.0047 and the one it records as a pass 0.0285, so 0.01 is the
    line between them. "Wealth gap does not close" is read as the late-run
    score holding at least half the early-run one.
    """
    return {
        "cycle": (
            c["u_sd"] >= 0.01,
            f"Out of work: {c['u_mean'] * 100:.1f}% on an average month, "
            f"but ranging from {c['u_min'] * 100:.1f}% to "
            f"{c['u_max'] * 100:.1f}% across the run.",
            f"month-to-month spread (standard deviation) {c['u_sd']:.4f}, "
            "wanted at least 0.01",
        ),
        "phillips": (
            c["phillips"] < 0,
            "Higher unemployment goes with slower pay growth, "
            f"{'strongly' if abs(c['phillips']) > 0.3 else 'weakly'} and "
            "consistently.",
            f"correlation {c['phillips']:+.3f}, wanted below 0 "
            "(−1 is a perfect opposite, 0 is no relationship)",
        ),
        "beveridge": (
            c["beveridge"] < 0,
            "More people out of work goes with fewer unfilled openings, "
            f"{'strongly' if abs(c['beveridge']) > 0.3 else 'weakly'} so.",
            f"correlation {c['beveridge']:+.3f}, wanted below 0 "
            "(−1 is a perfect opposite, 0 is no relationship)",
        ),
        "gini": (
            c["gini_mean"] > 0 and c["gini_late"] >= 0.5 * c["gini_early"],
            f"The gap scores {c['gini_mean']:.3f} on average, and it "
            f"{'widens' if c['gini_late'] > c['gini_early'] else 'holds'} over "
            f"time: {c['gini_early']:.3f} early on, {c['gini_late']:.3f} by the "
            "end.",
            f"mean {c['gini_mean']:.4f}, wanted above 0; compared over the "
            f"first and last {c['gini_tenth']} months, wanted not to close",
        ),
        "money": (
            c["money_spread"] < 1e-6,
            f"The total held by everyone stayed at {c['money_min']:,.0f} "
            f"throughout, drifting by {c['money_spread']:.0e} — that is a "
            "limit of decimal arithmetic, not money going missing.",
            f"spread {c['money_spread']:.3e}, wanted below 1e-6",
        ),
    }


# --- Figures ------------------------------------------------------------------
#
# `look_for` is the plain-English instruction; `decoder` translates the jargon
# printed inside the image itself, and is omitted where the image has none.

FIGURES = [
    ("criteria.png", "The four checks, drawn",
     "Top left is the boom-and-slump wave — that jagged line is check 1. "
     "Bottom right is the rich-poor gap climbing over time, check 4. The two "
     "clouds of dots are checks 2 and 3: what matters is that each cloud "
     "slopes downward, meaning the two things it compares move in opposite "
     "directions.",
     "The panel marked <em>Phillips curve</em> is check 2 above; "
     "<em>Beveridge curve</em> is check 3."),
    ("cycle.png", "One slump, close up",
     "Four things over the same stretch of time, stacked so you can read them "
     "top to bottom as a chain of events: households buy less, so unsold goods "
     "pile up, so firms let people go, so unemployment climbs and pay falls. "
     "This shows the most turbulent 240 months of the run — across all 500 "
     "years the waves are packed too tightly to see.",
     None),
    ("crosscorr.png", "What moves first",
     "Which of these is an early warning, and how far ahead. Everything is "
     "compared against unemployment: a line peaking to the <em>left</em> of "
     "the centre means that thing moves before unemployment does, so it is a "
     "leading signal. Height means how tightly the two track — near +1 or −1 "
     "is a strong relationship, near 0 is none.",
     "<em>Cross-correlation</em> just means \"how closely two lines track each "
     "other\"; <em>lag</em> is how far apart in time they are when they track "
     "best."),
    ("demand.png", "Why \"demand\" looks wilder than it is",
     "Households want a fairly steady amount of stuff. The wild line is not "
     "them changing their minds — it is one shopper who found an empty shelf, "
     "walked to the next shop, and got counted twice. Read the calm line as "
     "what people actually want.",
     "<em>Coefficient of variation</em> on the right is just how jumpy each "
     "line is, as a percentage — bigger means wilder."),
]

GLOSSARY = [
    ("Unemployment rate", "The share of all households with no job that month."),
    ("Vacancy", "A job a firm is trying to fill and hasn't yet."),
    ("Inventory", "Goods a firm made but hasn't sold — stock sitting on the "
     "shelf."),
    ("Wage inflation", "How fast pay is rising, month over month."),
    ("Correlation", "Whether two things move together. +1 means they rise and "
     "fall in lockstep, −1 means one rises exactly as the other falls, 0 means "
     "no relationship. It says nothing about which causes which."),
    ("Phillips curve", "The observation that low unemployment comes with "
     "faster-rising pay. Check 2 on this page."),
    ("Beveridge curve", "The observation that unfilled jobs and unemployed "
     "people move in opposite directions. Check 3."),
    ("Gini coefficient", "One number for how unevenly wealth is spread: 0 is "
     "perfectly equal, 1 is one household owning it all. Check 4."),
]

CSS = """
*, *::before, *::after { box-sizing: border-box; }
body { margin: 0; background: #fff; color: %(ink)s; font: 16px/1.6
  ui-sans-serif, -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
main { max-width: 780px; margin: 0 auto; padding: 56px 24px 110px; }
.wide { max-width: 1180px; margin: 0 auto 56px; padding: 0 24px; }
h1 { font-size: 30px; line-height: 1.25; margin: 0 0 12px; letter-spacing: -0.02em; }
h2 { font-size: 19px; margin: 0 0 10px; letter-spacing: -0.015em; }
h3 { font-size: 12px; text-transform: uppercase; letter-spacing: 0.09em;
  color: %(mute)s; margin: 0 0 18px; font-weight: 700; }
p { margin: 0 0 14px; }
em { font-style: normal; background: %(wash)s; padding: 0 3px; border-radius: 2px; }
code { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 0.9em; }
.meta { color: %(mute)s; font-size: 14px; margin: 0 0 46px; }
.check { border-top: 1px solid %(rule)s; padding: 26px 0 4px; }
.check .mark { float: left; width: 30px; margin-top: 1px; font-size: 17px;
  font-weight: 700; }
.check .mark.ok { color: %(pass)s; }
.check .mark.no { color: %(fault)s; }
.check .body { margin-left: 30px; }
.check .found { color: %(ink)s; font-weight: 600; }
.check .why { color: %(mute)s; }
.check .fine { color: %(mute)s; font-size: 13px; margin: 14px 0 0;
  padding-top: 12px; border-top: 1px dotted %(rule)s;
  font-variant-numeric: tabular-nums; }
.check .fine b { font-weight: 600; color: %(ink)s; }
figure { margin: 0 0 60px; }
figure img { width: 100%%; height: auto; display: block; margin-top: 16px;
  border: 1px solid %(rule)s; border-radius: 4px; }
figcaption { color: %(mute)s; font-size: 14.5px; max-width: 74ch; }
figcaption .decoder { display: block; margin-top: 8px; font-size: 13px; }
.missing { border: 1px dashed %(rule)s; border-radius: 4px; padding: 26px;
  margin-top: 16px; color: %(mute)s; font-size: 14px; }
.missing code { color: %(ink)s; }
table { border-collapse: collapse; width: 100%%; margin: 0 0 40px;
  font-size: 14px; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 9px 12px; border-bottom: 1px solid %(rule)s;
  vertical-align: baseline; }
th { font-size: 11px; text-transform: uppercase; letter-spacing: 0.07em;
  color: %(mute)s; font-weight: 600; }
td.num { font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  white-space: nowrap; }
dl { margin: 0; font-size: 14.5px; }
dt { font-weight: 650; margin-top: 16px; }
dd { margin: 3px 0 0; color: %(mute)s; }
hr { border: 0; border-top: 1px solid %(rule)s; margin: 60px 0 44px; }
""" % C


def headline(results):
    """The one-sentence verdict, and the count behind it."""
    failed = [k for k, (ok, _, _) in results.items() if not ok]
    n = len(CHECKS)
    if not failed:
        return ("This economy is behaving like a real one.",
                f"All {n} checks pass.")
    if "money" in failed:
        return ("Stop — money is not being conserved.",
                "That is a bug in the simulation, not a result. Nothing else "
                "on this page is trustworthy until it is fixed.")
    return ("Something is off in this economy.",
            f"{len(failed)} of {n} checks did not pass — they are marked below.")


def check_block(key, title, means, why, academic, ok, found, fine):
    mark = ('<span class="mark ok">&#10003;</span>' if ok
            else '<span class="mark no">&#10007;</span>')
    return (
        f'<section class="check">{mark}<div class="body">'
        f"<h2>{title}</h2>"
        f'<p class="found">{found}</p>'
        f"<p>{means}</p>"
        f'<p class="why">{why}</p>'
        f'<p class="fine">Economists call this <b>{academic}</b>. '
        f"Measured: {fine}.</p>"
        "</div></section>"
    )


def figure_block(out, name, title, look_for, decoder):
    if os.path.exists(os.path.join(out, name)):
        body = f'<img src="{name}" alt="{html.escape(title)}">'
    else:
        body = (f'<div class="missing"><code>{name}</code> has not been drawn '
                "yet. Run <code>.venv/bin/python scripts/plot.py</code>; if "
                "that reports missing columns, the run predates them and needs "
                "regenerating with a current build.</div>")
    decode = f'<span class="decoder">{decoder}</span>' if decoder else ""
    return (f"<figure><h2>{html.escape(title)}</h2>"
            f"<figcaption>{look_for}{decode}</figcaption>{body}</figure>")


def numbers_table(c, results):
    order = [("cycle", "1 · Booms and slumps"), ("phillips", "2 · Scarce jobs, flat pay"),
             ("beveridge", "3 · Openings vs jobseekers"), ("gini", "4 · Rich-poor gap"),
             ("money", "Money conservation")]
    rows = "".join(
        f"<tr><td>{label}</td><td class='num'>{results[k][2]}</td>"
        f"<td>{'pass' if results[k][0] else 'FAIL'}</td></tr>"
        for k, label in order)
    return ("<table><thead><tr><th>Check</th><th>Measured, with threshold</th>"
            "<th></th></tr></thead><tbody>" + rows + "</tbody></table>")


def glossary_list():
    return "<dl>" + "".join(f"<dt>{t}</dt><dd>{d}</dd>" for t, d in GLOSSARY) + "</dl>"


def _scale():
    """Household and firm counts, read from params.hpp so they cannot drift."""
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        with open(os.path.join(here, os.pardir, "src", "params.hpp")) as f:
            src = f.read()
    except OSError:
        return None
    got = {}
    for key in ("kNumHouseholds", "kNumFirms"):
        m = re.search(rf"{key}\s*=\s*(\d+)", src)
        if not m:
            return None
        got[key] = int(m.group(1))
    return (f"{got['kNumHouseholds']:,} households and {got['kNumFirms']} firms, "
            "shopping and hiring")


def write(path, out):
    """Render `<out>/index.html` for the run at `path`. Returns the page path."""
    c = validate.compute(validate.load(path))
    results = check_rows(c)
    verdict, count = headline(results)

    years = c["n"] / 12.0
    scale = _scale()
    seed = re.search(r"seed[-_]?(\d+)", os.path.basename(path))
    stamp = datetime.datetime.fromtimestamp(os.path.getmtime(path))
    meta = (f"{scale + ', ' if scale else ''}followed for {c['n']:,} months — "
            f"about {years:,.0f} years of simulated time."
            f"{f' Random seed {seed.group(1)}.' if seed else ''}"
            f"<br>From <code>{html.escape(path)}</code>, "
            f"recorded {stamp:%-d %B %Y at %H:%M}.")

    checks = "".join(check_block(k, t, m, w, a, *results[k])
                     for k, t, m, w, a in CHECKS)
    figures = "".join(figure_block(out, *f) for f in FIGURES)

    page = (
        "<!doctype html>\n<html lang='en'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>economy — run review</title>"
        f"<style>{CSS}</style></head><body>"
        "<main>"
        f"<h1>{verdict}</h1>"
        f"<p class='meta'>{count} {meta}</p>"
        f"{checks}"
        "</main>"
        "<div class='wide'>"
        "<h3>The pictures</h3>"
        f"{figures}"
        "</div>"
        "<main>"
        "<hr><h3>The numbers, unabbreviated</h3>"
        f"{numbers_table(c, results)}"
        "<h3>Words this page tried to avoid</h3>"
        f"{glossary_list()}"
        "</main>"
        "</body></html>\n"
    )

    os.makedirs(out, exist_ok=True)
    page_path = os.path.join(out, "index.html")
    with open(page_path, "w") as f:
        f.write(page)
    return page_path


def main():
    args = sys.argv[1:]
    out = "output"
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]
    path = args[0] if args else "output/run.csv"
    print(f"wrote {write(path, out)}")


if __name__ == "__main__":
    main()
