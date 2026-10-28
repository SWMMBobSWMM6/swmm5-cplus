#!/usr/bin/env python3
"""Interleaved timing of engine builds on a model set, with output identity against a reference.

  bench_variants.py MODELS.txt OUT.json --ref EXE --var name=EXE ... [--reps 3] [--threads N]

Each repetition runs every model on every build, in a rotated order so drift falls evenly.
Time is CPU time of the child (user + sys from wait4), which is steadier than wall time on a
shared two-core machine; wall time is recorded too. For each build the .out and the .rpt
(minus the three clock lines) are hashed and compared with the reference build's.
--threads N writes a copy of the .inp with THREADS N into the model's own folder (so relative
data paths still resolve), runs it, and deletes it.
"""
import sys, os, json, time, hashlib, argparse, tempfile, subprocess, statistics, re

ap = argparse.ArgumentParser()
ap.add_argument('models'); ap.add_argument('out')
ap.add_argument('--ref', required=True)
ap.add_argument('--var', action='append', default=[])
ap.add_argument('--reps', type=int, default=3)
ap.add_argument('--threads', type=int, default=0)
ap.add_argument('--corpus', default=os.environ.get('SWMM_CORPUS', 'corpus1729'))
x = ap.parse_args()
CLOCK = (b'Analysis begun on', b'Analysis ended on', b'Total elapsed time')

builds = [('ref', os.path.abspath(x.ref))] + [(v.split('=', 1)[0], os.path.abspath(v.split('=', 1)[1])) for v in x.var]
models = [l.strip() for l in open(x.models) if l.strip()]

def with_threads(inp, n):
    txt = open(inp, encoding='latin-1').read()
    txt = re.sub(r'(?im)^\s*THREADS\s+\S+.*$', '', txt)
    txt = re.sub(r'(?i)\[OPTIONS\]', '[OPTIONS]\nTHREADS              %d' % n, txt, count=1)
    p = os.path.join(os.path.dirname(inp), '_bench_threads_%d.inp' % os.getpid())
    open(p, 'w', encoding='latin-1').write(txt)
    return p

def run(exe, inp):
    with tempfile.TemporaryDirectory(dir='/tmp/claude-0') as d:
        rpt, out = d + '/m.rpt', d + '/m.out'
        t0 = time.perf_counter()
        p = subprocess.Popen([exe, inp, rpt, out], cwd=os.path.dirname(inp), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        _, status, ru = os.wait4(p.pid, 0)
        wall = time.perf_counter() - t0
        h = hashlib.sha256()
        if os.path.exists(out):
            with open(out, 'rb') as f:
                for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
        r = b''
        if os.path.exists(rpt):
            r = b'\n'.join(l for l in open(rpt, 'rb').read().split(b'\n') if not any(c in l for c in CLOCK))
        return dict(cpu=ru.ru_utime + ru.ru_stime, wall=wall, rc=os.waitstatus_to_exitcode(status),
                    out=h.hexdigest()[:16], rpt=hashlib.sha256(r).hexdigest()[:16])

res = {m: {b: [] for b, _ in builds} for m in models}
for rep in range(x.reps):
    for mi, m in enumerate(models):
        inp = os.path.join(x.corpus, m)
        if x.threads: inp = with_threads(inp, x.threads)
        try:
            k = (rep + mi) % len(builds)
            for b, exe in builds[k:] + builds[:k]:
                res[m][b].append(run(exe, inp))
        finally:
            if x.threads: os.remove(inp)
        print(f'rep {rep + 1}/{x.reps} {m}', flush=True)

summary = {}
for b, _ in builds:
    ratios, same = [], 0
    for m in models:
        rr = statistics.median(r['cpu'] for r in res[m]['ref'])
        rb = statistics.median(r['cpu'] for r in res[m][b])
        ratios.append(rb / rr)
        same += (res[m][b][0]['out'] == res[m]['ref'][0]['out'] and res[m][b][0]['rpt'] == res[m]['ref'][0]['rpt'])
    summary[b] = dict(median_ratio=statistics.median(ratios), geomean_ratio=statistics.geometric_mean(ratios),
                      min_ratio=min(ratios), max_ratio=max(ratios), identical=same, models=len(models))
json.dump(dict(builds=builds, reps=x.reps, threads=x.threads, results=res, summary=summary), open(x.out, 'w'), indent=1)
for b, s in summary.items():
    print(f"{b:12s} time x{s['geomean_ratio']:.3f} (median {s['median_ratio']:.3f}, range {s['min_ratio']:.3f}-{s['max_ratio']:.3f})  identical {s['identical']}/{s['models']}")
