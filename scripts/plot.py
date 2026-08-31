"""Figures for the v1 run: the four success criteria, the business cycle, its
lead-lag structure, and the demand decomposition.

Setup (no sudo, nothing installed system-wide):

    python3 -m venv .venv
    .venv/bin/python -m pip install matplotlib pandas

Usage:

    .venv/bin/python scripts/plot.py [path/to/run.csv] [--out output]

Writes the PNGs only. `scripts/validate.py` is the stdlib-only path to the
criteria numbers in the terminal.

The cycle, cross-correlation and demand figures need the demand-side columns
(`demand`, `planned_consumption`, `inventory`, `household_liquidity`). A run
recorded before those existed will still produce the criteria figure; the rest
are skipped with a message.
"""

import os
import sys

import matplotlib
matplotlib.use("Agg")  # headless: write files, never open a window
import matplotlib.pyplot as plt
import pandas as pd

# Series hues encode what circulates: indigo = labour, teal = goods,
# ochre = money, plum = consumption. Checked for colour-vision deficiency.
C = {"u": "#3E5AD4", "inv": "#0F8A6A", "wage": "#C77A08", "cons": "#A83480",
     "ink": "#191D1C", "mute": "#6F7B76", "rule": "#D8DDD8", "fault": "#B4453C"}

DEMAND_COLS = ["demand", "planned_consumption", "inventory", "household_liquidity"]

plt.rcParams.update({
    "figure.facecolor": "white", "axes.facecolor": "white",
    "axes.edgecolor": C["rule"], "axes.labelcolor": C["ink"],
    "text.color": C["ink"], "xtick.color": C["mute"], "ytick.color": C["mute"],
    "axes.spines.top": False, "axes.spines.right": False,
    "axes.titlesize": 11, "axes.titleweight": "bold", "axes.titlelocation": "left",
    "axes.labelsize": 9, "xtick.labelsize": 8.5, "ytick.labelsize": 8.5,
    "axes.grid": True, "grid.color": C["rule"], "grid.linewidth": 0.6,
    "figure.dpi": 130,
})


def _save(fig, out, name):
    path = os.path.join(out, name)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)
    print(f"wrote {path}")


def fig_criteria(df, out):
    """The four v1 success criteria, as originally specified."""
    fig, ax = plt.subplots(2, 2, figsize=(13, 8))

    ax[0][0].plot(df["month"], df["unemployment_rate"] * 100, lw=0.5, color=C["u"])
    ax[0][0].set(title="1 · Endogenous unemployment fluctuation",
                 xlabel="month", ylabel="unemployment (%)")

    ax[0][1].scatter(df["unemployment_rate"] * 100, df["wage_inflation"] * 100,
                     s=1.5, alpha=0.15, color=C["wage"], edgecolors="none")
    ax[0][1].set(title=f"2 · Phillips curve  (r = {df['unemployment_rate'].corr(df['wage_inflation']):+.3f})",
                 xlabel="unemployment (%)", ylabel="wage inflation (%/month)")

    ax[1][0].scatter(df["unemployment_rate"] * 100, df["vacancies"],
                     s=1.5, alpha=0.15, color=C["inv"], edgecolors="none")
    ax[1][0].set(title=f"3 · Beveridge curve  (r = {df['unemployment_rate'].corr(df['vacancies']):+.3f})",
                 xlabel="unemployment (%)", ylabel="firms with an open vacancy")

    ax[1][1].plot(df["month"], df["gini"], lw=0.5, color=C["cons"])
    ax[1][1].set(title="4 · Wealth dispersion", xlabel="month",
                 ylabel="Gini of household liquidity")

    _save(fig, out, "criteria.png")


def fig_cycle(df, out, window=240):
    """The cycle, as four panels on one shared time axis.

    Windowed on the most volatile stretch: over 6000 months the individual
    cycles are too compressed to read.
    """
    roll = df["unemployment_rate"].rolling(window).std()
    end = int(roll.idxmax()) + 1
    d = df.iloc[max(0, end - window):end]

    specs = [("planned_consumption", "Planned consumption", "units wanted", "cons", 1.0),
             ("inventory", "Inventory on shelves", "units held", "inv", 1.0),
             ("unemployment_rate", "Unemployment", "% of households", "u", 100.0),
             ("mean_wage", "Mean wage", "money per month", "wage", 1.0)]
    fig, ax = plt.subplots(len(specs), 1, figsize=(12, 8), sharex=True)
    for a, (col, name, unit, key, scale) in zip(ax, specs):
        y = d[col] * scale
        a.plot(d["month"], y, lw=1.3, color=C[key])
        a.fill_between(d["month"], y, y.min(), color=C[key], alpha=0.12)
        a.set(title=name, ylabel=unit)
        a.margins(x=0)
    ax[-1].set_xlabel("simulated month")
    fig.suptitle("Consumption softens, then inventory swells, then unemployment climbs, then wages fall",
                 x=0.008, ha="left", fontsize=12, fontweight="bold")
    _save(fig, out, "cycle.png")


