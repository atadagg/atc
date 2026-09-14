"""Stdlib-only validation of the v1 success criteria.

Uses only `csv` and `math` from the standard library, so the four criteria can
be measured with no third-party dependency and no virtual environment.
(matplotlib and pandas *are* installable here -- see CLAUDE.md -- but nothing
in this file needs them.) `scripts/ensemble.py` imports `load` and `compute`
from here so the per-seed and across-seed numbers cannot drift apart.

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


def autocorr(xs, lag):
    """Correlation of a series with itself `lag` months earlier."""
    return pearson(xs[:-lag], xs[lag:])


def excess_kurtosis(xs):
    """Fourth moment less 3, so a normal distribution scores 0.

    Real output growth is famously not normal: the distribution is tent-shaped,
    with far more quiet months and far more violent ones than a bell curve
    allows. A model reproducing that scores well above 0; one whose shocks are
    just averaged noise scores near it.
    """
    m = mean(xs)
    var = sum((x - m) ** 2 for x in xs) / len(xs)
    if var == 0:
        return float("nan")
    fourth = sum((x - m) ** 4 for x in xs) / len(xs)
    return fourth / (var * var) - 3.0


def load(path):
    """Read the run CSV into a list of dicts of the columns the criteria need."""
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            r = {
                "month": int(row["month"]),
                "unemployment_rate": float(row["unemployment_rate"]),
                "mean_price": float(row["mean_price"]),
                "mean_wage": float(row["mean_wage"]),
                "daily_capacity": float(row["daily_capacity"]),
                "vacancies": float(row["vacancies"]),
                "gini": float(row["gini"]),
                "total_money": float(row["total_money"]),
            }
            # Flow columns arrived after the criteria columns did, and the
            # supply-chain reasons (trade credit settlement, firm liquidation,
            # firm founding) after that. A CSV written before any of them
            # still loads; columns it lacks are simply absent from `compute`
            # rather than silently reported as zero.
            for col in ("flow_purchase", "flow_wage", "flow_profit",
                        "flow_issuance", "flow_trade_credit_settlement",
                        "flow_firm_liquidation", "flow_firm_founding",
                        "flow_discrepancy"):
                if col in row:
                    r[col] = float(row[col])
            rows.append(r)
    return rows


def compute(rows):
    """Every number the success criteria need, as a flat dict.

    The single source of truth for the criteria math: `main` prints from this
    dict and `scripts/ensemble.py` aggregates it across seeds.
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

    # Output growth, month over month. `daily_capacity` is the recorded
    # output rate, so its growth rate is the model's analogue of GDP growth.
    capacity = [r["daily_capacity"] for r in rows]
    output_growth = [
        (capacity[i] - capacity[i - 1]) / capacity[i - 1]
        for i in range(1, n) if capacity[i - 1] != 0.0
    ]
    # Okun's law relates output growth to the change in unemployment. In v1 it
    # is an identity rather than a result: recorded output is lambda x employed
    # and unemployment is 1 - employed/N, so the two series are the same
    # variable rescaled and the correlation is mechanically -1. Kept, and
    # printed with that caveat, because it stops being trivial the moment
    # productivity varies -- multiple goods, capital, or firm-level lambda.
    du = [unemployment[i] - unemployment[i - 1] for i in range(1, n)]

    tenth = max(1, n // 10)
    m_min, m_max = min(total_money), max(total_money)

    # The nominal side, from the flow columns. These are money that *moved*
    # during the month, where every other column is a level at its end.
    nominal = {}
    if "flow_purchase" in rows[0]:
        purchases = [r["flow_purchase"] for r in rows]
        wage_bill = [r["flow_wage"] for r in rows]
        discrepancy = [abs(r["flow_discrepancy"]) for r in rows]
        # Household spending on goods is the model's only final expenditure,
        # so it is nominal GDP on the expenditure side. Profit distribution is
        # deliberately not counted: it is a transfer of income, not output, and
        # it is ~82% of all movements -- adding it in would inflate GDP roughly
        # fivefold and measure the ownership proxy rather than the economy.
        nominal = {
            "nominal_gdp_mean": mean(purchases),
            "nominal_gdp_sd": stdev(purchases),
            # Labour's cut of nominal output. The rest reaches households as
            # the profit distribution, which is why this is well below 1.
            "wage_share": mean([w / g for w, g in zip(wage_bill, purchases) if g != 0.0]),
            # How many times the money stock turns over in purchases per month.
            "velocity": mean([g / m for g, m in zip(purchases, total_money) if m != 0.0]),
            "issuance_total": sum(r["flow_issuance"] for r in rows),
            # Stock-flow consistency: the flows the journal saw, against the
            # change in the household stock. The C++ side aborts a run above a
            # tolerance, so a number here is a crumb, not a gap.
            "flow_discrepancy_max": max(discrepancy),
        }

    return {
        **nominal,
        "n": n,
        "month_first": rows[0]["month"],
        "month_last": rows[-1]["month"],
        # The nominal levels. Kept alongside the real ones because the two
        # together are what distinguishes a change in the price level from a
        # change in the economy: money neutrality means these move and
        # `u_mean` does not.
        "price_mean": mean([r["mean_price"] for r in rows]),
        "wage_mean": mean(wages),
        "money_mean": mean(total_money),
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
        "u_autocorr1": autocorr(unemployment, 1),
        "u_autocorr6": autocorr(unemployment, 6),
        "growth_sd": stdev(output_growth),
        "growth_kurtosis": excess_kurtosis(output_growth),
        "okun": pearson(output_growth, du[:len(output_growth)]),
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

    # --- Beyond the four criteria -------------------------------------------
    # The four above can all be satisfied by an economy that is behaving
    # nothing like a real one. These are the cheap additional questions.
    print("Further checks (not v1 success criteria)")
    print(f"  unemployment persistence: autocorr lag 1 = {c['u_autocorr1']:+.4f}, "
          f"lag 6 = {c['u_autocorr6']:+.4f}  (want positive and decaying)")
    print(f"  output growth: sd = {c['growth_sd']:.4f}, "
          f"excess kurtosis = {c['growth_kurtosis']:+.3f}  (real economies: well above 0)")
    print(f"  Okun (output growth vs change in unemployment) = {c['okun']:+.4f}  "
          f"(an identity in v1 -- output IS employment; informative only once "
          f"productivity varies)")
    print()

    # --- The nominal side ---
    if "nominal_gdp_mean" in c:
        print("Nominal flows (money that moved, per month)")
        print(f"  nominal GDP (household purchases) = {c['nominal_gdp_mean']:.1f} "
              f"(sd {c['nominal_gdp_sd']:.1f})")
        print(f"  wage share of nominal GDP = {c['wage_share']:.4f}  "
              f"(the rest reaches households as the profit distribution)")
        print(f"  velocity = {c['velocity']:.4f} turns of the money stock per month")
        print(f"  issued over the run = {c['issuance_total']:.2f}")
        print(f"  stock-flow discrepancy: max |flow| - |change in stock| = "
              f"{c['flow_discrepancy_max']:.3e}  (want machine epsilon)")
        print()

    # --- Money conservation across the whole run ---
    print("Money conservation across the run")
    print(f"  min={c['money_min']:.9f} max={c['money_max']:.9f} "
          f"spread={c['money_spread']:.9f}")


if __name__ == "__main__":
    main()
