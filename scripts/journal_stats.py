"""Per-firm and per-reason statistics from a journal window. Stdlib only.

The run CSV records aggregates, so a mean can hide a pathological
distribution -- which is exactly how the size-weighted rewiring bug survived
(66 of 100 firms empty, invisible in every recorded series). The journal is
per-movement, so the distribution can be reconstructed from it without adding
anything to the recorder.

What is derived here, and from what:

  firm size      one `wage` entry per employee paid, so the count of wage
                 entries out of firm F is F's headcount that month.
  reason mix     the share of all movements by reason.
  tail residue   `distribute_profits` pays the last household the exact
                 remainder, so household N-1 receives a share that differs
                 from its proportional entitlement. That difference is the
                 rounding residue, and it always lands on the same household.

Usage: python3 scripts/journal_stats.py [path/to/journal.csv]
"""

import collections
import csv
import re
import sys
from pathlib import Path


def read_params():
    """Household and firm counts, read out of params.hpp so they cannot drift."""
    src = (Path(__file__).resolve().parent.parent / "src" / "params.hpp").read_text()

    def constant(name):
        m = re.search(rf"k{name}\s*=\s*(\d+)", src)
        if not m:
            raise SystemExit(f"journal_stats: k{name} not found in params.hpp")
        return int(m.group(1))

    return constant("NumHouseholds"), constant("NumFirms")


def load(path):
    with open(path, newline="") as f:
        return [
            {
                "month": int(r["month"]),
                "from_kind": r["from_kind"],
                "from_index": int(r["from_index"]),
                "to_kind": r["to_kind"],
                "to_index": int(r["to_index"]),
                "amount": float(r["amount"]),
                "reason": r["reason"],
            }
            for r in csv.DictReader(f)
        ]


def firm_sizes(entries, num_firms):
    """Headcount per firm per month, from the wage entries."""
    per_month = collections.defaultdict(lambda: [0] * num_firms)
    for e in entries:
        if e["reason"] == "wage":
            per_month[e["month"]][e["from_index"]] += 1
    return dict(per_month)


def histogram(sizes, edges):
    """Count firms per size bucket. `edges` are inclusive lower bounds."""
    counts = [0] * len(edges)
    for s in sizes:
        for i in reversed(range(len(edges))):
            if s >= edges[i]:
                counts[i] += 1
                break
    return counts


def tail_residue(entries, num_households):
    """How much the last household gains from taking the exact remainder.

    Each firm's profit block pays households 0..N-1. The first N-1 are paid a
    strictly proportional share, so their weights can be read straight off the
    journal: w_i = amount_i / pot. Those weights are captured once per month,
    before any distribution, so every firm in a month uses the same ones --
    which is what lets the last household's *entitlement* be reconstructed
    (w_last = 1 - sum of the others) and compared against what it was actually
    paid. The difference is the residue.
    """
    blocks = collections.defaultdict(list)
    for e in entries:
        if e["reason"] == "profit":
            blocks[(e["month"], e["from_index"])].append(e)

    complete = {}
    for key, block in blocks.items():
        if len(block) == num_households and block[-1]["to_index"] == num_households - 1:
            complete[key] = block

    # Recover the shared weights per month from the non-last payments.
    weights = {}
    for (month, _), block in complete.items():
        if month in weights:
            continue
        pot = sum(x["amount"] for x in block)
        if pot <= 0.0:
            continue
        weights[month] = 1.0 - sum(x["amount"] for x in block[:-1]) / pot

    residues = []
    for (month, _), block in complete.items():
        w_last = weights.get(month)
        if w_last is None:
            continue
        pot = sum(x["amount"] for x in block)
        residues.append(block[-1]["amount"] - pot * w_last)
    return residues, len(blocks), len(complete)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "output/journal.csv"
    num_households, num_firms = read_params()
    entries = load(path)
    if not entries:
        raise SystemExit(f"journal_stats: no entries in {path}")

    months = sorted({e["month"] for e in entries})
    print(f"loaded {len(entries)} entries from {path}")
    print(f"months {months[0]}..{months[-1]} ({len(months)} month(s)), "
          f"{num_firms} firms, {num_households} households")
    print()

    # --- Reason mix ---------------------------------------------------------
    print("Movements by reason")
    by_reason = collections.Counter(e["reason"] for e in entries)
    by_value = collections.Counter()
    for e in entries:
        by_value[e["reason"]] += e["amount"]
    for reason, count in by_reason.most_common():
        print(f"  {reason:<12} {count:>8} movements  {count / len(entries):>6.1%}"
              f"   value {by_value[reason]:>14.2f}")
    print()

    # --- Firm size distribution --------------------------------------------
    # The check the four criteria cannot make: a healthy economy and a
    # collapsed one can report the same mean employment.
    print("Firm size distribution (employees per firm, from wage entries)")
    sizes_by_month = firm_sizes(entries, num_firms)
    edges = [0, 1, 5, 10, 15, 20, 30]
    labels = ["0", "1-4", "5-9", "10-14", "15-19", "20-29", "30+"]
    for month in months:
        sizes = sizes_by_month.get(month, [0] * num_firms)
        counts = histogram(sizes, edges)
        staffed = [s for s in sizes if s > 0]
        bars = "  ".join(f"{lab}:{c}" for lab, c in zip(labels, counts))
        print(f"  month {month}: {bars}")
        print(f"    empty {counts[0]} of {num_firms}"
              f"   employed {sum(sizes)}"
              f"   mean per staffed firm "
              f"{(sum(staffed) / len(staffed) if staffed else 0):.1f}"
              f"   largest {max(sizes)}")
    print()

    # --- The profit-distribution tail --------------------------------------
    print("Profit distribution tail (residue landing on the last household)")
    residues, blocks, complete = tail_residue(entries, num_households)
    if not residues:
        print(f"  no complete distribution blocks in this window "
              f"({blocks} blocks, {complete} complete)")
    else:
        worst = max(residues, key=abs)
        total = sum(residues)
        print(f"  {len(residues)} complete blocks, residue per block: "
              f"worst {worst:+.3e}, total {total:+.3e}")
        print(f"  extrapolated over 6000 months: "
              f"{total * 6000 / len(months):+.3e}")


if __name__ == "__main__":
    main()
