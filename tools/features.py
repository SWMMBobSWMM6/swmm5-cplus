#!/usr/bin/env python3
"""Feature census of a SWMM5 .inp corpus: which code paths each model exercises.

  features.py CORPUS_DIR OUT.json

Per model: routing method, surcharge method, sections present, cross-section shapes,
culvert codes, storage curve types, pump types, infiltration model, and the
presence of expression features (GWF, TREATMENT, CONTROLS VARIABLE/EXPRESSION).
"""
import os, re, sys, json, collections

SHAPES = ['CIRCULAR', 'FORCE_MAIN', 'FILLED_CIRCULAR', 'RECT_CLOSED', 'RECT_OPEN', 'TRAPEZOIDAL',
          'TRIANGULAR', 'HORIZ_ELLIPSE', 'VERT_ELLIPSE', 'ARCH', 'PARABOLIC', 'POWER',
          'RECT_TRIANGULAR', 'RECT_ROUND', 'MODBASKETHANDLE', 'EGG', 'HORSESHOE', 'GOTHIC',
          'CATENARY', 'SEMIELLIPTICAL', 'BASKETHANDLE', 'SEMICIRCULAR', 'IRREGULAR', 'CUSTOM',
          'STREET', 'DUMMY']

def sections(text):
    cur = None
    out = collections.defaultdict(list)
    for raw in text.splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        if line.startswith('['):
            cur = line.upper()
            continue
        if cur:
            out[cur].append(line.split())
    return out

def census(path):
    t = open(path, encoding='latin-1', errors='replace').read()
    S = sections(t)
    f = {}
    opts = {r[0].upper(): (r[1].upper() if len(r) > 1 else '') for r in S.get('[OPTIONS]', [])}
    f['routing'] = opts.get('FLOW_ROUTING', 'KINWAVE')
    f['surcharge'] = opts.get('SURCHARGE_METHOD', 'EXTRAN')
    f['infiltration'] = opts.get('INFILTRATION', 'HORTON')
    f['sections'] = sorted(S.keys())
    shapes = collections.Counter()
    culverts = 0
    for r in S.get('[XSECTIONS]', []):
        if len(r) > 1:
            shapes[r[1].upper()] += 1
            if len(r) > 7 and r[7] not in ('0', '*'):
                culverts += 1
    f['shapes'] = dict(shapes)
    f['culverts'] = culverts
    f['storage_curves'] = sorted({r[4].upper() for r in S.get('[STORAGE]', []) if len(r) > 4})
    f['pumps'] = len(S.get('[PUMPS]', []))
    f['subcatchments'] = len(S.get('[SUBCATCHMENTS]', []))
    f['gwf'] = len(S.get('[GWF]', []))
    f['groundwater'] = len(S.get('[GROUNDWATER]', []))
    f['treatment'] = len(S.get('[TREATMENT]', []))
    ctl = ' '.join(' '.join(r) for r in S.get('[CONTROLS]', [])).upper()
    f['controls'] = len(S.get('[CONTROLS]', []))
    f['control_expr'] = int('EXPRESSION' in ctl or 'VARIABLE' in ctl)
    f['lid'] = len(S.get('[LID_USAGE]', []))
    f['snow'] = len(S.get('[SNOWPACKS]', []))
    f['rdii'] = len(S.get('[RDII]', []))
    f['inlets'] = len(S.get('[INLET_USAGE]', []))
    f['pollutants'] = len(S.get('[POLLUTANTS]', []))
    return f

if __name__ == '__main__':
    root = sys.argv[1]
    out = {}
    for d, dirs, files in os.walk(root):
        dirs[:] = [x for x in dirs if x != '.git']
        for fn in files:
            if fn.lower().endswith('.inp'):
                p = os.path.join(d, fn)
                try:
                    out[os.path.relpath(p, root)] = census(p)
                except Exception as e:
                    out[os.path.relpath(p, root)] = {'error': str(e)}
    json.dump(out, open(sys.argv[2], 'w'), indent=0)
    shapes = collections.Counter()
    for v in out.values():
        for s in v.get('shapes', {}):
            shapes[s] += 1
    print(len(out), 'models')
    print('models per shape:', dict(shapes.most_common()))
    for k in ['gwf', 'groundwater', 'treatment', 'control_expr', 'lid', 'snow', 'rdii', 'inlets',
              'culverts', 'pumps', 'subcatchments', 'pollutants']:
        print(k, sum(1 for v in out.values() if v.get(k)))
    print('routing', collections.Counter(v.get('routing') for v in out.values()))
    print('storage curves', collections.Counter(c for v in out.values() for c in v.get('storage_curves', [])))
