#!/usr/bin/env python3
"""How much of the legacy engine's difference from EPA 5.2.4 do its hydraulic changes explain?

  attribution.py ALL_DIFFERING.txt OUTFALL.jsonl MORE.jsonl OUT.json

ALL_DIFFERING lists the models where SWMM6's legacy engine and EPA 5.2.4 differ. OUTFALL.jsonl
compares the legacy engine with EPA 5.2.4 patched with the legacy outfall rule only
(epa524_outfall_like_53.patch); MORE.jsonl compares it, on the models still different, with
EPA 5.2.4 patched with the outfall rule plus the storage and conveyance changes.
"""
import sys, json, collections
lst, a, b, out = sys.argv[1:5]
n = sum(1 for l in open(lst) if l.strip())
def ident(path):
    st = {}
    for l in open(path):
        r = json.loads(l); p = next(iter(r.get('pairs', {}).values()), {})
        st[r['inp']] = p.get('status')
    return st
A, B = ident(a), ident(b)
res = dict(n=n, run=len(A), outfall=sum(1 for v in A.values() if v == 'identical values'),
           more=sum(1 for v in B.values() if v == 'identical values'),
           rest_status=dict(collections.Counter(v for v in B.values() if v != 'identical values')))
# what the models still different after both patches differ in
HYDRO = ('lateral inflow', 'runoff', 'evaporation + infiltration', 'rainfall', 'snow depth', 'groundwater inflow', 'RDII inflow',
         'dry weather inflow', 'total lateral inflow', 'air temperature', 'potential ET', 'evaporation', 'external inflow')
rest_kind = collections.Counter()
for l in open(b):
    r = json.loads(l); p = next(iter(r.get('pairs', {}).values()), {})
    if p.get('status') != 'differs': continue
    keys = (p.get('byVar') or {}).keys()
    other = any(k.startswith('sub:') or 'pollutant' in k or k.split(':', 1)[1] in HYDRO for k in keys)
    rest_kind['runoff, inflow or quality values differ' if other else 'hydraulic values only'] += 1
res['rest_kind'] = dict(rest_kind)
json.dump(res, open(out, 'w'), indent=1)
print(res)
