"""Run the same model across many seeds and report the spread. Stdlib only.

One run is one history. The criteria this project cares about are claims about
how much the economy moves, and reading those off a single trajectory cannot
distinguish "the model changed" from "the dice landed differently". This runs
N seeds and reports each criterion as a mean with a range, so a change can be
judged against the noise it has to beat.

Usage:
    python3 scripts/ensemble.py [--seeds 20] [--months 6000] [--burn-in 1000]
                               [--jobs 8] [--binary ./build/economy]
                               [--dir <scratch dir for the per-seed CSVs>]
"""

import argparse
import concurrent.futures
import math
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate import compute, load, mean, stdev  # noqa: E402

# Criteria worth reporting across seeds, as (key, label, format).
REPORTED = [
    ("u_mean", "unemployment mean", "{:.4f}"),
    ("u_sd", "unemployment sd", "{:.4f}"),
    ("u_max", "unemployment max", "{:.4f}"),
    ("phillips", "Phillips correlation", "{:+.4f}"),
    ("beveridge", "Beveridge correlation", "{:+.4f}"),
    ("gini_mean", "Gini mean", "{:.4f}"),
    ("gini_early", "Gini early", "{:.4f}"),
    ("gini_late", "Gini late", "{:.4f}"),
    ("money_spread", "books spread", "{:.2e}"),
]


def run_one(binary, seed, burn_in, months, out_dir):
    out = Path(out_dir) / f"seed{seed}.csv"
    proc = subprocess.run(
        [binary, "--seed", str(seed), "--burn-in", str(burn_in),
         "--months", str(months), "--out", str(out)],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        raise SystemExit(f"ensemble: seed {seed} failed:\n{proc.stderr}")
    return seed, compute(load(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--months", type=int, default=6000)
    ap.add_argument("--burn-in", type=int, default=1000)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--binary", default="./build/economy")
    ap.add_argument("--dir", default=None,
                    help="where to write the per-seed CSVs (default: a temp dir)")
    args = ap.parse_args()

    if not Path(args.binary).exists():
        raise SystemExit(f"ensemble: no binary at {args.binary} -- build first")

    out_dir = args.dir or tempfile.mkdtemp(prefix="ensemble-")
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    seeds = list(range(1, args.seeds + 1))

    print(f"{len(seeds)} seeds x {args.months} months "
          f"(burn-in {args.burn_in}), {args.jobs} at a time")
    print(f"per-seed CSVs in {out_dir}")
    print()

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_one, args.binary, s, args.burn_in,
                               args.months, out_dir) for s in seeds]
        for done in concurrent.futures.as_completed(futures):
            seed, c = done.result()
            results[seed] = c
            print(f"  seed {seed} done ({len(results)}/{len(seeds)})", flush=True)
    print()

    width = max(len(label) for _, label, _ in REPORTED)
    print(f"{'':<{width}}  {'mean':>12} {'sd':>12} {'min':>12} {'max':>12}")
    for key, label, fmt in REPORTED:
        xs = [results[s][key] for s in seeds]
        sd = stdev(xs) if len(xs) > 1 else 0.0
        print(f"{label:<{width}}  {fmt.format(mean(xs)):>12} "
              f"{fmt.format(sd):>12} {fmt.format(min(xs)):>12} "
              f"{fmt.format(max(xs)):>12}")
    print()

    # Sign agreement matters more than the average for the two correlations:
    # a criterion that holds on 20 of 20 seeds is a result, 14 of 20 is not.
    for key, label in [("phillips", "Phillips"), ("beveridge", "Beveridge")]:
        negative = sum(1 for s in seeds if results[s][key] < 0)
        print(f"{label} negative on {negative}/{len(seeds)} seeds")
    rising = sum(1 for s in seeds
                 if results[s]["gini_late"] >= 0.5 * results[s]["gini_early"])
    print(f"Gini late >= half of early on {rising}/{len(seeds)} seeds")


if __name__ == "__main__":
    main()
