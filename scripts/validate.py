"""Stdlib-only validation of the v1 success criteria.

Uses only `csv` and `math` from the standard library, so the four criteria can
be measured with no third-party dependency and no virtual environment.
(matplotlib and pandas *are* installable here -- see CLAUDE.md -- but nothing
in this file needs them.) `scripts/report.py` imports `load` and `compute`
from here so the HTML review page and this terminal output cannot drift apart.

Usage: python3 scripts/validate.py [path/to/run.csv]
"""

import csv
import math
import sys


def mean(xs):
    return sum(xs) / len(xs)


def stdev(xs):
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def pearson(xs, ys):
    n = len(xs)
    mx, my = mean(xs), mean(ys)
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx == 0 or sy == 0:
        return float("nan")
    return cov / (sx * sy)


def load(path):
    """Read the run CSV into a list of dicts of the columns the criteria need."""
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({
                "month": int(row["month"]),
                "unemployment_rate": float(row["unemployment_rate"]),
                "mean_price": float(row["mean_price"]),
                "mean_wage": float(row["mean_wage"]),
                "daily_capacity": float(row["daily_capacity"]),
                "vacancies": float(row["vacancies"]),
                "gini": float(row["gini"]),
                "total_money": float(row["total_money"]),
            })
    return rows


def compute(rows):
    """Every number the success criteria need, as a flat dict.

    The single source of truth for the criteria math: `main` prints from this
    dict and `scripts/report.py` renders the same dict into HTML.
    """
    n = len(rows)

    unemployment = [r["unemployment_rate"] for r in rows]
    vacancies = [r["vacancies"] for r in rows]
    gini = [r["gini"] for r in rows]
    total_money = [r["total_money"] for r in rows]
    wages = [r["mean_wage"] for r in rows]

    # Month-over-month wage inflation, aligned with unemployment_rate[1:].
    wage_inflation = [
        (wages[i] - wages[i - 1]) / wages[i - 1] for i in range(1, n)
    ]
    unemployment_tail = unemployment[1:]

    tenth = max(1, n // 10)
    m_min, m_max = min(total_money), max(total_money)

    return {
        "n": n,
        "month_first": rows[0]["month"],
        "month_last": rows[-1]["month"],
        "u_mean": mean(unemployment),
        "u_sd": stdev(unemployment),
        "u_min": min(unemployment),
        "u_max": max(unemployment),
        "phillips": pearson(unemployment_tail, wage_inflation),
        "beveridge": pearson(unemployment, vacancies),
        "gini_mean": mean(gini),
        "gini_tenth": tenth,
        "gini_early": mean(gini[:tenth]),
        "gini_late": mean(gini[-tenth:]),
        "money_min": m_min,
        "money_max": m_max,
        "money_spread": m_max - m_min,
    }


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "output/run.csv"

    rows = load(path)
    c = compute(rows)
    print(f"loaded {c['n']} rows from {path}")
    print()

    # --- Criterion 1: endogenous unemployment fluctuation ---
    print("Criterion 1: endogenous unemployment fluctuation")
    print(f"  unemployment_rate: mean={c['u_mean']:.4f} sd={c['u_sd']:.4f} "
          f"min={c['u_min']:.4f} max={c['u_max']:.4f}  (want sd > 0, visible cycling)")
    print()

    # --- Criterion 2: Phillips curve ---
    print("Criterion 2: Phillips curve (unemployment vs wage inflation)")
    print(f"  correlation = {c['phillips']:+.4f}  (want negative)")
    print()

    # --- Criterion 3: Beveridge curve ---
    print("Criterion 3: Beveridge curve (unemployment vs vacancies)")
    print(f"  correlation = {c['beveridge']:+.4f}  (want negative)")
    print()

    # --- Criterion 4: wealth dispersion ---
    print("Criterion 4: wealth dispersion (Gini of household liquidity)")
    print(f"  mean={c['gini_mean']:.4f}  (want > 0)")
    print(f"  early (first {c['gini_tenth']} months) = {c['gini_early']:.4f}, "
          f"late (last {c['gini_tenth']} months) = {c['gini_late']:.4f}  (want: no collapse to 0)")
    print()

    # --- Money conservation across the whole run ---
    print("Money conservation across the run")
    print(f"  min={c['money_min']:.9f} max={c['money_max']:.9f} "
          f"spread={c['money_spread']:.9f}")


if __name__ == "__main__":
    main()
