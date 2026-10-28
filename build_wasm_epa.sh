#!/bin/bash
# Reference: EPA SWMM 5.2.4 (C, unmodified) -> wasm32-wasi reactor, same toolchain.
set -e
SRC=${1:-../epa524/src/solver}; OUT=${2:-build/wasm}
mkdir -p $OUT/objc
EXPORTS="swmm_run swmm_open swmm_start swmm_step swmm_end swmm_report swmm_close \
swmm_getMassBalErr swmm_getVersion swmm_getError swmm_getWarnings swmm_getCount swmm_getValue malloc free"
EXP=""; for e in $EXPORTS; do EXP="$EXP -Wl,--export=$e"; done
for f in $SRC/*.c; do
  clang --target=wasm32-wasi --sysroot=/usr -O2 -w -include src/wasm/wasi_shims.h -I$SRC -I$SRC/include \
    -c $f -o $OUT/objc/$(basename ${f%.c}).o &
  while [ $(jobs -r | wc -l) -ge 2 ]; do wait -n; done
done
wait
clang --target=wasm32-wasi --sysroot=/usr -O2 -include src/wasm/wasi_shims.h -c src/wasm/wasm_shims_c.c -o $OUT/objc/zz_shims.o
clang --target=wasm32-wasi --sysroot=/usr -mexec-model=reactor -O2 -Wl,--strip-all $EXP \
  -o $OUT/swmm524_epa.wasm $OUT/objc/*.o
ls -la $OUT/swmm524_epa.wasm
