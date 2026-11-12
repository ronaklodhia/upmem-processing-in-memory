#!/usr/bin/env bash
# Runs every sweep in order. Takes a while on an Apple Silicon Mac (emulation).
D="$(dirname "$0")"
bash "$D/task1_sweep.sh" && bash "$D/task2_sweep.sh" && bash "$D/task3_sweep.sh" && bash "$D/task4_sweep.sh"
