#!/usr/bin/env python3
"""
Запускает bench_batch с разными параметрами и рисует графики в images/.
Используется один раз для генерации картинок в README.

Зависимости:
    pip install matplotlib

Запуск из корня проекта:
    make all
    python3 scripts/plot_bench.py
"""

import csv
import io
import os
import subprocess
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH = os.path.join(ROOT, "bin", "bench_batch")
IMG = os.path.join(ROOT, "images")
os.makedirs(IMG, exist_ok=True)


def run_bench(m, n, tmin, tmax, density, runs):
    cmd = [BENCH, str(m), str(n), str(tmin), str(tmax), str(density), str(runs), "--csv"]
    out = subprocess.check_output(cmd, text=True)
    rows = list(csv.DictReader(io.StringIO(out)))
    for r in rows:
        r["avg_ratio"] = float(r["avg_ratio"])
        r["min_ratio"] = float(r["min_ratio"])
        r["max_ratio"] = float(r["max_ratio"])
        r["avg_ms"] = float(r["avg_ms"])
        r["wins"] = int(r["wins"])
        r["runs"] = int(r["runs"])
    return rows


# единая цветовая палитра по алгоритмам
COLORS = {
    "greedy":   "#e15759",
    "lpt":      "#4e79a7",
    "multifit": "#f28e2b",
    "lp":       "#76b7b2",
    "local":    "#59a14f",
    "restart":  "#b07aa1",
}


def plot_ratio(rows, path):
    rows_sorted = sorted(rows, key=lambda r: r["avg_ratio"])
    names = [r["algo"] for r in rows_sorted]
    vals = [r["avg_ratio"] for r in rows_sorted]
    mins = [r["min_ratio"] for r in rows_sorted]
    maxs = [r["max_ratio"] for r in rows_sorted]
    err_lo = [v - m for v, m in zip(vals, mins)]
    err_hi = [M - v for v, M in zip(vals, maxs)]

    fig, ax = plt.subplots(figsize=(9, 5))
    bars = ax.bar(
        names, vals,
        color=[COLORS[n] for n in names],
        edgecolor="black", linewidth=0.5,
    )
    ax.errorbar(
        names, vals, yerr=[err_lo, err_hi],
        fmt="none", ecolor="black", capsize=6, capthick=1, alpha=0.7,
    )
    for bar, v in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, v + 0.001,
                f"{v:.4f}", ha="center", va="bottom", fontsize=10)
    ax.axhline(1.0, ls="--", lw=1, color="grey", alpha=0.6)
    ax.text(len(names) - 0.5, 1.001, "оптимум", color="grey", fontsize=9, va="bottom", ha="right")
    ax.set_ylabel("makespan / lower_bound")
    ax.set_title("Качество: среднее отношение к нижней границе (планки — min/max за 30 инстансов)")
    ax.set_ylim(0.99, max(maxs) * 1.02)
    ax.grid(axis="y", ls=":", alpha=0.5)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_time(rows, path):
    rows_sorted = sorted(rows, key=lambda r: r["avg_ms"])
    names = [r["algo"] for r in rows_sorted]
    vals = [r["avg_ms"] for r in rows_sorted]

    fig, ax = plt.subplots(figsize=(9, 5))
    bars = ax.barh(
        names, vals,
        color=[COLORS[n] for n in names],
        edgecolor="black", linewidth=0.5,
    )
    for bar, v in zip(bars, vals):
        ax.text(v, bar.get_y() + bar.get_height()/2,
                f"  {v:.3f} мс", va="center", fontsize=10)
    ax.set_xlabel("ср. время на инстанс, мс")
    ax.set_title("Скорость: чем короче, тем быстрее")
    ax.grid(axis="x", ls=":", alpha=0.5)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_wins(rows, path):
    rows_sorted = sorted(rows, key=lambda r: -r["wins"])
    names = [r["algo"] for r in rows_sorted]
    vals = [r["wins"] for r in rows_sorted]
    runs = rows_sorted[0]["runs"]

    fig, ax = plt.subplots(figsize=(9, 5))
    bars = ax.bar(
        names, vals,
        color=[COLORS[n] for n in names],
        edgecolor="black", linewidth=0.5,
    )
    for bar, v in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, v + 0.5,
                f"{v}/{runs}", ha="center", va="bottom", fontsize=10)
    ax.set_ylabel(f"побед из {runs} инстансов")
    ax.set_title("Сколько раз алгоритм выдал лучший makespan (возможны ничьи)")
    ax.set_ylim(0, runs * 1.15)
    ax.grid(axis="y", ls=":", alpha=0.5)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_pareto(rows, path):
    fig, ax = plt.subplots(figsize=(9, 6))
    for r in rows:
        ax.scatter(r["avg_ms"], r["avg_ratio"],
                   s=160, color=COLORS[r["algo"]], edgecolor="black", zorder=3)
        ax.annotate(r["algo"],
                    (r["avg_ms"], r["avg_ratio"]),
                    textcoords="offset points", xytext=(8, 8), fontsize=11)
    ax.set_xlabel("ср. время, мс")
    ax.set_ylabel("ср. ratio = makespan / lower_bound")
    ax.set_title("Парето: качество vs время  (← и ↓ = лучше)")
    ax.axhline(1.0, ls="--", lw=1, color="grey", alpha=0.6)
    ax.grid(ls=":", alpha=0.5)
    ax.set_xlim(left=-0.002)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_scaling(path):
    """как меняется качество при росте n (на фиксированном m)"""
    ns = [10, 25, 50, 100, 200, 400]
    series = {a: [] for a in COLORS}
    for n in ns:
        rows = run_bench(5, n, 1, 100, 0.6, 20)
        for r in rows:
            series[r["algo"]].append(r["avg_ratio"])

    fig, ax = plt.subplots(figsize=(9, 5))
    for algo, ys in series.items():
        ax.plot(ns, ys, marker="o", color=COLORS[algo], label=algo, lw=2)
    ax.axhline(1.0, ls="--", lw=1, color="grey", alpha=0.6)
    ax.set_xlabel("n (число заданий)")
    ax.set_ylabel("avg ratio")
    ax.set_title("Как качество ведёт себя при росте n  (m=5, density=0.6, 20 инстансов на точку)")
    ax.legend(loc="upper right", ncol=3, fontsize=9)
    ax.grid(ls=":", alpha=0.5)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    if not os.path.isfile(BENCH):
        print(f"бенч не собран: {BENCH}\nзапусти `make all` сначала", file=sys.stderr)
        sys.exit(1)

    print("==> главный прогон: m=5, n=50, density=0.6, 30 инстансов")
    rows = run_bench(5, 50, 1, 100, 0.6, 30)
    for r in rows:
        print(f"  {r['algo']:>10}: ratio={r['avg_ratio']:.4f}  ms={r['avg_ms']:.3f}  wins={r['wins']}")

    plot_ratio(rows, os.path.join(IMG, "bench_ratio.png"))
    plot_time(rows,  os.path.join(IMG, "bench_time.png"))
    plot_wins(rows,  os.path.join(IMG, "bench_wins.png"))
    plot_pareto(rows, os.path.join(IMG, "bench_pareto.png"))

    print("==> scaling по n …")
    plot_scaling(os.path.join(IMG, "bench_scaling.png"))

    print("\nготово, графики в", IMG)


if __name__ == "__main__":
    main()
