#!/usr/bin/env bash
# Task 3: y = y OP x for 4 operations x 6 data types, 16 tasklets, 1 DPU, 1 MB per vector.
# Output: results/task3.csv  (instructions per element = total / number of elements)
cd "$(dirname "$0")/../task3" || exit 1
mkdir -p ../results
OUT=../results/task3.csv
echo "op,type,elements,instructions,instr_per_element,correct" >> /dev/null
echo "op,type,elements,instructions,instr_per_element,correct" > $OUT
BYTES=1048576
for OP in ADD SUB MUL DIV; do
  for TYPE in CHAR SHORT INT32 INT64 FLOAT DOUBLE; do
    case $TYPE in CHAR) S=1;; SHORT) S=2;; INT32|FLOAT) S=4;; INT64|DOUBLE) S=8;; esac
    N=$(( BYTES / S ))
    make clean > /dev/null
    NR_DPUS=1 NR_TASKLETS=16 BLOCK=9 TYPE=$TYPE OP=$OP PERF=INSTRUCTIONS make -s > /dev/null 2>&1 || { echo "build failed"; exit 1; }
    RUN=$(./bin/host_code -w 0 -e 1 -i $N)
    INS=$(echo "$RUN" | grep "DPU instructions" | awk '{print $NF}')
    OK=$(echo "$RUN" | grep -q "Outputs are equal" && echo yes || echo NO)
    PER=$(awk -v a=$INS -v n=$N 'BEGIN{printf "%.2f", a/n}')
    echo "$OP,$TYPE,$N,$INS,$PER,$OK" >> $OUT
    echo "$OP $TYPE  instr/element=$PER  correct=$OK"
  done
done
echo "Saved $OUT"
