#!/usr/bin/env python3
"""Summarise cmp4.py results: controls, identity counts, size of the differences, run failures.

  summarize.py FEATURES.json.gz OUT.json RESULTS.jsonl [RESULTS.jsonl ...] [--set NAME=LIST ...]

--set counts how many summarised models came from each model list (e.g. A=list_A.txt).

A differing pair is put in one class by the largest change in any element, measured element by
element as max|B-A| over the run divided by that element's peak (the larger of the two peaks),
for link flow and node depth, above a floor of 1 L/s and 1 mm:
  last digits   under 0.01 %
  under 1 %     0.01 to 1 %
  1 to 10 %
  over 10 %
  off-network   values differ but no link flow or node depth above the floor does (system
                totals, subcatchments, or elements below the floor)
"""
import sys, json, gzip, collections

args = sys.argv[1:]
sets = dict(a.split('=', 1)[1].split('=', 1) for a in args if a.startswith('--set='))
args = [a for a in args if not a.startswith('--set=')]
FEAT, OUT, files = args[0], args[1], args[2:]
F = json.load(gzip.open(FEAT))
rows = {}
for f in files:
    for l in open(f):
        r = json.loads(l); rows[r['inp']] = r          # a later file overrides an earlier one
rows = list(rows.values())
PAIRS = ['E|C', 'L|L2', 'N|N2', 'E|L', 'L|N', 'E|N', 'C|N']
CLASSES = ['identical', 'last digits', 'under 1 %', '1 to 10 %', 'over 10 %', 'off-network', 'not comparable', 'missing']

def same_bytes(r, pair):
    a, b = pair.split('|'); ru = r.get('runs', {})
    return a in ru and b in ru and ru[a]['out_bytes'] > 0 and ru[a]['out_sha'] == ru[b]['out_sha']

def status(r, pair):
    p = r.get('pairs', {}).get(pair, {}); st = p.get('status', 'none')
    if st != 'identical values' and same_bytes(r, pair): return 'identical values'   # e.g. no reporting periods at all
    if st == 'not comparable' and p.get('reason') == 'no common reporting periods': return 'no periods'
    return st

def size(r, pair):
    p = r.get('pairs', {}).get(pair, {}); st = status(r, pair)
    if st == 'identical values': return 'identical', 0.0
    if st == 'no periods': return 'not comparable', None
    if st == 'missing': return 'missing', None
    if st == 'not comparable': return 'not comparable', None
    m = (r.get('move') or {}).get(pair)
    if m is None:                                   # pairs without movement metrics (controls)
        return 'last digits' if max(p.get('maxAbs', {}).values() or [0]) < 1e-4 else 'over 10 %', None
    worst = max([(m.get('linkFlow') or {}).get('shapeMax') or 0, (m.get('nodeDepth') or {}).get('shapeMax') or 0])
    nel = ((m.get('linkFlow') or {}).get('n') or 0) + ((m.get('nodeDepth') or {}).get('n') or 0)
    if worst == 0:
        return 'off-network', 0.0
    if worst < 0.01: return 'last digits', worst
    if worst < 1: return 'under 1 %', worst
    if worst < 10: return '1 to 10 %', worst
    return 'over 10 %', worst

res = {'models': len(rows), 'errors': [r['inp'] for r in rows if 'runs' not in r]}
rows = [r for r in rows if 'runs' in r]
res['status'] = {p: dict(collections.Counter(status(r, p) for r in rows)) for p in PAIRS}
res['classes'] = {p: {c: 0 for c in CLASSES} for p in ('E|L', 'L|N', 'E|N')}
worst = {p: [] for p in ('E|L', 'L|N', 'E|N')}
for r in rows:
    for p in res['classes']:
        c, w = size(r, p); res['classes'][p][c] += 1
        if w is not None and w > 0: worst[p].append((w, r['inp']))
res['worst'] = {p: [[round(w, 3), m] for w, m in sorted(v, reverse=True)[:12]] for p, v in worst.items()}

# strata: routing method as the .inp states it
def stratum(m):
    f = F.get(m, {}); rt = (f.get('routing') or '?').upper()
    rt = {'DW': 'DYNWAVE', 'STEADYFLOW': 'STEADY'}.get(rt, rt)
    return rt + (' + SLOT' if f.get('surcharge') == 'SLOT' else '')
st = collections.defaultdict(lambda: collections.Counter())
for r in rows:
    s = stratum(r['inp'])
    for p in ('E|L', 'L|N', 'E|N'):
        st[s][p + ' ' + size(r, p)[0]] += 1
    st[s]['models'] += 1
