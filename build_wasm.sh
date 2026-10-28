#!/bin/bash
# swmm5-cplus -> wasm32-wasi reactor.  Needs: clang-18, wasi-libc,
# libclang-rt-18-dev-wasm32, libc++-18-dev-wasm32, libc++abi-18-dev-wasm32 (Ubuntu 24.04).
set -e
OUT=${1:-build/wasm}
mkdir -p $OUT/obj
EXPORTS="swmm5cp_create swmm5cp_destroy swmm5cp_count swmm5cp_run swmm5cp_open swmm5cp_start \
swmm5cp_step swmm5cp_stride swmm5cp_end swmm5cp_report swmm5cp_close swmm5cp_getMassBalErr \
swmm5cp_getVersion swmm5cp_getError swmm5cp_getWarnings swmm5cp_getCount swmm5cp_getName \
swmm5cp_getIndex swmm5cp_getValue swmm5cp_setValue swmm5cp_getSavedValue \
swmm_run swmm_open swmm_start swmm_step swmm_end swmm_report swmm_close swmm_getMassBalErr \
swmm_getVersion swmm_getError swmm_getWarnings swmm_getCount swmm_getValue malloc free"
EXP=""; for e in $EXPORTS; do EXP="$EXP -Wl,--export=$e"; done
CXX="clang++ --target=wasm32-wasi --sysroot=/usr -std=c++17 -O2 -flto -fno-exceptions -w -include src/wasm/wasi_shims.h"
for f in src/engine/*.cpp src/api/api.cpp src/wasm/wasm_shims.cpp; do
  o=$OUT/obj/$(basename ${f%.cpp}).o
  $CXX -Isrc/engine -Isrc/engine/include -Isrc/api -c $f -o $o &
  while [ $(jobs -r | wc -l) -ge 2 ]; do wait -n; done
done
wait
clang++ --target=wasm32-wasi --sysroot=/usr -mexec-model=reactor -O2 -flto -fno-exceptions \
  -Wl,--strip-all $EXP -o $OUT/swmm5cplus.wasm $OUT/obj/*.o
ls -la $OUT/swmm5cplus.wasm
