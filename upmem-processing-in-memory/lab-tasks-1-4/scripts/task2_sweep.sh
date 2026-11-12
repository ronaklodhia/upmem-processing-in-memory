#!/usr/bin/env bash
# Task 2: AXPY with 1..24 tasklets, 8 MB per input vector (2,097,152 int32 elements), 1 DPU.
# BLOCK=9 (512-byte blocks) so 24 tasklets fit in 64 KB of WRAM.
# Output: results/task2.csv
cd "$(dirname "$0")/../task2" || exit 1
mkdir -p ../results
OUT=../results/task2.csv
N=2097152
echo "tasklets,instructions_total,instructions_per_tasklet,correct" > $OUT
for T in $(seq 1 24); do
  make clean > /dev/null
  NR_DPUS=1 NR_TASKLETS=$T BLOCK=9 TYPE=INT32 PERF=INSTRUCTIONS make -s > /dev/null 2>&1 || { echo "build failed"; exit 1; }
  RUN=$(./bin/host_code -w 0 -e 1 -i $N -a 3)
  INS=$(echo "$RUN" | grep "DPU instructions" | awk '{print $NF}')
  OK=$(echo "$RUN" | grep -q "Outputs are equal" && echo yes || echo NO)
  PER=$(awk -v a=$INS -v t=$T 'BEGIN{printf "%.0f", a/t}')
  echo "$T,$INS,$PER,$OK" >> $OUT
  echo "tasklets=$T  total=$INS  per-tasklet=$PER  correct=$OK"
done
echo "Saved $OUT"
