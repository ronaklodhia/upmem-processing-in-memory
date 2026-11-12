#!/usr/bin/env bash
# Runs the four experiments and writes one CSV file each into results/.
# Run from the project folder, inside the UPMEM SDK environment:  bash scripts/run_experiments.sh
# Takes about 15 minutes on the simulator (the float and double versions are slow).
cd "$(dirname "$0")/.." || exit 1
mkdir -p results
HEADER="version,unroll,dpus,tasklets,block_pixels,pixels,instructions,instr_per_pixel,busiest_dpu,max_error,mean_error,correct"
ALL="LIGHT_BASE LIGHT_SORT LIGHT_TREE AVG_BASE AVG_RECIP AVG_LUT LUM_DOUBLE LUM_FLOAT LUM_FIXED LUM_MUL8"

run() {   # run <extra csv field> <host arguments...>  -> prints one CSV line
  local tag=$1; shift
  local line; line=$(./bin/gray_host "$@" | grep "^RESULT," | cut -d, -f2-)
  [ -z "$line" ] && line="FAILED,,,,,,,,,,,NO"
  echo "$tag,$line"
}
build() { make -s "$@" > /dev/null 2>&1 || { echo "build failed: $*"; exit 1; }; }

echo "== 1/4  every version, with and without loop unrolling, on two images"
OUT=results/versions.csv; echo "image,$HEADER" > $OUT
for V in $ALL; do for U in 1 8; do
  build VERSION=$V UNROLL=$U NR_TASKLETS=16
  run test   -i images/test.ppm >> $OUT       # the colour test picture (512 x 512)
  run random -n 262144          >> $OUT       # random pixels, same size
  tail -2 $OUT
done; done

echo "== 2/4  tasklets 1..24: the three plain versions and the three best versions"
OUT=results/tasklets.csv; echo "kind,$HEADER" > $OUT
for T in $(seq 1 24); do
  for V in LIGHT_BASE AVG_BASE LUM_FLOAT; do build VERSION=$V UNROLL=1 NR_TASKLETS=$T; run plain -i images/test.ppm >> $OUT; done
  for V in LIGHT_TREE AVG_LUT LUM_MUL8;   do build VERSION=$V UNROLL=8 NR_TASKLETS=$T; run best  -i images/test.ppm >> $OUT; done
  echo "tasklets=$T done"
done

echo "== 3/4  DPUs 1..64: 4-megapixel random image, best luminosity version"
OUT=results/dpus.csv; echo "kind,$HEADER" > $OUT
build VERSION=LUM_MUL8 UNROLL=8 NR_TASKLETS=16
for D in 1 2 4 8 16 32 64; do run best -n 4194304 -d $D >> $OUT; tail -1 $OUT; done

echo "== 4/4  block size 8..512 pixels: best luminosity version, 11 tasklets"
OUT=results/blocks.csv; echo "kind,$HEADER" > $OUT
for B in 3 4 5 6 7 8 9; do
  build VERSION=LUM_MUL8 UNROLL=8 NR_TASKLETS=11 BLOCK=$B
  run best -i images/test.ppm >> $OUT; tail -1 $OUT
done
echo "Saved results/versions.csv tasklets.csv dpus.csv blocks.csv"
