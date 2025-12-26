#!/usr/bin/env python3
"""
Plot latency histograms and annotate mean/median.

Usage:
  python3 utils/plot.py data/latencies-*.txt
  python3 utils/plot.py  # plots all files in ./data
"""

import argparse
import statistics
from pathlib import Path

try:
  import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover - runtime dependency
  raise SystemExit("matplotlib is required: pip install matplotlib") from exc

PLOTS_DIR_NAME = "plots"

def read_latencies(path: Path) -> list[float]:
  values: list[float] = []
  with path.open("r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue
      try:
        values.append(float(line))
      except ValueError:
        continue
  return values


def list_latency_files(data_dir: Path) -> list[Path]:
  return sorted(data_dir.glob("latencies-*.txt"))


def plot_hist(ax, values: list[float], title: str, bins: int, log_y: bool) -> None:
  mean = statistics.fmean(values)
  median = statistics.median(values)
  ax.hist(values, bins=bins, color="#2b6cb0", alpha=0.85)
  ax.axvline(mean, color="#d97706", linewidth=1.5, label=f"mean {mean:.2f} ms")
  ax.axvline(median, color="#16a34a", linewidth=1.5, label=f"median {median:.2f} ms")
  ax.set_title(title)
  ax.set_xlabel("Latency (ms)")
  ax.set_ylabel("Count")
  if log_y:
    ax.set_yscale("log")
  ax.legend()


def main() -> int:
  parser = argparse.ArgumentParser(description="Plot latency histograms with mean/median.")
  parser.add_argument("files", nargs="*", help="Latency files (one float per line)")
  parser.add_argument("--bins", type=int, default=50, help="Histogram bins")
  parser.add_argument("--log-y", action="store_true", help="Use log scale for histogram counts")
  parser.add_argument("--save", help="Save figure to path instead of showing")
  args = parser.parse_args()

  if args.files:
    paths = [Path(p) for p in args.files]
  else:
    data_dir = Path(__file__).resolve().parent.parent / "data"
    paths = list_latency_files(data_dir)
    if not paths:
      print(f"No latency files found in {data_dir}")
      return 1

  values_list: list[tuple[Path, list[float]]] = []
  for path in paths:
    values = read_latencies(path)
    if not values:
      print(f"No valid values in {path}")
      continue
    values_list.append((path, values))

  if not values_list:
    return 1

  for path, values in values_list:
    mean = statistics.fmean(values)
    median = statistics.median(values)
    print(f"{path}: count={len(values)} mean={mean:.2f} ms median={median:.2f} ms")

  multiple = len(values_list) > 1
  if args.save:
    base_path = Path(args.save)
  else:
    root = Path(__file__).resolve().parent.parent
    base_path = root / PLOTS_DIR_NAME

  for path, values in values_list:
    fig, ax = plt.subplots(1, 1, figsize=(6, 4))
    plot_hist(ax, values, path.name, args.bins, args.log_y)
    fig.tight_layout()

    if args.save:
      if multiple:
        if base_path.suffix:
          out_dir = base_path.parent
          out_dir.mkdir(parents=True, exist_ok=True)
          save_path = out_dir / f"{base_path.stem}-{path.stem}{base_path.suffix}"
        else:
          base_path.mkdir(parents=True, exist_ok=True)
          save_path = base_path / f"{path.stem}.png"
      else:
        if base_path.suffix:
          base_path.parent.mkdir(parents=True, exist_ok=True)
          save_path = base_path
        else:
          base_path.mkdir(parents=True, exist_ok=True)
          save_path = base_path / f"{path.stem}.png"
    else:
      base_path.mkdir(parents=True, exist_ok=True)
      save_path = base_path / f"{path.stem}.png"

    fig.savefig(save_path, dpi=150)
    plt.close(fig)
    print(f"Saved plot to {save_path}")

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