def fig_crosscorr(df, out, max_lag=24):
    """Cross-correlation against unemployment. Negative lag = series leads."""
    u = df["unemployment_rate"]
    specs = [("planned_consumption", "planned consumption", "cons"),
             ("inventory", "inventory", "inv"),
             ("vacancies", "vacancies", "u"),
             ("mean_wage", "mean wage", "wage")]
    lags = range(-max_lag, max_lag + 1)
    fig, a = plt.subplots(figsize=(11, 5.5))
    for col, label, key in specs:
        r = [df[col].shift(-k).corr(u) for k in lags]
        a.plot(list(lags), r, lw=1.8, color=C[key], label=label)
        peak = max(range(len(r)), key=lambda i: abs(r[i]))
        a.plot(lags[peak], r[peak], "o", ms=6, color=C[key],
               markeredgecolor="white", markeredgewidth=1.5)
        a.annotate(f"{r[peak]:+.2f} @ {lags[peak]:+d}mo",
                   (lags[peak], r[peak]), textcoords="offset points",
                   xytext=(6, 6), fontsize=8, color=C[key], fontweight="bold")
    a.axhline(0, color=C["mute"], lw=0.8)
    a.axvline(0, color=C["mute"], lw=0.8, ls=":")
    a.set(title="Cross-correlation against unemployment",
          xlabel="lag (months) — negative means the series leads unemployment",
          ylabel="correlation")
    a.legend(frameon=False, fontsize=9)
    _save(fig, out, "crosscorr.png")


def fig_demand(df, out, window=240):
    """Stable household demand vs the volatile figure firms record.

    Left panel is windowed: at 6000 months the spikes overplot into a solid
    block. Right panel reports CVs over the whole run.
    """
    roll = df["unemployment_rate"].rolling(window).std()
    end = int(roll.idxmax()) + 1
    d = df.iloc[max(0, end - window):end]
    capacity = d["daily_capacity"] * 21.0
    fig, (a, b) = plt.subplots(1, 2, figsize=(13, 5),
                               gridspec_kw={"width_ratios": [2.2, 1]})

    a.plot(d["month"], d["demand"], lw=1.2, color=C["inv"], label="demand recorded by firms")
    a.plot(d["month"], d["planned_consumption"], lw=1.4, color=C["cons"], label="planned consumption")
    a.plot(d["month"], capacity, lw=1.1, color=C["mute"], ls="--", label="productive capacity")
    a.set(title="One want, counted several times", xlabel="simulated month",
          ylabel="units per month")
    a.legend(frameon=False, fontsize=8.5)
    a.margins(x=0)

    series = [("planned\nconsumption", df["planned_consumption"], C["cons"]),
              ("demand\nrecorded", df["demand"], C["inv"]),
              ("household\nliquidity", df["household_liquidity"], C["u"])]
    cvs = [s.std() / s.mean() * 100 for _, s, _ in series]
    bars = b.bar([n for n, _, _ in series], cvs,
                 color=[c for _, _, c in series], width=0.6)
    for bar, cv in zip(bars, cvs):
        b.annotate(f"{cv:.1f}%", (bar.get_x() + bar.get_width() / 2, cv),
                   textcoords="offset points", xytext=(0, 4),
                   ha="center", fontsize=10, fontweight="bold")
    b.set(title="Volatility (coefficient of variation)", ylabel="CV (%)")
    b.grid(axis="x", visible=False)
    _save(fig, out, "demand.png")


def main():
    args = [a for a in sys.argv[1:]]
    out = "output"
    if "--out" in args:
        i = args.index("--out")
        out = args[i + 1]
        del args[i:i + 2]
    path = args[0] if args else "output/run.csv"

    df = pd.read_csv(path)
    df["wage_inflation"] = df["mean_wage"].pct_change()
    df = df.dropna(subset=["wage_inflation"])
    os.makedirs(out, exist_ok=True)

    fig_criteria(df, out)
    missing = [c for c in DEMAND_COLS if c not in df.columns]
    if missing:
        print(f"skipping cycle/crosscorr/demand figures: {path} lacks {', '.join(missing)}.\n"
              f"  regenerate with a current build: ./build/atc --out {path}")
    else:
        fig_cycle(df, out)
        fig_crosscorr(df, out)
        fig_demand(df, out)


if __name__ == "__main__":
    main()
