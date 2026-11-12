#!/usr/bin/env bash
# Task 4: 4 reduction versions x vector sizes (16 KB, 1 MB, 16 MB per DPU) x tasklets, 1 DPU, int64.
# Records both instructions and cycles (two builds per configuration).
# Output: results/task4.csv
cd "$(dirname "$0")/../task4" || exit 1
mkdir -p ../results
OUT=../results/task4.csv
echo "version,size_bytes,tasklets,instructions,cycles,correct" > $OUT
SIZES="16384 1048576 16777216"
for V in SINGLE BARRIER HANDSHAKE MUTEX; do
  for T in 1 2 4 8 11 16 24; do
    declare -A INS CYC OK
    for PERF in INSTRUCTIONS CYCLES; do
      make clean > /dev/null
      NR_DPUS=1 NR_TASKLETS=$T BLOCK=9 TYPE=INT64 VERSION=$V PERF=$PERF make -s > /dev/null 2>&1 || { echo "build failed"; exit 1; }
      for BYTES in $SIZES; do
        R=$(./bin/host_code -i $(( BYTES / 8 )))
        VAL=$(echo "$R" | grep -E "DPU (instructions|cycles)" | awk '{print $NF}')
        if [ $PERF = INSTRUCTIONS ]; then INS[$BYTES]=$VAL; else CYC[$BYTES]=$VAL; fi
        OK[$BYTES]=$(echo "$R" | grep -q "Outputs are equal" && echo yes || echo NO)
      done
    done
    for BYTES in $SIZES; do
      echo "$V,$BYTES,$T,${INS[$BYTES]},${CYC[$BYTES]},${OK[$BYTES]}" >> $OUT
      echo "$V size=$BYTES tasklets=$T instr=${INS[$BYTES]} cycles=${CYC[$BYTES]} correct=${OK[$BYTES]}"
    done
    unset INS CYC OK
  done
done
echo "Saved $OUT"
