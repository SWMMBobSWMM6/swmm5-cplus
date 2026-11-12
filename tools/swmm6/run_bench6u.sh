#!/bin/bash
# CPU time of SWMM6 at db4634a against EPA 5.2.4, with c40580c as first built (A) and without FP
# contraction (B), on the eight speed-study models and the five larger networks. Same method as
# bench_swmm6.json and bench_large.json: interleaved, three repetitions, EPA timed against itself.
cd /home/claude/swmm6cmp
P=/home/claude/swmm5-cplus/tools/perf; S=/home/claude/swmm6src
REF=/home/claude/perf/v_o2/runswmm
V="--var ref2=$REF --var cplus_lto=/home/claude/swmm6cmp/cp/runswmm_cp --var cplus_pgo=/home/claude/perf/cp_r4/use/runswmm_cp \
   --var swmm6_new_A=$S/build/src/cli/openswmm --var swmm6_new_B=$S/build_B/src/cli/openswmm \
   --var swmm6_new_C=$S/build_C/src/cli/openswmm --var swmm6_legacy_C=$S/build_C/src/legacy/cli/openswmm-legacy"
SWMM_CORPUS=/home/claude/corpus1729 python3 $P/bench_variants.py $P/models.txt bench6u_small.json --ref $REF $V --reps 3 > bench6u_small.log 2>&1
SWMM_CORPUS=/home/claude/corpus1729 python3 $P/bench_variants.py models_large.txt bench6u_large.json --ref $REF $V --reps 3 > bench6u_large.log 2>&1
python3 - <<'PY'
import json
o = json.load(open('/home/claude/swmm5-cplus/results/swmm6/bench_large.json'))
n = json.load(open('bench6u_large.json'))
n['links'] = o['links']; n['links_counted_as'] = o['links_counted_as']
json.dump(n, open('bench6u_large.json', 'w'), indent=1)
PY
echo BENCH DONE
