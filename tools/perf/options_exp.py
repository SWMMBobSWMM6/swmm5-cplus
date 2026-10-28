#!/usr/bin/env python3
"""Speed versus accuracy for routing options: run each model as given and with one option changed.

  options_exp.py EXE OUT.json

Each variant is a copy of the .inp with [OPTIONS] lines replaced, written into the model's own
folder (relative data paths keep working) and deleted after the run. Time is child CPU time;
accuracy comes from outdiff.js against the as-given run of the same build.
"""
import sys, os, re, json, time, subprocess, tempfile, shutil

EXE = os.path.abspath(sys.argv[1]); OUT = sys.argv[2]
C = os.environ.get('SWMM_CORPUS', 'corpus1729')
MODELS = ['Semi_Real_Models/375_H&H_Elements (1).inp', 'XPSWMM/MAYBR.inp', 'Hydraulics/short_trap_SW5.inp',
          'Training/Exercise5_transect.inp']

def opts(path):
    o = {}; sec = None
    for l in open(path, encoding='latin-1'):
        s = l.strip()
        if s.startswith('['): sec = s.upper(); continue
        if sec == '[OPTIONS]' and s and not s.startswith(';'):
            p = s.split(None, 1); o[p[0].upper()] = p[1].strip() if len(p) > 1 else ''
    return o

def hms(v):
    if ':' in v:
        p = [float(x) for x in v.split(':')]
        while len(p) < 3: p.append(0)
        return p[0] * 3600 + p[1] * 60 + p[2]
    return float(v)

def variants(o):
    rs = hms(o.get('ROUTING_STEP', '20'))
    vs = float(o.get('VARIABLE_STEP', '0') or 0)
    ms = float(o.get('MINIMUM_STEP', '0.5') or 0.5)
    v = [('as given', {})]
    v.append(('ROUTING_STEP x2', {'ROUTING_STEP': '%g' % (2 * rs)}))
    v.append(('ROUTING_STEP x10', {'ROUTING_STEP': '%g' % (10 * rs)}))
    if vs > 0:
        v.append(('VARIABLE_STEP 1.0', {'VARIABLE_STEP': '1.0'}))
    v.append(('MINIMUM_STEP 1 s' if ms < 1 else 'MINIMUM_STEP x2', {'MINIMUM_STEP': '%g' % (1.0 if ms < 1 else 2 * ms)}))
    v.append(('LENGTHENING_STEP 30 s', {'LENGTHENING_STEP': '30'}))
    v.append(('MAX_TRIALS 4', {'MAX_TRIALS': '4'}))
    v.append(('HEAD_TOLERANCE 0.01 ft', {'HEAD_TOLERANCE': '0.01'}))
    v.append(('SKIP_STEADY_STATE YES', {'SKIP_STEADY_STATE': 'YES', 'SYS_FLOW_TOL': '5', 'LAT_FLOW_TOL': '5'}))
    v.append(('SURCHARGE_METHOD SLOT', {'SURCHARGE_METHOD': 'SLOT'}))
    return v

def write_variant(src, changes):
    lines = open(src, encoding='latin-1').read().split('\n'); out = []; sec = None; done = set()
    for l in lines:
        s = l.strip()
        if s.startswith('['):
            if sec == '[OPTIONS]':
                for k, val in changes.items():
                    if k not in done: out.append('%-20s %s' % (k, val))
            sec = s.upper()
        if sec == '[OPTIONS]' and s and not s.startswith(';') and not s.startswith('['):
            k = s.split()[0].upper()
            if k in changes: out.append('%-20s %s' % (k, changes[k])); done.add(k); continue
        out.append(l)
    p = os.path.join(os.path.dirname(src), '_optexp_%d.inp' % os.getpid())
    open(p, 'w', encoding='latin-1').write('\n'.join(out)); return p

def run(inp, d):
    t0 = time.perf_counter()
    p = subprocess.Popen([EXE, inp, d + '/m.rpt', d + '/m.out'], cwd=os.path.dirname(inp), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _, st, ru = os.wait4(p.pid, 0)
    return ru.ru_utime + ru.ru_stime, time.perf_counter() - t0

results = {}
for m in MODELS:
    src = os.path.join(C, m); o = opts(src); results[m] = {'options': o, 'variants': []}
    base = tempfile.mkdtemp(dir='/tmp/claude-0')
    for name, ch in variants(o):
        inp = write_variant(src, ch) if ch else src
        d = base if not ch else tempfile.mkdtemp(dir='/tmp/claude-0')
        try:
            cpu, wall = run(inp, d)
        finally:
            if ch: os.remove(inp)
        row = dict(name=name, changes=ch, cpu=cpu, wall=wall)
        if ch:
            r = subprocess.run(['node', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'outdiff.js'), base + '/m.out', d + '/m.out', base + '/m.rpt', d + '/m.rpt'], capture_output=True, text=True)
            try: row['diff'] = json.loads(r.stdout)
            except Exception: row['diff'] = {'error': r.stderr[-300:]}
            shutil.rmtree(d)
        results[m]['variants'].append(row)
        print(m[:30], name, round(cpu, 2), flush=True)
    shutil.rmtree(base)
json.dump(results, open(OUT, 'w'), indent=1)
