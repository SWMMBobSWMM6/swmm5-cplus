#!/usr/bin/env python3
"""The four-engine gate again, on a newer SWMM6, with the build effect separated from the code effect.

  cmp6u.py LIST RESULTS.jsonl --epa EXE --cplus EXE
           --legacyA EXE --newA EXE   (SWMM6 at the old commit, as first built)
           --legacyB EXE --newB EXE   (the same commit, rebuilt with -ffp-contract=off)
           --legacyC EXE --newC EXE   (SWMM6 at the new commit)
           --prev OLD.jsonl[,OLD2.jsonl...] [--corpus DIR] [--jobs N] [--timeout S]

Per model: E (EPA 5.2.4), C (swmm5-cplus), LC and LC2, NC and NC2 (the new commit, each twice:
the null-effect controls), LB and NB (the old commit without contraction). A is not rerun when
its output hash, recorded in the old gate rows (--prev), equals B's: identical bytes need no
second look. Where B's hash differs from A's, A is run again so the two can be compared value by
value. Outputs are compared with cmp4.js. Resumable: models already in RESULTS are skipped.
"""
import os, sys, json, gzip, time, argparse, subprocess, tempfile, shutil, hashlib, resource
from concurrent.futures import ThreadPoolExecutor, as_completed

ap = argparse.ArgumentParser()
ap.add_argument('list'); ap.add_argument('results')
for k in ('epa', 'cplus', 'legacyA', 'newA', 'legacyB', 'newB', 'legacyC', 'newC'):
    ap.add_argument('--' + k, required=True)
ap.add_argument('--prev', required=True)
ap.add_argument('--corpus', default=os.environ.get('SWMM_CORPUS', '/home/claude/corpus1729'))
ap.add_argument('--jobs', type=int, default=2)
ap.add_argument('--timeout', type=float, default=300)
ap.add_argument('--scratch', default=os.environ.get('CMP4_SCRATCH', '/tmp/claude-0/cmp6u'))
a = ap.parse_args()
HERE = os.path.dirname(os.path.abspath(__file__))
X = {k: os.path.abspath(getattr(a, k)) for k in ('epa', 'cplus', 'legacyA', 'newA', 'legacyB', 'newB', 'legacyC', 'newC')}
ALWAYS = [('E', X['epa']), ('C', X['cplus']), ('LC', X['legacyC']), ('LC2', X['legacyC']),
          ('NC', X['newC']), ('NC2', X['newC']), ('LB', X['legacyB']), ('NB', X['newB'])]
os.makedirs(a.scratch, exist_ok=True)
BIG = 400 << 20     # outputs above this are hashed, then deleted; their pairs are compared by hash only

prev = {}
for p in a.prev.split(','):
    op = gzip.open if p.endswith('.gz') else open
    for l in op(p, 'rt'):
        r = json.loads(l)
        prev[r['inp']] = {k: v.get('out_sha') for k, v in r.get('runs', {}).items()}   # later files override

def limits():
    resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))
    resource.setrlimit(resource.RLIMIT_FSIZE, (2 << 30, 2 << 30))

def run(exe, inp, d):
    os.makedirs(d, exist_ok=True)
    rpt, out = os.path.join(d, 'm.rpt'), os.path.join(d, 'm.out')
    t0 = time.perf_counter()
    p = subprocess.Popen([exe, inp, rpt, out], cwd=os.path.dirname(inp), stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, preexec_fn=limits)
    try:
        _, err = p.communicate(timeout=a.timeout); rc = p.returncode
    except subprocess.TimeoutExpired:
        p.kill(); p.communicate(); rc = 'timeout'; err = b''
    wall = time.perf_counter() - t0
    h = hashlib.sha256()
    if os.path.exists(out):
        with open(out, 'rb') as f:
            for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    size = os.path.getsize(out) if os.path.exists(out) else 0
    big = size > BIG
    if big: os.remove(out)        # ten runs of a 1,000-year model would not fit on disk; compared by hash
    return dict(rc=rc, wall=round(wall, 4), out_bytes=size, out_sha=h.hexdigest()[:16],
                stderr=err.decode('latin-1')[-200:] if err else '', **({'deleted_big': True} if big else {}))

def one(m):
    inp = os.path.join(a.corpus, m)
    work = tempfile.mkdtemp(dir=a.scratch)
    try:
        runs, files = {}, {}
        def go(k, exe):
            d = os.path.join(work, k); runs[k] = run(exe, inp, d)
            files[k] = {'out': os.path.join(d, 'm.out'), 'rpt': os.path.join(d, 'm.rpt')}
        for k, exe in ALWAYS: go(k, exe)
        old = prev.get(m, {})
        # A only where B's bytes differ from what A wrote in the old gate (or A has no record)
        for kb, ka, exe in (('NB', 'NA', X['newA']), ('LB', 'LA', X['legacyA'])):
            if old.get(ka[0]) != runs[kb]['out_sha']: go(ka, exe)
        pairs = [['E', 'C'], ['LC', 'LC2'], ['NC', 'NC2'], ['E', 'LC'], ['LC', 'NC'], ['E', 'NC'],
                 ['E', 'LB'], ['E', 'NB'], ['LB', 'NB'], ['LB', 'LC'], ['NB', 'NC']]
        move = [['E', 'LC'], ['LC', 'NC'], ['E', 'NC'], ['E', 'LB'], ['E', 'NB'], ['LB', 'NB'], ['LB', 'LC'], ['NB', 'NC']]
        if 'NA' in runs: pairs.append(['NA', 'NB']); move.append(['NA', 'NB'])
        if 'LA' in runs: pairs.append(['LA', 'LB']); move.append(['LA', 'LB'])
        spec = {'runs': files, 'pairs': pairs, 'move': move}
        sp = os.path.join(work, 'spec.json'); json.dump(spec, open(sp, 'w'))
        r = subprocess.run(['node', '--max-old-space-size=3072', os.path.join(HERE, 'cmp4.js'), sp],
                           capture_output=True, text=True, timeout=1800)
        try: cmp = json.loads(r.stdout)
        except Exception: cmp = {'error': (r.stderr or '')[-300:]}
        return dict(inp=m, runs=runs, prevA={'L': old.get('L'), 'N': old.get('N')}, **cmp)
    finally:
        shutil.rmtree(work, ignore_errors=True)

models = [l.rstrip('\n') for l in open(a.list) if l.strip()]
done = set()
if os.path.exists(a.results):
    for l in open(a.results):
        try:
            r = json.loads(l)
            if 'runs' in r: done.add(r['inp'])      # a row that failed (e.g. disk full) runs again
        except Exception: pass
todo = [m for m in models if m not in done]
print(f'{len(todo)} to run, {len(done)} already done', flush=True)
with open(a.results, 'a') as fo, ThreadPoolExecutor(a.jobs) as ex:
    futs = {ex.submit(one, m): m for m in todo}
    for i, f in enumerate(as_completed(futs), 1):
        try: row = f.result()
        except Exception as e: row = dict(inp=futs[f], error=repr(e)[:300])
        fo.write(json.dumps(row) + '\n'); fo.flush()
        if i % 25 == 0 or i == len(todo): print(f'{i}/{len(todo)}', flush=True)
