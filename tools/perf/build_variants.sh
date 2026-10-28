#!/bin/bash
# compile flag variants of EPA 5.2.4 C. Ubuntu's gcc adds _FORTIFY_SOURCE=3, stack protector,
# stack-clash protection and CET by default; "plain" turns those off.
set -e
cd "$(dirname "$0")"
PLAIN="-U_FORTIFY_SOURCE -fno-stack-protector -fno-stack-clash-protection -fcf-protection=none"
./build_epa.sh v_o2        "-O2"
./build_epa.sh v_o2plain   "-O2 $PLAIN"
./build_epa.sh v_o3        "-O3 $PLAIN"
./build_epa.sh v_lto       "-O2 $PLAIN -flto=2" "-flto=2"
./build_epa.sh v_o3native  "-O3 $PLAIN -march=native -ffp-contract=off"
./build_epa.sh v_fma       "-O3 $PLAIN -march=native"
./build_epa.sh v_nomerrno  "-O2 $PLAIN -fno-math-errno"
./build_epa.sh v_fastmath  "-O3 $PLAIN -ffast-math"
./build_epa.sh v_omp       "-O2 $PLAIN -fopenmp" "-fopenmp"
echo built
