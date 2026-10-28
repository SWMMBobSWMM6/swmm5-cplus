#!/bin/bash
# Profile-guided native build of swmm5-cplus: instrument, train, rebuild with -O3 -flto.
#   tools/build_pgo.sh OUT TRAIN_LIST [CORPUS_DIR]
# TRAIN_LIST holds .inp paths relative to CORPUS_DIR, one per line; keep it disjoint from any
# models you time afterwards. PGO only changes code layout and inlining, not arithmetic: the
# outputs stay byte-identical (checked by tools/gate.py).
set -e
OUT=$1; LIST=$2; CORPUS=$(cd "${3:-.}" && pwd)
H="-U_FORTIFY_SOURCE -fno-stack-protector -fno-stack-clash-protection -fcf-protection=none"
tools/build_native.sh src/engine src/api $OUT/gen "-O2 $H -fprofile-generate -fprofile-update=single"
EXE=$(cd $OUT/gen && pwd)/runswmm_cp
while IFS= read -r m; do
  [ -z "$m" ] && continue
  t=$(mktemp -d)
  ( cd "$(dirname "$CORPUS/$m")" && "$EXE" "$CORPUS/$m" "$t/m.rpt" "$t/m.out" > /dev/null 2>&1 ) || true
  rm -rf "$t"
done < "$LIST"
mkdir -p $OUT/use/obj && cp $OUT/gen/obj/*.gcda $OUT/use/obj/
tools/build_native.sh src/engine src/api $OUT/use "-O3 $H -flto=2 -fprofile-use -fprofile-correction -Wno-missing-profile"
ls -la $OUT/use/runswmm_cp
