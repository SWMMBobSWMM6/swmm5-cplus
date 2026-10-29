#!/usr/bin/env python3
"""Two engines on a list of models, compared value by value with cmp4.js (an attribution test).

  pair.py LIST RESULTS.jsonl A=EXE B=EXE [--corpus DIR] [--jobs N] [--timeout S]

Used to test a cause: patch one engine with the single change suspected of explaining a
difference, and see whether the difference goes away.
"""
import os, sys, json, argparse, subprocess, tempfile, shutil
from concurrent.futures import ThreadPoolExecutor, as_completed

ap = argparse.ArgumentParser()
ap.add_argument('list'); ap.add_argument('results'); ap.add_argument('a'); ap.add_argument('b')
ap.add_argument('--corpus', default=os.environ.get('SWMM_CORPUS', '/home/claude/corpus1729'))
ap.add_argument('--jobs', type=int, default=2); ap.add_argument('--timeout', type=float, default=300)
x = ap.parse_args()
HERE = os.path.dirname(os.path.abspath(__file__))
ENG = dict(v.split('=', 1) for v in (x.a, x.b)); ENG = {k: os.path.abspath(v) for k, v in ENG.items()}
A, B = list(ENG)

def one(m):
    inp = os.path.join(x.corpus, m); work = tempfile.mkdtemp(dir='/tmp/claude-0')
    try:
        spec = {'runs': {}, 'pairs': [[A, B]], 'move': [[A, B]]}; rc = {}
        for k, exe in ENG.items():
            d = os.path.join(work, k); os.makedirs(d)
            try:
                rc[k] = subprocess.run([exe, inp, d + '/m.rpt', d + '/m.out'], cwd=os.path.dirname(inp), capture_output=True, timeout=x.timeout).returncode
            except subprocess.TimeoutExpired:
                rc[k] = 'timeout'
            spec['runs'][k] = {'out': d + '/m.out', 'rpt': d + '/m.rpt'}
        json.dump(spec, open(work + '/spec.json', 'w'))
        r = subprocess.run(['node', os.path.join(HERE, 'cmp4.js'), work + '/spec.json'], capture_output=True, text=True)
        try: cmp = json.loads(r.stdout)
        except Exception: cmp = {'error': r.stderr[-300:]}
        return dict(inp=m, rc=rc, **cmp)
    finally:
        shutil.rmtree(work, ignore_errors=True)

models = [l.rstrip('\n') for l in open(x.list) if l.strip()]
with open(x.results, 'w') as fo, ThreadPoolExecutor(x.jobs) as ex:
    for f in as_completed([ex.submit(one, m) for m in models]):
        fo.write(json.dumps(f.result()) + '\n'); fo.flush()
