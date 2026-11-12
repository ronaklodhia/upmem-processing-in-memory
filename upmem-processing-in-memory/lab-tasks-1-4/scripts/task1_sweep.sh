#!/usr/bin/env bash
# Task 1: bandwidth of SERIAL / PARALLEL / BROADCAST transfers
# for 1, 24, 48 MB per DPU and 1..64 DPUs.  Run from the problem/ folder INSIDE the container.
# Output: results/task1.csv
cd "$(dirname "$0")/../task1" || exit 1
mkdir -p ../results
OUT=../results/task1.csv
echo "transfer,dpus,mb_per_dpu,cpu_dpu_MBps,dpu_cpu_MBps" > $OUT

for TRANSFER in SERIAL PARALLEL BROADCAST; do
  for DPUS in 1 2 4 8 16 32 64; do
    make clean > /dev/null                       # the Makefile sometimes keeps an old binary, so always rebuild
    NR_DPUS=$DPUS TRANSFER=$TRANSFER make -s > /dev/null 2>&1 || { echo "build failed"; exit 1; }
    for MB in 1 24 48; do
      ELEMS=$(( MB * 131072 * DPUS ))            # 1 MB = 131072 int64 elements; -i is the TOTAL count
      LINE=$(./bin/host_code -w 1 -e 3 -i $ELEMS 2>&1 | grep RESULT)
      if [ -z "$LINE" ]; then                   # usually "Killed" = Docker ran out of memory
        echo "$TRANSFER,$DPUS,$MB,NA,NA" >> $OUT
        echo "$TRANSFER DPUs=$DPUS ${MB}MB -> FAILED (out of memory?)"
        continue
      fi
      IN=$(echo "$LINE"  | sed -E 's/.*CPU-DPU_MBps=([0-9.]+).*/\1/')
      OUTBW=$(echo "$LINE" | sed -E 's/.*DPU-CPU_MBps=([0-9.]+).*/\1/')
      echo "$TRANSFER,$DPUS,$MB,$IN,$OUTBW" >> $OUT
      echo "$TRANSFER DPUs=$DPUS ${MB}MB -> in $IN MB/s, out $OUTBW MB/s"
    done
  done
done
echo "Saved $OUT"
