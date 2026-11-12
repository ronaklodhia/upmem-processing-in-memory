#!/usr/bin/env python3
"""plot.py -- draws the figures in plots/ from the CSV files in results/.

    python3 scripts/plot.py          (needs matplotlib:  pip3 install matplotlib)
"""
import csv
import os

import matplotlib
matplotlib.use("Agg")                      # draw to files; no window needed
import matplotlib.pyplot as plt

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RES, PLOTS = os.path.join(ROOT, "results"), os.path.join(ROOT, "plots")
os.makedirs(PLOTS, exist_ok=True)

METHOD_COLOUR = {"LIGHT": "#4477AA", "AVG": "#228833", "LUM": "#CC6677"}


def rows(name):
    path = os.path.join(RES, name)
    if not os.path.exists(path):
        print("skip (missing):", name)
        return []
    with open(path) as f:
        return [r for r in csv.DictReader(f) if r["correct"] == "yes"]


def colour(version):
    return METHOD_COLOUR[version.split("_")[0]]


# ---- 1. every version: instructions per pixel (log scale) ----
r = [x for x in rows("versions.csv") if x["image"] == "test"]
if r:
    versions = []
    for x in r:
        if x["version"] not in versions:
            versions.append(x["version"])
    val = {(x["version"], x["unroll"]): float(x["instr_per_pixel"]) for x in r}
    fig, ax = plt.subplots(figsize=(9, 4.8))
    ys = range(len(versions))
    for k, v in enumerate(versions):
        ax.barh(k - 0.2, val[(v, "1")], 0.38, color=colour(v), alpha=0.45)
        ax.barh(k + 0.2, val[(v, "8")], 0.38, color=colour(v))
        ax.text(val[(v, "1")] * 1.06, k - 0.2, "%.1f" % val[(v, "1")], va="center", fontsize=8)
        ax.text(val[(v, "8")] * 1.06, k + 0.2, "%.1f" % val[(v, "8")], va="center", fontsize=8)
    ax.set_yticks(list(ys))
    ax.set_yticklabels(versions)
    ax.invert_yaxis()
    ax.set_xscale("log")
    ax.set_xlim(5, 4000)
    ax.set_xlabel("DPU instructions per pixel (log scale); light bar = UNROLL 1, dark bar = UNROLL 8")
    ax.set_title("RGB to grayscale on one DPU, 16 tasklets, 512 x 512 test image")
    ax.grid(axis="x", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "versions.png"), dpi=150)
    plt.close(fig)

# ---- 2. tasklets: total stays flat, per-tasklet share falls as 1/n ----
r = rows("tasklets.csv")
if r:
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4.2))
    for v in ["LIGHT_BASE", "AVG_BASE", "LUM_FLOAT", "LIGHT_TREE", "AVG_LUT", "LUM_MUL8"]:
        pts = sorted((int(x["tasklets"]), float(x["instr_per_pixel"])) for x in r if x["version"] == v)
        if not pts:
            continue
        t, y = zip(*pts)
        style = "--" if v in ("LIGHT_BASE", "AVG_BASE", "LUM_FLOAT") else "-"
        a1.plot(t, y, style, marker="o", ms=3, color=colour(v), label=v)
        a2.plot(t, [yy / tt for tt, yy in pts], style, marker="o", ms=3, color=colour(v), label=v)
    for ax, title in ((a1, "Whole DPU: instructions per pixel"), (a2, "Share of one tasklet: instructions per pixel")):
        ax.set_yscale("log")
        ax.set_xlabel("Number of tasklets")
        ax.set_title(title)
        ax.grid(alpha=0.3)
    a1.legend(fontsize=8, ncol=2)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "tasklets.png"), dpi=150)
    plt.close(fig)

# ---- 3. DPUs: the busiest DPU's work falls as 1/D ----
r = rows("dpus.csv")
if r:
    d = [int(x["dpus"]) for x in r]
    busiest = [float(x["busiest_dpu"]) / 1e6 for x in r]
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.plot(d, busiest, "-o", color=METHOD_COLOUR["LUM"], label="measured (busiest DPU)")
    ax.plot(d, [busiest[0] / k for k in d], ":", color="gray", label="ideal: 1 / number of DPUs")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xticks(d)
    ax.set_xticklabels(d)
    ax.set_xlabel("Number of DPUs")
    ax.set_ylabel("Instructions on the busiest DPU (millions)")
    ax.set_title("4-megapixel image, LUM_MUL8, 16 tasklets")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "dpus.png"), dpi=150)
    plt.close(fig)

# ---- 4. block size ----
r = rows("blocks.csv")
if r:
    b = [int(x["block_pixels"]) for x in r]
    y = [float(x["instr_per_pixel"]) for x in r]
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.plot(b, y, "-o", color=METHOD_COLOUR["LUM"])
    for bb, yy in zip(b, y):
        ax.annotate("%.1f" % yy, (bb, yy), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=8)
    ax.set_xscale("log", base=2)
    ax.set_xticks(b)
    ax.set_xticklabels(b)
    ax.set_ylim(0, max(y) * 1.2)
    ax.set_xlabel("Pixels per MRAM-WRAM block")
    ax.set_ylabel("Instructions per pixel")
    ax.set_title("Block size, LUM_MUL8, UNROLL 8, 11 tasklets")
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(PLOTS, "blocks.png"), dpi=150)
    plt.close(fig)

print("plots written to", os.path.normpath(PLOTS))
