"""Stdlib-only validation of the v1 success criteria.

matplotlib/pandas are not available in this environment (Arch Linux, PEP 668,
no pip). This script reproduces the numeric checks from plot.py using only
`csv` and `math` from the standard library, so the four criteria can be
measured without any third-party dependency.

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


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "output/run.csv"

    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
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

    n = len(rows)
    print(f"loaded {n} rows from {path}")
    print()

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

    # --- Criterion 1: endogenous unemployment fluctuation ---
    u_mean = mean(unemployment)
    u_sd = stdev(unemployment)
    u_min, u_max = min(unemployment), max(unemployment)
    print("Criterion 1: endogenous unemployment fluctuation")
    print(f"  unemployment_rate: mean={u_mean:.4f} sd={u_sd:.4f} "
          f"min={u_min:.4f} max={u_max:.4f}  (want sd > 0, visible cycling)")
    print()

    # --- Criterion 2: Phillips curve ---
    corr_phillips = pearson(unemployment_tail, wage_inflation)
    print("Criterion 2: Phillips curve (unemployment vs wage inflation)")
    print(f"  correlation = {corr_phillips:+.4f}  (want negative)")
    print()

    # --- Criterion 3: Beveridge curve ---
    corr_beveridge = pearson(unemployment, vacancies)
    print("Criterion 3: Beveridge curve (unemployment vs vacancies)")
    print(f"  correlation = {corr_beveridge:+.4f}  (want negative)")
    print()

    # --- Criterion 4: wealth dispersion ---
    gini_mean = mean(gini)
    tenth = max(1, n // 10)
    gini_early = mean(gini[:tenth])
    gini_late = mean(gini[-tenth:])
    print("Criterion 4: wealth dispersion (Gini of household liquidity)")
    print(f"  mean={gini_mean:.4f}  (want > 0)")
    print(f"  early (first {tenth} months) = {gini_early:.4f}, "
          f"late (last {tenth} months) = {gini_late:.4f}  (want: no collapse to 0)")
    print()

    # --- Money conservation across the whole run ---
    m_min, m_max = min(total_money), max(total_money)
    print("Money conservation across the run")
    print(f"  min={m_min:.9f} max={m_max:.9f} spread={m_max - m_min:.9f}")


if __name__ == "__main__":
    main()
