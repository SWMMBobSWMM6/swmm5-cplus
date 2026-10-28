#!/bin/bash
# native build of swmm5-cplus: C++ engine + api, EPA's own main.c compiled as C
set -e
ENG=$1; API=$2; OUT=$3; EXTRA=${4:-"-O2"}
mkdir -p $OUT/obj
pids=()
for f in $ENG/*.cpp $API/api.cpp; do
  o=$OUT/obj/$(basename ${f%.cpp}).o
  ( g++ -std=c++17 $EXTRA -w -I$ENG -I$ENG/include -I$API -c $f -o $o ) &
  pids+=($!)
  if [ ${#pids[@]} -ge 2 ]; then wait ${pids[0]}; pids=("${pids[@]:1}"); fi
done
for p in "${pids[@]}"; do wait $p; done
MAINC=${MAINC:-../epa524/src/run/main.c}
gcc -O2 -w -I$ENG/include -c $MAINC -o $OUT/obj/main_c.o
g++ $EXTRA -o $OUT/runswmm_cp $OUT/obj/*.o -lm -lpthread
g++ -std=c++17 $EXTRA -I$API -o $OUT/multi_native tests/multi_native.cpp $(ls $OUT/obj/*.o | grep -v main_c) -lm -lpthread
