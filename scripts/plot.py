"""Throwaway validation plots for the v1 success criteria.

Nothing in the C++ core depends on this file. Delete it freely.
Requires: pip install matplotlib pandas
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt

path = sys.argv[1] if len(sys.argv) > 1 else "output/run.csv"
df = pd.read_csv(path)

df["wage_inflation"] = df["mean_wage"].pct_change()

fig, axes = plt.subplots(2, 2, figsize=(13, 9))

axes[0][0].plot(df["month"], df["unemployment_rate"], linewidth=0.6)
axes[0][0].set_title("Criterion 1: endogenous unemployment fluctuation")
axes[0][0].set_xlabel("month")
axes[0][0].set_ylabel("unemployment rate")

axes[0][1].scatter(df["unemployment_rate"], df["wage_inflation"], s=1, alpha=0.2)
axes[0][1].set_title("Criterion 2: Phillips curve (expect negative slope)")
axes[0][1].set_xlabel("unemployment rate")
axes[0][1].set_ylabel("wage inflation")

axes[1][0].scatter(df["unemployment_rate"], df["vacancies"], s=1, alpha=0.2)
axes[1][0].set_title("Criterion 3: Beveridge curve (expect negative slope)")
axes[1][0].set_xlabel("unemployment rate")
axes[1][0].set_ylabel("vacancies")

axes[1][1].plot(df["month"], df["gini"], linewidth=0.6)
axes[1][1].set_title("Criterion 4: wealth dispersion")
axes[1][1].set_xlabel("month")
axes[1][1].set_ylabel("Gini of household liquidity")

plt.tight_layout()
plt.savefig("output/validation.png", dpi=130)
print("wrote output/validation.png")

corr_phillips = df["unemployment_rate"].corr(df["wage_inflation"])
corr_beveridge = df["unemployment_rate"].corr(df["vacancies"])
print(f"Phillips correlation:  {corr_phillips:+.3f}  (want negative)")
print(f"Beveridge correlation: {corr_beveridge:+.3f}  (want negative)")
print(f"unemployment: mean={df['unemployment_rate'].mean():.3f} "
      f"sd={df['unemployment_rate'].std():.3f}  (want sd > 0)")
print(f"gini: mean={df['gini'].mean():.3f}  (want > 0)")
