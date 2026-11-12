#!/usr/bin/env bash
# Correctness matrix: every version x loop unrolling x tasklet count x DPU count x image size.
# A run passes when the DPU output equals the CPU result byte for byte.
# Run from the project folder, inside the UPMEM SDK environment:  bash scripts/verify.sh
cd "$(dirname "$0")/.." || exit 1
VERSIONS="LIGHT_BASE LIGHT_SORT LIGHT_TREE AVG_BASE AVG_RECIP AVG_LUT LUM_DOUBLE LUM_FLOAT LUM_FIXED LUM_MUL8"
pass=0; fail=0
for V in $VERSIONS; do
  for U in 1 8; do
    for T in 1 7 11 16 24; do
      make -s VERSION=$V UNROLL=$U NR_TASKLETS=$T > /dev/null 2>&1 || { echo "BUILD FAILED: $V unroll=$U tasklets=$T"; exit 1; }
      for D in 1 3 8; do
        for N in 1 7 1000 12345; do          # odd sizes on purpose: they exercise the padding
          if ./bin/gray_host -n $N -d $D 2>/dev/null | grep -q "^\[OK\]"; then pass=$((pass+1))
          else fail=$((fail+1)); echo "FAIL: $V unroll=$U tasklets=$T dpus=$D pixels=$N"; fi
        done
      done
    done
  done
  echo "$V done (passed so far: $pass, failed: $fail)"
done
echo "TOTAL: $pass passed, $fail failed"
[ $fail -eq 0 ]
