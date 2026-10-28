#!/usr/bin/env python3
"""How far do results move when a build changes the arithmetic? Run each model with the reference
build and each test build once, then outdiff.js the pair.
  movement.py MODELS.txt OUT.json REF_EXE name=EXE ...
"""
import sys, os, json, subprocess, tempfile, shutil
models = [l.strip() for l in open(sys.argv[1]) if l.strip()]
OUT, REF = sys.argv[2], os.path.abspath(sys.argv[3])
tests = [(a.split('=', 1)[0], os.path.abspath(a.split('=', 1)[1])) for a in sys.argv[4:]]
C = os.environ.get('SWMM_CORPUS', 'corpus1729')
res = {}
for m in models:
    inp = os.path.join(C, m); res[m] = {}
    ref = tempfile.mkdtemp(dir='/tmp/claude-0')
    subprocess.run([REF, inp, ref + '/m.rpt', ref + '/m.out'], cwd=os.path.dirname(inp), capture_output=True)
    for name, exe in tests:
        d = tempfile.mkdtemp(dir='/tmp/claude-0')
        subprocess.run([exe, inp, d + '/m.rpt', d + '/m.out'], cwd=os.path.dirname(inp), capture_output=True)
        r = subprocess.run(['node', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'outdiff.js'), ref + '/m.out', d + '/m.out', ref + '/m.rpt', d + '/m.rpt'], capture_output=True, text=True)
        try: res[m][name] = json.loads(r.stdout)
        except Exception: res[m][name] = {'error': r.stderr[-300:]}
        shutil.rmtree(d)
    shutil.rmtree(ref)
    print(m, flush=True)
json.dump(res, open(OUT, 'w'), indent=1)
for m, v in res.items():
    for name, x in v.items():
        lf = x.get('linkFlow') or {}; nd = x.get('nodeDepth') or {}; of = x.get('outflow') or {}
        f = lambda v, p=3: '-' if v is None else ('%.' + str(p) + 'g') % v
        print('%-34s %-9s outflow vol %s%% peak %s%% | links n=%s peak med %s%% p90 %s%% max %s%% shape max %s%% | node depth peak max %s%% | contin %s' % (
            m[:34], name, f(of.get('volPct')), f(of.get('peakPct')), lf.get('n'), f(lf.get('peakMedian')), f(lf.get('peakP90')), f(lf.get('peakMax')), f(lf.get('shapeMax')), f(nd.get('peakMax')), x.get('routingContinuity')))
