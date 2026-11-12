#!/usr/bin/env python3
"""Find the commits that changed SWMM6's results, between two commits, on a set of models.

  bisect6.py REPO_WORKTREE BUILD_DIR OLD NEW MODELS.txt OUT.jsonl [--corpus DIR] [--jobs N] [--timeout S]

Builds SWMM6 (both engines) at commits along OLD..NEW (first parent, only those touching src,
include or the CMake files), runs models on both engines, and records the hash of each .out.
OLD and NEW run every model. After that each model is narrowed on its own: wherever its hash
changes between two commits that ran it, the commit halfway between is built and runs that model
(and every other model that wants the same commit), until each change sits between two adjacent
commits. A commit that does not build is recorded and skipped over.
Resumable: commits already in OUT are not rebuilt, and rows from a longer model list are reused
(only the models in MODELS.txt are compared). BUILD_DIR must already be configured
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
        r = json.loads(l); c = r['commit']
        if c in done and done[c].get('build_ok') and r.get('build_ok'):   # a later partial run of the same commit
            for k in ('L', 'N'): done[c][k].update(r[k])
        else:
            done[c] = r

def test(c, want):
    """Build commit c (if needed) and run the models in want on both engines; merge into done."""
    have = done.get(c)
    if have is not None and not have.get('build_ok'): return have
    want = [m for m in want if have is None or m not in have['L']]
    if have is not None and not want: return have
    t0 = time.time()
    # -f: before dc5a1c2 every configure rewrites two tracked export headers (comments only)
    git('checkout', '-q', '-f', '--detach', c)
    b = subprocess.run(['ninja', '-C', a.build, '-j2', 'openswmm', 'openswmm-legacy'], capture_output=True, text=True)
    row = dict(commit=c, subject=git('log', '-1', '--format=%s', c), build_ok=b.returncode == 0,
               build_s=round(time.time() - t0))
    if b.returncode == 0:
        with ThreadPoolExecutor(a.jobs) as ex:
            res = list(ex.map(lambda km: run(*km), [(k, m) for m in want for k in ('L', 'N')]))
        row['L'] = {m: res[2 * i] for i, m in enumerate(want)}
        row['N'] = {m: res[2 * i + 1] for i, m in enumerate(want)}
    else:
        row['error'] = (b.stdout + b.stderr)[-600:]
    row['run_s'] = round(time.time() - t0) - row['build_s']
    with open(a.out, 'a') as f: f.write(json.dumps(row) + '\n')
    if have is not None and row['build_ok']:
        for k in ('L', 'N'): have[k].update(row[k])
    else:
        done[c] = row
    print(f"{c} build={'ok' if row['build_ok'] else 'FAIL'} {row['build_s']}s run {row['run_s']}s "
          f"({len(want)} models)  {row['subject'][:70]}", flush=True)
    return done[c]

idx = {c: i for i, c in enumerate(commits)}
def ok(c): return c in done and done[c].get('build_ok')
def known(c, k, m): return ok(c) and m in done[c][k]

def pending():
    """Per model and engine: where its hash changes between two commits that ran it, the untested commit
    halfway between. Models are narrowed independently, so each run carries only the models that still
    need that commit."""
    want = {}
    for k in ('L', 'N'):
        for m in models:
            have = [c for c in commits if known(c, k, m)]
            for x, y in zip(have, have[1:]):
                if done[x][k][m] == done[y][k][m]: continue
                between = [c for c in commits[idx[x] + 1: idx[y]]
                           if not (c in done and (not done[c].get('build_ok') or m in done[c][k]))]
                if between: want.setdefault(between[len(between) // 2], set()).add(m)
    return want

test(commits[0], models); test(commits[-1], models)
while True:
    want = pending()
    if os.environ.get('BISECT_DRY'):
        print({c: len(v) for c, v in sorted(want.items(), key=lambda x: idx[x[0]])}); sys.exit()
    if not want: break
    c = max(want, key=lambda c: (len(want[c]), -idx[c]))
    test(c, sorted(want[c]))
# report: per engine, each pair of adjacent commits that ran a model and gave it different bytes
changes = {}
for k in ('L', 'N'):
    for m in models:
        have = [c for c in commits if known(c, k, m)]
        for x, y in zip(have, have[1:]):
            if done[x][k][m] == done[y][k][m]: continue
            key = (x, y, k)
            if key not in changes:
                changes[key] = dict(before=x, after=y, untestable_between=[c for c in commits[idx[x] + 1: idx[y]]],
                                    engine=k, subject=done[y]['subject'], models=[])
            changes[key]['models'].append(m)
changes = sorted(changes.values(), key=lambda ch: (idx[ch['after']], ch['engine']))
for ch in changes: ch['n'] = len(ch['models'])
json.dump(dict(commits=len(commits), built=len(done), changes=changes), open(a.out + '.changes.json', 'w'), indent=1)
for ch in changes: print(ch['engine'], ch['n'], ch['after'], ch['subject'][:80], 'skipped:', ch['untestable_between'])
