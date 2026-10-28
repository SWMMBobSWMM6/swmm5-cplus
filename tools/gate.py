#!/usr/bin/env python3
"""Bit-identity gate: run every .inp through two SWMM executables and compare.

  gate.py REF_EXE TEST_EXE CORPUS_DIR RESULTS.jsonl [--timeout S] [--jobs N] [--list FILE]

For each model: .out compared byte for byte; .rpt compared after dropping the three
wall-clock lines (begun / ended / elapsed). Resumable: models already in RESULTS are
skipped.
"""
import sys, os, subprocess, hashlib, json, time, tempfile, argparse, shutil, resource
from concurrent.futures import ThreadPoolExecutor, as_completed

ap = argparse.ArgumentParser()
ap.add_argument('ref'); ap.add_argument('test'); ap.add_argument('corpus'); ap.add_argument('results')
ap.add_argument('--timeout', type=float, default=60)
ap.add_argument('--jobs', type=int, default=2)
ap.add_argument('--list')
a = ap.parse_args()
a.ref = os.path.abspath(a.ref); a.test = os.path.abspath(a.test); a.corpus = os.path.abspath(a.corpus)

CLOCK = (b'Analysis begun on', b'Analysis ended on', b'Total elapsed time')

def norm_rpt(b):
    return b'\n'.join(l for l in b.split(b'\n') if not any(c in l for c in CLOCK))

def sha(b):
    return hashlib.sha256(b).hexdigest()[:16]

def limits():
    resource.setrlimit(resource.RLIMIT_AS, (3 << 30, 3 << 30))      # 3 GiB address space
    resource.setrlimit(resource.RLIMIT_FSIZE, (2 << 30, 2 << 30))   # 2 GiB per file

def file_digest(path, normalize=False):
    # streamed: a 1000-year simulation's files never have to fit in memory
    if not os.path.exists(path):
        return hashlib.sha256(b'').hexdigest(), 0, b''
    h = hashlib.sha256(); n = 0; head = b''
    with open(path, 'rb') as f:
        if normalize:
            for line in f:
                n += len(line)
                if any(c in line for c in CLOCK):
                    continue
                h.update(line)
                if len(head) < 400000: head += line
        else:
            while True:
                b = f.read(1 << 20)
                if not b: break
                n += len(b); h.update(b)
    return h.hexdigest(), n, head

def run(exe, inp, work):
    rpt = os.path.join(work, 'm.rpt'); out = os.path.join(work, 'm.out')
    t0 = time.perf_counter()
    try:
        p = subprocess.run([exe, inp, rpt, out], cwd=os.path.dirname(inp),
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=a.timeout, preexec_fn=limits)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        rc = 'timeout'
    dt = time.perf_counter() - t0
    r = file_digest(rpt, normalize=True)
    o = file_digest(out)
    for fpath in (rpt, out):
        try: os.remove(fpath)
        except OSError: pass
    return rc, dt, r, o

def one(inp):
    with tempfile.TemporaryDirectory(dir='/tmp/claude-0') as d:
        wr = os.path.join(d, 'ref'); wt = os.path.join(d, 'test')
        os.makedirs(wr); os.makedirs(wt)
        rc1, t1, r1, o1 = run(a.ref, inp, wr)
        rc2, t2, r2, o2 = run(a.test, inp, wt)
    rec = dict(inp=os.path.relpath(inp, a.corpus), rc_ref=rc1, rc_test=rc2,
               t_ref=round(t1, 4), t_test=round(t2, 4),
               out_bytes=o1[1], out_same=(o1[0] == o2[0] and o1[1] == o2[1]), out_sha=o1[0][:16],
               rpt_same=(r1[0] == r2[0]),
               rpt_err=(b'ERROR' in r1[2]), rpt_bytes=r1[1])
    if not rec['rpt_same']:
        l1 = r1[2].split(b'\n'); l2 = r2[2].split(b'\n')
        for i, (x, y) in enumerate(zip(l1, l2)):
            if x != y:
                rec['rpt_first_diff'] = [i, x.decode('latin-1')[:120], y.decode('latin-1')[:120]]
                break
    return rec

if a.list:
    models = [l.strip() for l in open(a.list) if l.strip()]
    models = [os.path.abspath(m) for m in models]
else:
    models = []
    for root, dirs, files in os.walk(a.corpus):
        dirs[:] = [d for d in dirs if d != '.git']
        for f in files:
            if f.lower().endswith('.inp'):
                models.append(os.path.join(root, f))
    models.sort()

done = set()
if os.path.exists(a.results):
    for l in open(a.results):
        try: done.add(json.loads(l)['inp'])
        except Exception: pass
todo = [m for m in models if os.path.relpath(m, a.corpus) not in done]
print(f'{len(models)} models, {len(done)} done, {len(todo)} to run', flush=True)
with open(a.results, 'a') as fo, ThreadPoolExecutor(a.jobs) as ex:
    futs = {ex.submit(one, m): m for m in todo}
    n = 0
    for fu in as_completed(futs):
        rec = fu.result()
        fo.write(json.dumps(rec) + '\n'); fo.flush()
        n += 1
        if not (rec['out_same'] and rec['rpt_same']):
            print('DIFF', rec['inp'], rec.get('rpt_first_diff'), flush=True)
        if n % 100 == 0:
            print(f'{n}/{len(todo)}', flush=True)
