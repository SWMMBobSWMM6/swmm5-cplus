#!/usr/bin/env python3
"""Which kinds of model FP contraction moved: features of the models whose new-engine output changed by
more than 10 % of some element's peak between build A (contraction on) and build B (off), against the
models whose output bytes it left alone.

  flagfeat6u.py RESULTS6U.jsonl FEATURES.json.gz OUT.json
"""
import sys, json, gzip

src, feat, out = sys.argv[1:4]
F = json.load(gzip.open(feat))
op = gzip.open if src.endswith('.gz') else open
rows = [r for r in (json.loads(l) for l in op(src, 'rt')) if 'runs' in r]

def worst(r, pair):
    m = (r.get('move') or {}).get(pair) or {}
    return max((m.get('linkFlow') or {}).get('shapeMax') or 0, (m.get('nodeDepth') or {}).get('shapeMax') or 0)

moved, unmoved = [], []
for r in rows:
    m = r['inp']
    if m not in F: continue
    if 'NA' in r['runs']:
        if r.get('pairs', {}).get('NA|NB', {}).get('status') == 'differs' and worst(r, 'NA|NB') >= 10: moved.append(m)
    elif r['prevA'].get('N') == r['runs']['NB']['out_sha'] and r['runs']['NB']['out_bytes'] > 0:
        unmoved.append(m)

def share(ms, k):
    return round(100 * sum(1 for m in ms if (F[m].get(k) or 0) > 0) / max(1, len(ms)))

R = {}
for name, ms in (('moved_over10', moved), ('unmoved', unmoved)):
    R[name] = {'n': len(ms), **{k: share(ms, k) for k in ('pumps', 'controls', 'subcatchments', 'lid', 'pollutants')},
               'dynwave': round(100 * sum(1 for m in ms if F[m].get('routing') == 'DYNWAVE') / max(1, len(ms)))}
json.dump(R, open(out, 'w'), indent=1)
print(json.dumps(R))
