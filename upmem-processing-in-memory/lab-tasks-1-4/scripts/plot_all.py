#!/usr/bin/env python3
"""Makes every plot for the report from results/*.csv -> plots/*.png
Run from the problem/ folder:  python3 scripts/plot_all.py
Needs matplotlib:  pip3 install matplotlib
"""
import csv, os
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")            # draw to files, no window needed
import matplotlib.pyplot as plt

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RES, PLOTS = os.path.join(ROOT, "results"), os.path.join(ROOT, "plots")
os.makedirs(PLOTS, exist_ok=True)

def rows(name):
    path = os.path.join(RES, name)
    if not os.path.exists(path):
        print("skip (missing):", name); return []
    with open(path) as f:
        return list(csv.DictReader(f))

def num(x):
    try: return float(x)
    except (TypeError, ValueError): return None

# ---------- Task 1: bandwidth vs number of DPUs, one panel per transfer size ----------
r1 = rows("task1.csv")
if r1:
    sizes = sorted({int(r["mb_per_dpu"]) for r in r1})
    fig, axes = plt.subplots(1, len(sizes), figsize=(5 * len(sizes), 4), sharey=True)
    for ax, mb in zip(axes, sizes):
        for tr in ["SERIAL", "PARALLEL", "BROADCAST"]:
            for col, direction, style in [("cpu_dpu_MBps", "CPU->DPU", "-o"), ("dpu_cpu_MBps", "DPU->CPU", "--s")]:
                if tr == "BROADCAST" and direction == "DPU->CPU":
                    continue                     # broadcast exists only CPU->DPU
                pts = [(int(r["dpus"]), num(r[col]) / 1024) for r in r1
                       if r["transfer"] == tr and int(r["mb_per_dpu"]) == mb and num(r[col])]
                if pts:
                    x, y = zip(*sorted(pts))
                    ax.plot(x, y, style, label=f"{tr} {direction}")
        ax.set_xscale("log", base=2); ax.set_xticks([1, 2, 4, 8, 16, 32, 64]); ax.set_xticklabels([1, 2, 4, 8, 16, 32, 64])
        ax.set_title(f"{mb} MB per DPU"); ax.set_xlabel("Number of DPUs"); ax.grid(alpha=0.3)
    axes[0].set_ylabel("Bandwidth (GB/s)"); axes[-1].legend(fontsize=8)
    fig.tight_layout(); fig.savefig(os.path.join(PLOTS, "task1_bandwidth.png"), dpi=150); plt.close(fig)

# ---------- Task 2: instructions vs tasklets ----------
r2 = rows("task2.csv")
if r2:
    t = [int(r["tasklets"]) for r in r2]
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(t, [num(r["instructions_per_tasklet"]) / 1e6 for r in r2], "-o", label="per tasklet")
    ax.plot(t, [num(r["instructions_total"]) / 1e6 for r in r2], "--s", label="whole DPU (all tasklets)")
    ax.set_xlabel("Number of tasklets"); ax.set_ylabel("Instructions (millions)")
    ax.set_title("AXPY, 8 MB int32 vectors, 1 DPU"); ax.grid(alpha=0.3); ax.legend()
    fig.tight_layout(); fig.savefig(os.path.join(PLOTS, "task2_instructions.png"), dpi=150); plt.close(fig)

# ---------- Task 3: instructions per element, grouped bars ----------
r3 = rows("task3.csv")
if r3:
    ops = ["ADD", "SUB", "MUL", "DIV"]; types = ["CHAR", "SHORT", "INT32", "INT64", "FLOAT", "DOUBLE"]
    val = {(r["op"], r["type"]): num(r["instr_per_element"]) for r in r3}
    fig, ax = plt.subplots(figsize=(10, 4.5)); w = 0.2
    for k, op in enumerate(ops):
        xs = [i + (k - 1.5) * w for i in range(len(types))]
        ax.bar(xs, [val.get((op, ty), 0) for ty in types], w, label=op)
    ax.set_xticks(range(len(types))); ax.set_xticklabels(types); ax.set_yscale("log")
    ax.set_ylabel("Instructions per element (log scale)"); ax.set_title("y = y OP x, 16 tasklets, 1 MB vectors")
    ax.grid(axis="y", alpha=0.3); ax.legend()
    fig.tight_layout(); fig.savefig(os.path.join(PLOTS, "task3_ops_types.png"), dpi=150); plt.close(fig)

# ---------- Task 4: instructions vs tasklets, one panel per vector size ----------
r4 = rows("task4.csv")
if r4:
    sizes = sorted({int(r["size_bytes"]) for r in r4})
    label = {16384: "16 KB", 1048576: "1 MB", 16777216: "16 MB"}
    fig, axes = plt.subplots(1, len(sizes), figsize=(5 * len(sizes), 4))
    for ax, s in zip(axes, sizes):
        for v in ["SINGLE", "BARRIER", "HANDSHAKE", "MUTEX"]:
            pts = sorted((int(r["tasklets"]), num(r["instructions"])) for r in r4
                         if r["version"] == v and int(r["size_bytes"]) == s and num(r["instructions"]))
            if pts:
                x, y = zip(*pts); ax.plot(x, y, "-o", label=v)
        ax.set_title(f"Vector size {label.get(s, s)}"); ax.set_xlabel("Number of tasklets"); ax.grid(alpha=0.3)
    axes[0].set_ylabel("Instructions"); axes[0].legend(fontsize=8)
    fig.tight_layout(); fig.savefig(os.path.join(PLOTS, "task4_reduction.png"), dpi=150); plt.close(fig)

print("Plots written to", PLOTS)
