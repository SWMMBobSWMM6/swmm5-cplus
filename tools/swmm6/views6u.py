#!/usr/bin/env python3
"""Turn cmp6u.py rows into cmp4-style rows for one SWMM6 build, so summarize.py reads them unchanged,
and measure what changed between the builds.

  views6u.py RESULTS6U.jsonl OUTDIR PREV.jsonl[,PREV2...] [SKIP.txt]

Writes OUTDIR/view_B.jsonl (SWMM6 c40580c rebuilt without FP contraction) and OUTDIR/view_C.jsonl
(SWMM6 at the new commit), and OUTDIR/deltas.json:
  build   the old commit as first built (A) against the same commit without contraction (B)
  code    B against the new commit (C)
  moves   how each model's class against EPA, and new against legacy, moved from A to B to C
Classes are summarize.py's: the largest element change as a percent of that element's peak.
SKIP.txt lists models left out of the moves (A is read from the old gate, so a model whose input
files changed since then, e.g. a hot-start file another model writes, would compare two inputs).
"""
import sys, os, json, gzip, collections

src, outdir, prevs = sys.argv[1], sys.argv[2], sys.argv[3]
skip = {l.strip() for l in open(sys.argv[4]) if l.strip()} if len(sys.argv) > 4 else set()
os.makedirs(outdir, exist_ok=True)
prev = {}
for p in prevs.split(','):
    op = gzip.open if p.endswith('.gz') else open
    for l in op(p, 'rt'):
        r = json.loads(l); prev[r['inp']] = r
rows = {}
for l in open(src):
    r = json.loads(l); rows[r['inp']] = r
rows = [r for r in rows.values() if 'runs' in r]

def same_bytes(r, a, b):
    ru = r.get('runs', {})
    return a in ru and b in ru and ru[a]['out_bytes'] > 0 and ru[a]['out_sha'] == ru[b]['out_sha']

def size(r, pair):
    """summarize.py's class for one pair of one row."""
    a, b = pair.split('|')
    p = r.get('pairs', {}).get(pair, {}); st = p.get('status', 'none')
    if st != 'identical values' and same_bytes(r, a, b): st = 'identical values'
    if st == 'identical values': return 'identical'
    if st == 'not comparable' and p.get('reason') == 'no common reporting periods': return 'not comparable'
    if st in ('missing', 'not comparable', 'none'): return st
    m = (r.get('move') or {}).get(pair)
    if m is None: return 'differs'
    worst = max([(m.get('linkFlow') or {}).get('shapeMax') or 0, (m.get('nodeDepth') or {}).get('shapeMax') or 0])
    if worst == 0: return 'off-network'
    if worst < 0.01: return 'last digits'
    if worst < 1: return 'under 1 %'
    if worst < 10: return '1 to 10 %'
    return 'over 10 %'

def view(r, L, L2, N, N2, pairmap):
    ru = r['runs']
    v = dict(inp=r['inp'], runs={'E': ru['E'], 'C': ru['C'], 'L': ru[L], 'L2': ru[L2], 'N': ru[N], 'N2': ru[N2]},
             pairs={}, move={}, rpt={})
    for new, old in pairmap.items():
        if old in r.get('pairs', {}): v['pairs'][new] = r['pairs'][old]
        if old in (r.get('move') or {}): v['move'][new] = r['move'][old]
    if 'E|N' in v['pairs'] and v['pairs'].get('E|C', {}).get('status') == 'identical values':
        v['pairs']['C|N'] = v['pairs']['E|N']
    rp = r.get('rpt', {})
    v['rpt'] = {'E': rp.get('E'), 'L': rp.get(L), 'N': rp.get(N)}
    return v

with open(os.path.join(outdir, 'view_C.jsonl'), 'w') as fc, open(os.path.join(outdir, 'view_B.jsonl'), 'w') as fb:
    for r in rows:
        fc.write(json.dumps(view(r, 'LC', 'LC2', 'NC', 'NC2', {'E|C': 'E|C', 'L|L2': 'LC|LC2', 'N|N2': 'NC|NC2',
                 'E|L': 'E|LC', 'L|N': 'LC|NC', 'E|N': 'E|NC'})) + '\n')
        fb.write(json.dumps(view(r, 'LB', 'LB', 'NB', 'NB', {'E|C': 'E|C', 'E|L': 'E|LB', 'L|N': 'LB|NB',
                 'E|N': 'E|NB'})) + '\n')

def old_size(m, pair):
    r = prev.get(m)
    if not r: return 'none'
    a, b = pair.split('|')
    return size(r, pair)

D = {'models': len(rows), 'build': {}, 'code': {}, 'moves': {}, 'moves_skipped': sorted(skip)}
# build effect: A against B, by bytes first (old gate hashes), values where they differ
for eng, kb, ka in (('new', 'NB', 'NA'), ('legacy', 'LB', 'LA')):
    c = collections.Counter(); ex = []
    for r in rows:
        pa = (r.get('prevA') or {}).get(ka[0])
        if pa is None: c['no old record'] += 1; continue
        if pa == r['runs'][kb]['out_sha']: c['identical bytes'] += 1; continue
        cl = size(r, ka + '|' + kb); c[cl] += 1
        if cl not in ('identical', 'missing'): ex.append(r['inp'])
    D['build'][eng] = dict(c); D['build'][eng + '_examples'] = ex[:40]
# code effect: B against C
for eng, kb, kc in (('new', 'NB', 'NC'), ('legacy', 'LB', 'LC')):
    c = collections.Counter(); ex = []
    for r in rows:
        cl = size(r, kb + '|' + kc); c[cl] += 1
        if cl not in ('identical', 'missing', 'none'): ex.append(r['inp'])
    D['code'][eng] = dict(c); D['code'][eng + '_models'] = ex
# moves: class per model at A (old gate), B and C, for each comparison
ORDER = ['identical', 'last digits', 'under 1 %', '1 to 10 %', 'off-network', 'over 10 %', 'not comparable', 'missing', 'none', 'differs']
for name, pa, pb, pc in (('E|L', 'E|L', 'E|LB', 'E|LC'), ('L|N', 'L|N', 'LB|NB', 'LC|NC'), ('E|N', 'E|N', 'E|NB', 'E|NC')):
    ab, bc, ac = collections.Counter(), collections.Counter(), collections.Counter()
    for r in rows:
        if r['inp'] in skip: continue
        A, B, C = old_size(r['inp'], pa), size(r, pb), size(r, pc)
        ab[A + ' -> ' + B] += 1; bc[B + ' -> ' + C] += 1; ac[A + ' -> ' + C] += 1
    D['moves'][name] = {'A->B': dict(ab), 'B->C': dict(bc), 'A->C': dict(ac)}
json.dump(D, open(os.path.join(outdir, 'deltas.json'), 'w'), indent=1)
print(json.dumps({'models': D['models'], 'build': {k: v for k, v in D['build'].items() if not k.endswith('examples')},
                  'code': {k: v for k, v in D['code'].items() if not k.endswith('models')}}, indent=1))
