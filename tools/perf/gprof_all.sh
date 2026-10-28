#!/bin/bash
# run every model in models.txt under the gprof build, one at a time; gmon.out lands in the cwd,
# so each run gets its own scratch cwd copy of nothing but the gmon file via GMON_OUT_PREFIX
C=${SWMM_CORPUS:-corpus1729}; P=${PERF_DIR:-$PWD}
mkdir -p $P/gp
i=0
while IFS= read -r m; do
  i=$((i+1)); d=$(dirname "$C/$m")
  ( cd "$d" && GMON_OUT_PREFIX=$P/gp/m$i $P/prof_pg/runswmm "$C/$m" /tmp/claude-0/gp_$i.rpt /tmp/claude-0/gp_$i.out > /dev/null 2>&1; rm -f /tmp/claude-0/gp_$i.out )
  f=$(ls $P/gp/m$i.* | head -1)
  gprof -b -p $P/prof_pg/runswmm "$f" > $P/gp/m$i.flat.txt 2>/dev/null
  echo "m$i $m $(head -6 $P/gp/m$i.flat.txt | tail -1)"
done < $P/models.txt
echo done
