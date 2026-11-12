#!/usr/bin/env python3
"""Find the commits that changed SWMM6's results, between two commits, on a set of models.

  bisect6.py REPO_WORKTREE BUILD_DIR OLD NEW MODELS.txt OUT.jsonl [--corpus DIR] [--jobs N] [--timeout S]

Builds SWMM6 (both engines) at commits along OLD..NEW (first parent, only those touching src,
include or the CMake files), runs every model on both
engines, and records the hash of each .out. Starting from OLD and NEW, it builds the commit
halfway between any two tested commits whose hashes differ for some model, until every change
sits between two adjacent commits. A commit that does not build is recorded and skipped over.
Resumable: commits already in OUT are not rebuilt. BUILD_DIR must already be configured
(FP contraction off, same options as the comparison builds).
"""
import os, sys, json, time, argparse, subprocess, hashlib, resource, tempfile, shutil
from concurrent.futures import ThreadPoolExecutor

ap = argparse.ArgumentParser()
ap.add_argument('repo'); ap.add_argument('build'); ap.add_argument('old'); ap.add_argument('new')
ap.add_argument('models'); ap.add_argument('out')
ap.add_argument('--corpus', default='/home/claude/corpus1729')
ap.add_argument('--jobs', type=int, default=2)
ap.add_argument('--timeout', type=float, default=300)
ap.add_argument('--scratch', default='/tmp/claude-0/bisect6')
a = ap.parse_args()
os.makedirs(a.scratch, exist_ok=True)
git = lambda *x: subprocess.run(['git', '-C', a.repo, *x], capture_output=True, text=True, check=True).stdout.strip()
# only commits that touch what is compiled: a commit that changes only docs, tests or Python
# cannot change either engine's output
commits = [a.old] + git('rev-list', '--reverse', '--first-parent', f'{a.old}..{a.new}', '--',
                        'src', 'include', 'CMakeLists.txt', 'cmake').split()
if git('rev-parse', a.new) != git('rev-parse', commits[-1]): commits.append(a.new)
commits = [git('rev-parse', '--short=7', c) for c in commits]
models = [l.strip() for l in open(a.models) if l.strip()]
EXE = {'L': os.path.join(a.build, 'src/legacy/cli/openswmm-legacy'), 'N': os.path.join(a.build, 'src/cli/openswmm')}

def limits():
    resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30)); resource.setrlimit(resource.RLIMIT_FSIZE, (2 << 30, 2 << 30))

def run(k, m):
    inp = os.path.join(a.corpus, m); d = tempfile.mkdtemp(dir=a.scratch)
    try:
        out = os.path.join(d, 'm.out')
        p = subprocess.Popen([EXE[k], inp, os.path.join(d, 'm.rpt'), out], cwd=os.path.dirname(inp),
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, preexec_fn=limits)
        try: p.wait(timeout=a.timeout); rc = p.returncode
        except subprocess.TimeoutExpired: p.kill(); p.wait(); return 'timeout'
        if not os.path.exists(out) or os.path.getsize(out) == 0: return f'none rc={rc}'
        h = hashlib.sha256()
        with open(out, 'rb') as f:
            for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
        return h.hexdigest()[:16]
    finally:
        shutil.rmtree(d, ignore_errors=True)

done = {}
if os.path.exists(a.out):
    for l in open(a.out):
        r = json.loads(l); done[r['commit']] = r

def test(c):
    if c in done: return done[c]
    t0 = time.time()
    # -f: before dc5a1c2 every configure rewrites two tracked export headers (comments only)
    git('checkout', '-q', '-f', '--detach', c)
    b = subprocess.run(['ninja', '-C', a.build, '-j2', 'openswmm', 'openswmm-legacy'], capture_output=True, text=True)
    row = dict(commit=c, subject=git('log', '-1', '--format=%s', c), build_ok=b.returncode == 0,
               build_s=round(time.time() - t0))
    if b.returncode == 0:
        with ThreadPoolExecutor(a.jobs) as ex:
            res = list(ex.map(lambda km: run(*km), [(k, m) for m in models for k in ('L', 'N')]))
        row['L'] = {m: res[2 * i] for i, m in enumerate(models)}
        row['N'] = {m: res[2 * i + 1] for i, m in enumerate(models)}
    else:
        row['error'] = (b.stdout + b.stderr)[-600:]
    row['run_s'] = round(time.time() - t0) - row['build_s']
    with open(a.out, 'a') as f: f.write(json.dumps(row) + '\n')
    done[c] = row
    print(f"{c} build={'ok' if row['build_ok'] else 'FAIL'} {row['build_s']}s run {row['run_s']}s  {row['subject'][:70]}", flush=True)
    return row

def sig(r): return None if not r.get('build_ok') else (r['L'], r['N'])
idx = {c: i for i, c in enumerate(commits)}
test(commits[0]); test(commits[-1])
while True:
    tested = [c for c in commits if c in done and done[c].get('build_ok')]
    todo = None
    for x, y in zip(tested, tested[1:]):
        if sig(done[x]) != sig(done[y]):
            between = [c for c in commits[idx[x] + 1: idx[y]] if c not in done]
            if between: todo = between[len(between) // 2]; break
    if todo is None: break
    test(todo)
# report: each change between adjacent tested commits
tested = [c for c in commits if c in done and done[c].get('build_ok')]
changes = []
for x, y in zip(tested, tested[1:]):
    if sig(done[x]) == sig(done[y]): continue
    skipped = [c for c in commits[idx[x] + 1: idx[y]]]
    for k in ('L', 'N'):
        moved = [m for m in models if done[x][k][m] != done[y][k][m]]
        if moved: changes.append(dict(before=x, after=y, untestable_between=skipped, engine=k, n=len(moved),
                                      subject=done[y]['subject'], models=moved))
json.dump(dict(commits=len(commits), built=len(done), changes=changes), open(a.out + '.changes.json', 'w'), indent=1)
for ch in changes: print(ch['engine'], ch['n'], ch['after'], ch['subject'][:80], 'skipped:', ch['untestable_between'])
