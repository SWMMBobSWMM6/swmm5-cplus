#!/usr/bin/env python3
"""Control for the conduit-loss commit: which dynamic-wave models have evaporation or seepage, and
which of them the commit changed.

  losses6u.py RESULTS6U.jsonl CHANGES.json COMMIT OUT.json [--corpus DIR]

CHANGES.json is bisect6.py's report; COMMIT is the commit whose models are tested (40f439a).
A model has evaporation if its [EVAPORATION] rate is a nonzero constant or comes from a series,
file, monthly values or temperature; it has seepage if any [LOSSES] row has a seepage rate > 0.
Counted over models whose legacy engine wrote output at the old commit (build B). Also compares the
legacy engine's reported routing continuity error before (B) and after (C) for the models that moved;
that is the engine's own bookkeeping, not an independent check.
"""
import sys, os, re, json, gzip, argparse, collections

ap = argparse.ArgumentParser()
ap.add_argument('results'); ap.add_argument('changes'); ap.add_argument('commit'); ap.add_argument('out')
ap.add_argument('--corpus', default='/home/claude/corpus1729')
a = ap.parse_args()

def sections(t):
    S = collections.defaultdict(list); s = None
    for line in t.splitlines():
        l = line.split(';', 1)[0].strip()
        if not l: continue
        if l.startswith('['): s = l.upper(); continue
        S[s].append(l.split())
    return S

num = re.compile(r'^[-+]?[\d.]+([eE][-+]?\d+)?$')
def feat(m):
    S = sections(open(os.path.join(a.corpus, m), encoding='latin-1').read())
    evap = False
    for x in S['[EVAPORATION]']:
        k = x[0].upper()
        if k == 'CONSTANT' and len(x) > 1 and num.match(x[1]) and float(x[1]) != 0: evap = True
        if k in ('MONTHLY', 'TIMESERIES', 'TEMPERATURE', 'FILE'): evap = True
    seep = any(len(x) > 5 and num.match(x[5]) and float(x[5]) > 0 for x in S['[LOSSES]'])
    rout = [x[1].upper() for x in S['[OPTIONS]'] if x[0].upper() == 'FLOW_ROUTING' and len(x) > 1]
    return dict(evap=evap, seep=seep, dw=bool(rout) and rout[0] == 'DYNWAVE',
                pumps=bool(S['[PUMPS]']), controls=bool(S['[CONTROLS]']))

ch = json.load(open(a.changes))
moved = set()
for c in ch['changes']:
    if c['after'] == a.commit: moved |= set(c['models'])
op = gzip.open if a.results.endswith('.gz') else open
C = collections.Counter(); kinds = collections.Counter(); cont = collections.Counter(); cont_rows = []
for l in op(a.results, 'rt'):
    r = json.loads(l)
    if 'runs' not in r or r['runs']['LB']['out_bytes'] == 0: continue
    try: f = feat(r['inp'])
    except OSError: continue
    if not f['dw']: continue
    lossy = f['evap'] or f['seep']
    C[('with losses' if lossy else 'without losses', 'moved' if r['inp'] in moved else 'unchanged')] += 1
    if r['inp'] in moved:
        rb = (r.get('rpt') or {}).get('LB') or {}; rc = (r.get('rpt') or {}).get('LC') or {}
        cb, cc = rb.get('routingContinuity'), rc.get('routingContinuity')
        if cb is not None and cc is not None:
            d = abs(cc) - abs(cb)
            cont['smaller' if d < -0.005 else 'larger' if d > 0.005 else 'same'] += 1
            cont_rows.append((r['inp'], cb, cc))
        kinds['evaporation' if f['evap'] and not f['seep'] else 'seepage' if f['seep'] and not f['evap'] else 'both' if lossy else 'neither'] += 1
        kinds['with pumps'] += f['pumps']; kinds['with control rules'] += f['controls']
R = {'commit': a.commit, 'moved': len(moved),
     'dynwave': {f'{k[0]}, {k[1]}': v for k, v in sorted(C.items())}, 'moved_by_kind': dict(kinds),
     'routing_continuity_legacy': {'abs_error_smaller': cont['smaller'], 'abs_error_larger': cont['larger'],
                                   'same_within_0.005': cont['same'], 'rows': cont_rows}}
json.dump(R, open(a.out, 'w'), indent=1)
print(json.dumps(R, indent=1))