# the system view: total outflow volume and peak, and the typical link, for models that differ
def pct(v, f):
    v = sorted(v); return v[min(len(v) - 1, int(f * len(v)))] if v else None
res['system'] = {}
for p in ('E|L', 'L|N', 'E|N'):
    vol, pk, lmed = [], [], []
    for r in rows:
        m = (r.get('move') or {}).get(p)
        if status(r, p) != 'differs' or not m: continue
        of = m.get('outflow') or {}
        if of.get('volPct') is not None: vol.append(abs(of['volPct']))
        if of.get('peakPct') is not None: pk.append(abs(of['peakPct']))
        if (m.get('linkFlow') or {}).get('peakMedian') is not None: lmed.append(m['linkFlow']['peakMedian'])
    res['system'][p] = {'n_volume': len(vol), 'volume_median': pct(vol, .5), 'volume_p90': pct(vol, .9),
                        'volume_under_0.1': sum(1 for v in vol if v < 0.1), 'volume_under_1': sum(1 for v in vol if v < 1),
                        'n_peak': len(pk), 'peak_median': pct(pk, .5), 'peak_p90': pct(pk, .9), 'peak_under_1': sum(1 for v in pk if v < 1),
                        'link_peak_median_of_medians': pct(lmed, .5)}
res['strata'] = {k: dict(v) for k, v in sorted(st.items(), key=lambda kv: -kv[1]['models'])}

# features present in models where the new engine differs from its legacy engine
feat_keys = ['pollutants', 'snow', 'lid', 'rdii', 'groundwater', 'controls', 'inlets', 'pumps', 'culverts', 'treatment', 'subcatchments']
fc = {'all': collections.Counter(), 'L|N differs': collections.Counter()}
for r in rows:
    f = F.get(r['inp'], {})
    has = [k for k in feat_keys if f.get(k)]
    for k in has: fc['all'][k] += 1
    if status(r, 'L|N') == 'differs':
        for k in has: fc['L|N differs'][k] += 1
res['features'] = {k: dict(v) for k, v in fc.items()}

# which output variables differ, counted once per model
bv = {p: collections.Counter() for p in ('E|L', 'L|N')}
for r in rows:
    for p in bv:
        for k in (r.get('pairs', {}).get(p, {}).get('byVar') or {}): bv[p][k] += 1
res['variables'] = {p: dict(v.most_common(15)) for p, v in bv.items()}

# runs: who produced an output file
def has(r, k): return r['runs'][k]['out_bytes'] > 0
fail = collections.Counter(); lists = collections.defaultdict(list)
for r in rows:
    e, l, n = has(r, 'E'), has(r, 'L'), has(r, 'N')
    key = ('E' if e else '-') + ('L' if l else '-') + ('N' if n else '-')
    fail[key] += 1
    if key not in ('ELN', '---'):
        err = (r.get('rpt', {}).get('N') or {}).get('error') or (r.get('rpt', {}).get('L') or {}).get('error') or (r.get('rpt', {}).get('E') or {}).get('error')
        lists[key].append([r['inp'], err, r['runs']['N']['rc']])
res['outputs'] = {'pattern': dict(fail), 'cases': dict(lists)}
res['extra_period'] = sum(1 for r in rows if (r.get('pairs', {}).get('E|N', {}).get('onlyB') or 0) > 0)
res['wall'] = {k: round(sum(r['runs'][k]['wall'] for r in rows), 1) for k in ('E', 'C', 'L', 'N')}
res['timeouts'] = {k: [r['inp'] for r in rows if r['runs'][k]['rc'] == 'timeout'] for k in ('E', 'C', 'L', 'L2', 'N', 'N2')}
res['timeouts_new'] = [dict(inp=r['inp'], epa_output=r['runs']['E']['out_bytes'] > 0, epa_s=r['runs']['E']['wall'], legacy_s=r['runs']['L']['wall'],
                            legacy_output=r['runs']['L']['out_bytes'] > 0) for r in rows if r['runs']['N']['rc'] == 'timeout']
for name, lst in sets.items():
    want = set(l.rstrip('\n') for l in open(lst) if l.strip())
    res['models_' + name] = sum(1 for r in rows if r['inp'] in want)
json.dump(res, open(OUT, 'w'), indent=1)
print(json.dumps({k: res[k] for k in ('models', 'status', 'classes', 'extra_period', 'wall')}, indent=1))
print(json.dumps(res['outputs']['pattern']))
