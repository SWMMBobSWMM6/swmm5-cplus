#!/usr/bin/env python3
"""Cost per link-iteration: CPU time / (routing steps x iterations per step x links), per model.

Runs each model once with the given build, keeps the report and the .out header, and divides
the median CPU time from a bench JSON by the work the report says was done.
  costmodel.py EXE BENCH.json OUT.json
"""
import sys, os, re, json, struct, subprocess, tempfile, statistics, datetime
EXE, BENCH, OUT = os.path.abspath(sys.argv[1]), sys.argv[2], sys.argv[3]
C = os.environ.get('SWMM_CORPUS', 'corpus1729')
bench = json.load(open(BENCH))['results']
rows = []
for m in bench:
    inp = os.path.join(C, m)
    with tempfile.TemporaryDirectory(dir='/tmp/claude-0') as d:
        subprocess.run([EXE, inp, d + '/m.rpt', d + '/m.out'], cwd=os.path.dirname(inp), capture_output=True)
        rpt = open(d + '/m.rpt', encoding='latin-1').read()
        b = open(d + '/m.out', 'rb').read(28)
        nsub, nnode, nlink = struct.unpack('<3i', b[12:24])
    g = lambda pat: (re.search(pat, rpt) or [None, None])[1]
    avg = float(g(r'Average Time Step\s*:\s*([\d.]+)') or 'nan')
    it = float(g(r'Average Iterations per Step\s*:\s*([\d.]+)') or 'nan')
    st = g(r'Starting Date \.+ (\S+ \S+)'); en = g(r'Ending Date \.+ (\S+ \S+)')
    f = '%m/%d/%Y %H:%M:%S'
    dur = (datetime.datetime.strptime(en, f) - datetime.datetime.strptime(st, f)).total_seconds()
    steps = dur / avg if avg > 0 else float('nan')   # the report rounds very small steps to 0.00
    cpu = statistics.median(r['cpu'] for r in bench[m]['ref'])
    rows.append(dict(model=m, subcatchments=nsub, nodes=nnode, links=nlink, duration_h=dur / 3600, avg_step_s=avg,
                     steps=steps, iterations=it, cpu_s=cpu, ns_per_link_iteration=1e9 * cpu / (steps * it * max(nlink, 1))))
json.dump(rows, open(OUT, 'w'), indent=1)
for r in rows:
    print('%-42s subs %4d nodes %5d links %5d  %6.1f h  step %7.3f s  steps %9.0f  iter %.2f  cpu %5.2f s  %6.0f ns per link-iteration' % (
        r['model'][:42], r['subcatchments'], r['nodes'], r['links'], r['duration_h'], r['avg_step_s'], r['steps'], r['iterations'], r['cpu_s'], r['ns_per_link_iteration']))
