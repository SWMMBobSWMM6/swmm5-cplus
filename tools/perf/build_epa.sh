#!/bin/bash
# build EPA 5.2.4 C (solver + runswmm main) with given flags: build_epa.sh OUT "CFLAGS" ["LDFLAGS"]
set -e
OUT=$1; CF=$2; LF=${3:-}
EPA=${EPA_SRC:-../epa524/src}; S=${SRC:-$EPA/solver}; M=$EPA/run/main.c
mkdir -p $OUT/obj
for f in $S/*.c $M; do
  o=$OUT/obj/$(basename ${f%.c}).o
  gcc $CF -w -I$S -I$S/include -c $f -o $o &
  while [ $(jobs -r | wc -l) -ge 2 ]; do wait -n; done
done
wait
gcc $CF $LF -o $OUT/runswmm $OUT/obj/*.o -lm
