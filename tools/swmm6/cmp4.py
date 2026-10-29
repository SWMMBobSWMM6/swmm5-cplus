#!/usr/bin/env python3
"""Four engines on the same models: EPA 5.2.4, swmm5-cplus, SWMM6's legacy engine, SWMM6's new engine.

  cmp4.py LIST RESULTS.jsonl --epa EXE --cplus EXE --legacy EXE --new EXE [--corpus DIR] [--jobs N] [--timeout S]

Per model, six runs: E (EPA), C (swmm5-cplus), L and L2 (SWMM6 legacy, twice), N and N2 (SWMM6
new, twice). The repeats are the null-effect control: an engine that does not reproduce itself
cannot be compared with anything. Wall time is recorded but runs share the machine, so speed is
measured separately (bench_variants.py, interleaved, one run at a time). The .out files are then
compared value by value (cmp4.js): every reporting period's date and every float. The .out
header's version number is not a result and is not compared. Pairs that differ are measured
element by element against the first engine of the pair.

Resumable: models already in RESULTS are skipped.
"""
import os, sys, json, time, argparse, subprocess, tempfile, shutil, hashlib, resource
from concurrent.futures import ThreadPoolExecutor, as_completed

ap = argparse.ArgumentParser()
ap.add_argument('list'); ap.add_argument('results')
for k in ('epa', 'cplus', 'legacy', 'new'): ap.add_argument('--' + k, required=True)
ap.add_argument('--corpus', default=os.environ.get('SWMM_CORPUS', '/home/claude/corpus1729'))
ap.add_argument('--jobs', type=int, default=2)
ap.add_argument('--timeout', type=float, default=300)
ap.add_argument('--scratch', default=os.environ.get('CMP4_SCRATCH', '/tmp/claude-0/cmp4'))
ap.add_argument('--keep', help='folder to keep the outputs of models where a control pair differs')
a = ap.parse_args()
HERE = os.path.dirname(os.path.abspath(__file__))
ENG = {'E': a.epa, 'C': a.cplus, 'L': a.legacy, 'L2': a.legacy, 'N': a.new, 'N2': a.new}
ENG = {k: os.path.abspath(v) for k, v in ENG.items()}
PAIRS = [['E', 'C'], ['L', 'L2'], ['N', 'N2'], ['E', 'L'], ['L', 'N'], ['E', 'N'], ['C', 'N']]
MOVE = [['E', 'L'], ['L', 'N'], ['E', 'N']]
os.makedirs(a.scratch, exist_ok=True)

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
        _, err = p.communicate(timeout=a.timeout)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        p.kill(); p.communicate(); rc = 'timeout'; err = b''
    wall = time.perf_counter() - t0
    h = hashlib.sha256()
    if os.path.exists(out):
        with open(out, 'rb') as f:
            for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return dict(rc=rc, wall=round(wall, 4), out_bytes=os.path.getsize(out) if os.path.exists(out) else 0,
                out_sha=h.hexdigest()[:16], stderr=err.decode('latin-1')[-200:] if err else '')

def one(m):
    inp = os.path.join(a.corpus, m)
    work = tempfile.mkdtemp(dir=a.scratch)
    try:
        runs, spec = {}, {'runs': {}, 'pairs': PAIRS, 'move': MOVE}
        for k, exe in ENG.items():
            d = os.path.join(work, k)
            runs[k] = run(exe, inp, d)
            spec['runs'][k] = {'out': os.path.join(d, 'm.out'), 'rpt': os.path.join(d, 'm.rpt')}
        sp = os.path.join(work, 'spec.json'); json.dump(spec, open(sp, 'w'))
        r = subprocess.run(['node', '--max-old-space-size=3072', os.path.join(HERE, 'cmp4.js'), sp], capture_output=True, text=True, timeout=1800)
        try: cmp = json.loads(r.stdout)
        except Exception: cmp = {'error': (r.stderr or '')[-300:]}
        row = dict(inp=m, runs=runs, **cmp)
        # keep the files only when a control fails: an engine that did not reproduce itself, or
        # swmm5-cplus not matching EPA. Any other pair can be rerun on demand.
        if a.keep and any(cmp.get('pairs', {}).get(k, {}).get('status') == 'differs' for k in ('E|C', 'L|L2', 'N|N2')):
            dst = os.path.join(a.keep, m.replace('/', '__'))
            if not os.path.exists(dst): shutil.copytree(work, dst, ignore=shutil.ignore_patterns('spec.json'))
        return row
    finally:
        shutil.rmtree(work, ignore_errors=True)

models = [l.rstrip('\n') for l in open(a.list) if l.strip()]
done = set()
if os.path.exists(a.results):
    for l in open(a.results):
        try: done.add(json.loads(l)['inp'])
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
