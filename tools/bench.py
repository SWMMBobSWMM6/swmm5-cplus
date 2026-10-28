#!/usr/bin/env python3
"""Interleaved timing of two SWMM executables with a noise floor (Corinne check 6).

  bench.py A_EXE B_EXE LIST.txt OUT.json [--reps N]

Each rep runs, for every model, A then B then A' (a second A run) in that order,
so drift affects both engines alike. The noise floor is the spread of A vs A'.
Reports per-model median times and the median B/A ratio with its A'/A floor.
"""
import sys, os, subprocess, time, json, statistics, argparse, tempfile
ap = argparse.ArgumentParser()
ap.add_argument('a'); ap.add_argument('b'); ap.add_argument('lst'); ap.add_argument('out')
ap.add_argument('--reps', type=int, default=5)
x = ap.parse_args()
A = os.path.abspath(x.a); B = os.path.abspath(x.b)
models = [os.path.abspath(l.strip()) for l in open(x.lst) if l.strip()]

def t(exe, inp):
    with tempfile.TemporaryDirectory(dir='/tmp/claude-0') as d:
        t0 = time.perf_counter()
        subprocess.run([exe, inp, d + '/m.rpt', d + '/m.out'], cwd=os.path.dirname(inp),
                       capture_output=True)
        return time.perf_counter() - t0

res = {m: {'a': [], 'b': [], 'a2': []} for m in models}
for rep in range(x.reps):
    for m in models:
        res[m]['a'].append(t(A, m))
        res[m]['b'].append(t(B, m))
        res[m]['a2'].append(t(A, m))
    print('rep', rep + 1, 'done', flush=True)
rows = []
for m in models:
    a = statistics.median(res[m]['a']); b = statistics.median(res[m]['b']); a2 = statistics.median(res[m]['a2'])
    rows.append(dict(model=os.path.basename(m), a=a, b=b, a2=a2, ratio=b / a, floor=a2 / a))
ratios = [r['ratio'] for r in rows]; floors = [r['floor'] for r in rows]
summary = dict(median_ratio=statistics.median(ratios), median_floor=statistics.median(floors),
               max_abs_floor_dev=max(abs(f - 1) for f in floors), reps=x.reps, models=len(models))
json.dump(dict(rows=rows, summary=summary, raw=res), open(x.out, 'w'), indent=1)
for r in rows:
    print(f"{r['model'][:40]:40s} A {r['a']:.3f}s  B {r['b']:.3f}s  B/A {r['ratio']:.3f}  A'/A {r['floor']:.3f}")
print(json.dumps(summary, indent=1))
