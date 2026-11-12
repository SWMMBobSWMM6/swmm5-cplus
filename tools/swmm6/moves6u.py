#!/usr/bin/env python3
"""Which models the newer SWMM6 commit moved closer to EPA 5.2.4, and which it moved further away.

  moves6u.py RESULTS6U.jsonl OUT.json [--skip LIST.txt]

For every model whose output changed between B (the old commit without FP contraction) and C (the
new commit), in either engine, compares the distance to EPA before and after. Distance is the
worst element change against EPA (link flow or node depth, over the run, as a percent of that
element's peak, the same measure as summarize.py's classes); 0 where the bytes are identical.
  closer       distance fell by more than a tenth of itself and more than 0.001 %
  further      distance rose by the same margin
  about same   otherwise (the output changed, but not its distance to EPA)
  no EPA       EPA wrote no comparable output for this model
--skip drops models (one per line) whose results depend on files other corpus models write.
A model whose run hit the time limit in either build is listed under timed_out and not counted.
"""
import sys, json, collections

src, out = sys.argv[1], sys.argv[2]
skip = set()
if '--skip' in sys.argv:
    skip = {l.strip() for l in open(sys.argv[sys.argv.index('--skip') + 1]) if l.strip()}
rows = {}
for l in open(src):
    r = json.loads(l)
    if 'runs' in r: rows[r['inp']] = r

def same(r, a, b):
    ru = r['runs']; return ru[a]['out_bytes'] > 0 and ru[a]['out_sha'] == ru[b]['out_sha']

def dist(r, a, b):
    """Worst element change, percent of peak; None when the pair has no comparable values."""
    if same(r, a, b): return 0.0
    p = r.get('pairs', {}).get(a + '|' + b, {})
    if p.get('status') == 'identical values': return 0.0
    m = (r.get('move') or {}).get(a + '|' + b)
    if p.get('status') != 'differs' or not m: return None
    return max((m.get('linkFlow') or {}).get('shapeMax') or 0, (m.get('nodeDepth') or {}).get('shapeMax') or 0)

def vol(r, a, b):
    m = (r.get('move') or {}).get(a + '|' + b) or {}
    return (m.get('outflow') or {}).get('volPct')

def verdict(d0, d1):
    if d0 is None or d1 is None: return 'no EPA'
    if abs(d1 - d0) <= max(0.1 * min(d0, d1), 0.001): return 'about same'
    return 'closer' if d1 < d0 else 'further'

R = {'models': len(rows), 'skipped': sorted(skip & set(rows)), 'engines': {}}
for eng, kb, kc in (('legacy', 'LB', 'LC'), ('new', 'NB', 'NC')):
    lst, cnt, timeouts = [], collections.Counter(), []
    for m, r in sorted(rows.items()):
        if m in skip: continue
        ru = r['runs']
        if ru[kb]['out_bytes'] == 0 and ru[kc]['out_bytes'] == 0: continue
        if same(r, kb, kc): continue
        if 'timeout' in (ru[kb]['rc'], ru[kc]['rc']):
            timeouts.append(m); continue          # a run stopped by the time limit is not a result
        p = r.get('pairs', {}).get(kb + '|' + kc, {})
        if p.get('status') == 'identical values': continue
        d0, d1 = dist(r, 'E', kb), dist(r, 'E', kc)
        bc = dist(r, kb, kc)
        v = verdict(d0, d1)
        if ru[kb]['out_bytes'] == 0 or ru[kc]['out_bytes'] == 0:
            v = 'now runs' if ru[kb]['out_bytes'] == 0 else 'now refused'
        cnt[v] += 1
        lst.append(dict(inp=m, verdict=v, before=d0, after=d1, change=bc,
                        vol_before=vol(r, 'E', kb), vol_after=vol(r, 'E', kc),
                        exact_before=d0 == 0.0, exact_after=d1 == 0.0,
                        rc_before=ru[kb]['rc'], rc_after=ru[kc]['rc']))
    R['engines'][eng] = dict(counts=dict(cnt), changed=len(lst),
                             left_exact=sum(1 for x in lst if x['exact_before'] and not x['exact_after']),
                             became_exact=sum(1 for x in lst if x['exact_after'] and not x['exact_before']),
                             timed_out=timeouts, models=lst)
# models that changed in both engines, and whether they moved the same way
L = {x['inp']: x for x in R['engines']['legacy']['models']}
N = {x['inp']: x for x in R['engines']['new']['models']}
both = sorted(set(L) & set(N))
R['both'] = dict(n=len(both), same_verdict=sum(1 for m in both if L[m]['verdict'] == N[m]['verdict']),
                 only_legacy=len(set(L) - set(N)), only_new=len(set(N) - set(L)))
json.dump(R, open(out, 'w'), indent=1)
for eng, e in R['engines'].items():
    print(eng, e['changed'], e['counts'], 'left exact', e['left_exact'], 'became exact', e['became_exact'])
print('both', R['both'])
