#!/bin/bash
# callgrind every model in models.txt with the profiling build; two at a time
C=${SWMM_CORPUS:-corpus1729}; P=${PERF_DIR:-$PWD}
mkdir -p $P/cg
i=0
while IFS= read -r m; do
  i=$((i+1)); d=$(dirname "$C/$m")
  ( cd "$d" && valgrind --tool=callgrind --callgrind-out-file=$P/cg/m$i.cg $P/prof/runswmm "$C/$m" /tmp/claude-0/cg_$i.rpt /tmp/claude-0/cg_$i.out > $P/cg/m$i.log 2>&1 ; rm -f /tmp/claude-0/cg_$i.out ) &
  while [ $(jobs -r | wc -l) -ge 2 ]; do wait -n; done
done < $P/models.txt
wait
echo done
