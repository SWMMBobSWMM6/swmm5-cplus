#!/usr/bin/env python3
"""Sort the models where SWMM6's two engines still differ (new commit, no FP contraction) into groups.

  lngroups6u.py RESULTS6U.jsonl OUT.json [--corpus DIR]

Groups the models whose largest element change is over 10 % of peak: networks with a Type 1 pump
(flow stepped by wet-well volume) drawing from a non-storage node, decks from the 2026 OpenSWMM code
review, and the rest. Also counts, over the whole corpus, the models whose Type 1 pumps all draw from
storage nodes and how many of those differ, which is the control for the pump group.
"""
import sys, os, re, json, gzip, argparse, collections

ap = argparse.ArgumentParser()
ap.add_argument('results'); ap.add_argument('out')
ap.add_argument('--corpus', default='/home/claude/corpus1729')
a = ap.parse_args()
op = gzip.open if a.results.endswith('.gz') else open
rows = {}
for l in op(a.results, 'rt'):
    r = json.loads(l)
    if 'runs' in r: rows[r['inp']] = r

def sections(t):
    S = collections.defaultdict(list); s = None
    for line in t.splitlines():
        l = line.split(';', 1)[0].strip()
        if not l: continue
        if l.startswith('['): s = l.upper(); continue
        S[s].append(l.split())
    return S

def pump1(m):
    """'storage', 'junction', 'both' or None for the inlets of the model's Type 1 pumps."""
    try: t = open(os.path.join(a.corpus, m), encoding='latin-1').read()
    except OSError: return None
    S = sections(t)
    stor = {x[0] for x in S['[STORAGE]']}
    curves = {x[0]: x[1].upper() for x in S['[CURVES]'] if len(x) > 1 and x[1].upper().startswith('PUMP')}
    k = set()
    for p in S['[PUMPS]']:
        if len(p) >= 4 and curves.get(p[3]) == 'PUMP1':
            k.add('storage' if p[1] in stor else 'junction')
    return None if not k else (k.pop() if len(k) == 1 else 'both')

def worst(r, pair):
    m = (r.get('move') or {}).get(pair) or {}
    return max((m.get('linkFlow') or {}).get('shapeMax') or 0, (m.get('nodeDepth') or {}).get('shapeMax') or 0)

def state(r):
    ru = r['runs']
    if ru['LC']['out_bytes'] == 0 or ru['NC']['out_bytes'] == 0: return 'missing'
    if ru['LC']['out_sha'] == ru['NC']['out_sha'] or r.get('pairs', {}).get('LC|NC', {}).get('status') == 'identical values':
        return 'identical'
    return 'differs' if r.get('pairs', {}).get('LC|NC', {}).get('status') == 'differs' else 'other'

G = {'pump1_junction': [], 'code_review': [], 'other': []}
ctl = collections.Counter()
for m, r in sorted(rows.items()):
    k = pump1(m); st = state(r)
    if k: ctl[(k, st)] += 1
    if st != 'differs' or worst(r, 'LC|NC') < 10: continue
    if k in ('junction', 'both'): G['pump1_junction'].append(m)
    elif m.startswith('OpenSWMM_Code_Review_2026/'): G['code_review'].append(m)
    else: G['other'].append(m)
R = {'groups': G, 'counts': {k: len(v) for k, v in G.items()},
     'pump1_control': {f'{k} {s}': n for (k, s), n in sorted(ctl.items())}}
json.dump(R, open(a.out, 'w'), indent=1)
print(json.dumps(R['counts']), json.dumps(R['pump1_control']))
print('other:', G['other'])
