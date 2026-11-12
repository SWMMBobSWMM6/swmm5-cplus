#!/bin/bash
# The five larger networks again after the container restart: EPA against itself, swmm5-cplus, and SWMM6 at
# db4634a (both engines). The contraction pricing (builds A and B) is on the eight small models.
cd /home/claude/swmm6cmp
P=/home/claude/swmm5-cplus/tools/perf; S=/home/claude/swmm6src
REF=/home/claude/perf/v_o2/runswmm
SWMM_CORPUS=/home/claude/corpus1729 python3 $P/bench_variants.py models_large.txt bench6u_large.json --ref $REF \
  --var ref2=$REF --var cplus_lto=/home/claude/swmm6cmp/cp/runswmm_cp \
  --var swmm6_new_C=$S/build_C/src/cli/openswmm --var swmm6_legacy_C=$S/build_C/src/legacy/cli/openswmm-legacy \
  --reps 3 > bench6u_large.log 2>&1
python3 - <<'PY'
import json
o = json.load(open('/home/claude/swmm5-cplus/results/swmm6/bench_large.json'))
n = json.load(open('bench6u_large.json'))
n['links'] = o['links']; n['links_counted_as'] = o['links_counted_as']
json.dump(n, open('bench6u_large.json', 'w'), indent=1)
PY
echo BENCH DONE
