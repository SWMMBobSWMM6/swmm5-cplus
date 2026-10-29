#!/usr/bin/env python3
"""Assemble swmm5-cplus.html (single file) and doc/essay.json from measured results."""
import os, re, json, math, base64, hashlib, collections, datetime, html, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'doc'))
import figures as F
import essay as E

HOME = '/home/claude'
REPO = f'{HOME}/swmm5-cplus'
AB = f'{HOME}/appbuild'
GATE = f'{HOME}/gate'
CORPUS = f'{HOME}/corpus1729'

def jl(p): return [json.loads(l) for l in open(p)] if os.path.exists(p) else []
def J(p): return json.load(open(p))

lift = J(f'{REPO}/docs/lift_report.json')
matrix_txt = open(f'{REPO}/docs/shape_override_matrix.txt').read()
cm = J(f'{GATE}/coupling_matrix.json')
coupled = J(f'{AB}/coupled_series.json')
b_o2 = J(f'{GATE}/bench_r3b.json'); b_lto = J(f'{GATE}/bench_lto_both.json')
features = J(f'{GATE}/features.json')
commit = open(f'{GATE}/final/engine_commit.txt').read().strip()

# ------------------------------------------------------------------ gate results
full_main = jl(f'{GATE}/full.jsonl')
# after the in-order gate was stopped, a seeded size-stratified sample of the unrun large models
sample_rows = jl(f'{GATE}/large_sample.jsonl') if os.path.exists(f'{GATE}/large_sample.jsonl') else []
full = full_main + sample_rows
N_SAMPLE = len(sample_rows)
corpus_n = sum(1 for l in open(f'{GATE}/full_by_size.txt') if l.strip())
def inconclusive(r):
    bad = {'timeout'}
    return str(r['rc_ref']) in bad or str(r['rc_test']) in bad or (r['rc_ref'] == r['rc_test'] and r['rc_ref'] in (-25, -9, 137, 153))
def crashed(r):
    # both engines killed by the same signal (negative return code), outputs compared and equal
    a, b = r['rc_ref'], r['rc_test']
    return isinstance(a, int) and a < 0 and a == b and r['out_same'] and r['rpt_same']
fold = collections.OrderedDict()
for r in sorted(full, key=lambda r: r['inp']):
    k = r['inp'].split('/')[0]
    f = fold.setdefault(k, dict(name=k, n=0, same=0, inc=0, diff=0, crash=0))
    f['n'] += 1
    if inconclusive(r): f['inc'] += 1
    elif crashed(r): f['crash'] += 1
    elif r['out_same'] and r['rpt_same']: f['same'] += 1
    else: f['diff'] += 1
FULL_DONE = len(full)
FULL_INC = sum(f['inc'] for f in fold.values())
FULL_DIFF = sum(f['diff'] for f in fold.values())
FULL_SAME = sum(f['same'] for f in fold.values())
FULL_CRASH = sum(f['crash'] for f in fold.values())
GATE_DONE = FULL_DONE >= corpus_n
# a stopped gate: everything below the smallest unrun model's input size has been run
GATE_STOPPED = (not GATE_DONE) and os.path.exists(f'{GATE}/full.stopped')
_done = {r['inp'] for r in full_main}
_rem = [l.strip()[len('corpus1729/'):] for l in open(f'{GATE}/full_by_size.txt') if l.strip() and l.strip()[len('corpus1729/'):] not in _done]
_sz = lambda p: os.path.getsize(f'{CORPUS}/{p}')
CUT_B = min((_sz(p) for p in _rem), default=0)
GATE_EXTRA = sum(1 for r in full_main if _sz(r['inp']) >= CUT_B) if _rem else 0
GATE_REMAIN = len(_rem)
CUT_TXT = f'{CUT_B / 1e6:.2f} MB' if CUT_B >= 1e6 else f'{CUT_B / 1e3:.0f} KB'
MAX_TXT = f'{max((_sz(p) for p in _rem), default=0) / 1e6:.0f} MB'
# time to run the rest, from the stratified sample (equal quartiles, so the plain mean is fair);
# a timed-out run counts at its cap, so this is a lower bound
_cpu = [(300.0 if not isinstance(r.get('t_ref'), (int, float)) else r['t_ref']) +
        (300.0 if not isinstance(r.get('t_test'), (int, float)) else r['t_test']) for r in sample_rows]
EST_H = (sum(_cpu) / len(_cpu)) * (len(_rem) - N_SAMPLE) / 2 / 3600 if _cpu else 0   # two jobs in parallel
EMPTY_OUT = sum(1 for r in full if r.get('out_bytes') == 0 and not inconclusive(r) and not crashed(r) and r['out_same'] and r['rpt_same'])
FULL_COMPARED = FULL_DONE - FULL_INC
diffs = [r for r in full if not inconclusive(r) and not crashed(r) and not (r['out_same'] and r['rpt_same'])]
errs = sum(1 for r in full if r.get('rpt_err') and not inconclusive(r))

wasm = [r for r in jl(f'{GATE}/wasm.jsonl') if not r.get('timeout')]
WASM_N = len(wasm)
WASM_SAME = sum(1 for r in wasm if r['wasm_out'] and r['wasm_rpt'])
PLAT_DIFF = sum(1 for r in wasm if not (r['native_out'] and r['native_rpt']))

# coverage census of shapes
cov = [l.strip().replace('corpus1729/', '') for l in open(f'{GATE}/coverage.txt') if l.strip()]
SHAPES = ['CIRCULAR', 'FORCE_MAIN', 'FILLED_CIRCULAR', 'RECT_CLOSED', 'RECT_OPEN', 'TRAPEZOIDAL', 'TRIANGULAR',
          'HORIZ_ELLIPSE', 'VERT_ELLIPSE', 'ARCH', 'PARABOLIC', 'POWER', 'RECT_TRIANGULAR', 'RECT_ROUND',
          'MODBASKETHANDLE', 'EGG', 'HORSESHOE', 'GOTHIC', 'CATENARY', 'SEMIELLIPTICAL', 'BASKETHANDLE',
          'SEMICIRCULAR', 'IRREGULAR', 'CUSTOM', 'STREET', 'DUMMY']
ENUM = {'POWER': 'POWERFUNC', 'RECT_TRIANGULAR': 'RECT_TRIANG', 'MODBASKETHANDLE': 'MOD_BASKET', 'EGG': 'EGGSHAPED', 'STREET': 'STREET_XSECT'}
shape_census = []
for s in SHAPES:
    n = sum(1 for v in features.values() if s in v.get('shapes', {}))
    c = sum(1 for k in cov if s in features.get(k, {}).get('shapes', {}))
    shape_census.append([ENUM.get(s, s), n, c])
shape_census.sort(key=lambda r: -r[1])

# ------------------------------------------------------------------ numbers for text
priv = sum(len(v) for v in lift['private_module_vars'].values())
NUM = dict(
    FUNCS=lift['functions'], GLOBALS=lift['globals_h_members'], SHARED=len(lift['shared_module_vars']),
    PRIVATE=priv, PRIVATE_FILES=len(lift['private_module_vars']),
    MUTABLE=lift['globals_h_members'] + len(lift['shared_module_vars']) + priv,
    TABLES=len(lift['kept_tables']), CONSTS=len(lift['kept_const']), EDITS=f"{lift['edits']:,}",
    PRIV_REFS=f"{lift['private_ref_rewrites']:,}", MACRO_REFS=f"{lift['macro_arg_refs']:,}",
    CFILES=54, CLINES='46,467', EXTERN_FILES=12,
    LEAK_BYTES='361,322', LEAK_ALLOCS=134,
    FULL_DONE=f'{FULL_DONE:,}', FULL_SAME=f'{FULL_SAME:,}', FULL_DIFF=f'{FULL_DIFF:,}', FULL_INC=f'{FULL_INC:,}',
    FULL_CRASH=FULL_CRASH, EMPTY_OUT=f'{EMPTY_OUT:,}', FUNCS_NOW=870, FUNCS_OUT=97,
    PRIV_STATIC=226, PRIV_EXTERN=47,
    GATE_STATE=(f'It has finished all {FULL_DONE:,}' if GATE_DONE else
                f'It ran {FULL_DONE:,} of them: every model with less than {CUT_TXT} of input, {GATE_EXTRA} larger ones from an earlier pass, and a seeded random sample of {N_SAMPLE} of the {GATE_REMAIN:,} larger models, {N_SAMPLE // 4} from each size quartile up to {MAX_TXT}' if GATE_STOPPED else
                f'At the time this was written it had finished {FULL_DONE:,}'),
    GATE_REST=('' if GATE_DONE else f'. The other {GATE_REMAIN - N_SAMPLE:,} large models have not been run; at the sample’s average run time they would take at least {EST_H:.0f} more hours on this two-core machine' if GATE_STOPPED else '; the rest were still running'),
    GATE_LIMIT=(f'The corpus gate ran all {corpus_n:,} models' if GATE_DONE else
                f'The corpus gate ran every model under {CUT_TXT} of input but only a sample of {N_SAMPLE} of the {GATE_REMAIN:,} larger ones; the other {GATE_REMAIN - N_SAMPLE:,} have not been run' if GATE_STOPPED else
                f'The corpus gate was still running when this was written ({FULL_DONE:,} of {corpus_n:,} done)'),
    GATE_SHORT=(f'all {corpus_n:,}' if GATE_DONE else f'{FULL_DONE:,} of {corpus_n:,}: all under {CUT_TXT}, plus a sample of {N_SAMPLE} larger' if GATE_STOPPED else f'{FULL_DONE:,} of {corpus_n:,} so far'),
    CORPUS_N=f'{corpus_n:,}', WASM_N=WASM_N, WASM_SAME=WASM_SAME, PLAT_DIFF=PLAT_DIFF,
    MULTI_STEPS='591,096',
    CPL_PEAK_C=f"{max(coupled['coupled']):.1f}", CPL_PEAK_M=f"{max(coupled['merged']):.1f}",
    CPL_PEAK_PCT=f"{100 * (max(coupled['coupled']) - max(coupled['merged'])) / max(coupled['merged']):+.1f}%",
    CPL_VOL_PCT=f"{100 * (sum(coupled['coupled']) - sum(coupled['merged'])) / sum(coupled['merged']):+.2f}%",
    T_O2=f"{b_o2['summary']['median_ratio']:.3f}", T_O2_FLOOR=f"±{100 * b_o2['summary']['max_abs_floor_dev']:.0f}%",
    T_LTO=f"{b_lto['summary']['median_ratio']:.3f}", T_LTO_FLOOR=f"±{100 * b_lto['summary']['max_abs_floor_dev']:.1f}%",
    GLOBAL_MODULES=len(cm['modules']),
)
NUM_GATE_SHORT = NUM['GATE_SHORT']
def sub(s):
    return re.sub(r'\{([A-Z0-9_]+)\}', lambda m: str(NUM.get(m.group(1), m.group(0))), s)

# ------------------------------------------------------------------ figures
multi6 = [dict(name='extran1_semi_elliptical', steps=1440), dict(name='extran1_power', steps=1440),
          dict(name='exam79', steps=8641), dict(name='high_elevation_model', steps=12465),
          dict(name='transect_example', steps=710), dict(name='attenuation_DW_High_CE', steps=566400)]
gate_note = ((f'All {corpus_n:,} corpus models run; ' if GATE_DONE else
              f'{FULL_DONE:,} of {corpus_n:,} models: all under {CUT_TXT}, plus a sample of {N_SAMPLE} larger; ' if GATE_STOPPED else
              f'{FULL_DONE:,} of {corpus_n:,} corpus models run so far; ') +
             f'{FULL_SAME:,} identical, {FULL_CRASH} crash both engines alike, {FULL_DIFF} different, {FULL_INC} inconclusive. Log scale.')
# ------------------------------------------------------------------ speed study (results/perf in the repo)
PERF = f'{REPO}/results/perf'
def ratios(bench, b, key='cpu'):
    R = bench['results']; per = []
    for m in R:
        pr = [y[key] / x[key] for x, y in zip(R[m]['ref'], R[m][b])]
        per.append(sorted(pr)[len(pr) // 2])
    geo = math.exp(sum(math.log(v) for v in per) / len(per))
    return geo, per
bf, br4, bomp, bompb = J(f'{PERF}/bench_flags.json'), J(f'{PERF}/bench_r4.json'), J(f'{PERF}/bench_omp2.json'), J(f'{PERF}/bench_omp_big.json')
be1, be2 = J(f'{PERF}/bench_e1.json'), J(f'{PERF}/bench_e2.json')
mvf, mve2, optx = J(f'{PERF}/movement_flags.json'), J(f'{PERF}/movement_e2.json'), J(f'{PERF}/options_exp.json')
cats = J(f'{PERF}/profile_categories.json')
SP = {}
for lab, bench, b in [('o3', bf, 'o3'), ('lto', bf, 'lto'), ('pgo', bf, 'pgo'), ('pgolto', bf, 'pgolto'), ('fma', bf, 'fma'), ('fastmath', bf, 'fastmath'),
                      ('omp1', bf, 'omp1'), ('cplus', bf, 'cplus'), ('r4', br4, 'cplus_memo_pgo'), ('r4lto', br4, 'cplus_memo_lto'),
                      ('memo', be1, 'memo'), ('warm', be2, 'warm')]:
    SP[lab] = ratios(bench, b)
noise_per = ratios(bf, 'ref2')[1] + ratios(br4, 'ref2')[1]
NOISE = (min(noise_per), max(noise_per))
omp_rows = [(m, ratios({'results': {m: bomp['results'][m]}}, 'omp2', 'wall')[0]) for m in bomp['results']] + \
           [(m, ratios({'results': {m: bompb['results'][m]}}, 'omp2', 'wall')[0]) for m in bompb['results']]
LINKS = {'Semi_Real_Models/375_H&H_Elements (1).inp': 197, 'XPSWMM/MAYBR.inp': 122, 'Hydrology/46_Subs.inp': 69, 'Hydrology/10_Subs_Continuous.inp': 9,
         'SWMM5_NCIMM/lid1d.inp': 333, 'Training/Exercise5_transect.inp': 46, 'WQ/Peter Klaver_WQ_Tracer.inp': 2, 'Hydraulics/short_trap_SW5.inp': 1}
for m in bompb['results']:   # link counts of the two big models from their .out headers
    import struct, subprocess as _sp, tempfile as _tf
    with _tf.TemporaryDirectory(dir='/tmp/claude-0') as _d:
        _sp.run([f'{GATE}/final/runswmm_cp', f'{CORPUS}/{m}', _d + '/m.rpt', _d + '/m.out'], cwd=os.path.dirname(f'{CORPUS}/{m}'), capture_output=True)
        LINKS[m] = struct.unpack('<i', open(_d + '/m.out', 'rb').read(24)[20:24])[0]
omp_rows = [(m, r, LINKS.get(m, 0)) for m, r in sorted(omp_rows, key=lambda x: LINKS.get(x[0], 0))]
# speed against movement points
mv_pts = []
bm = {m: i for i, m in enumerate(bf['results'])}
def per_model(bench, b):
    R = bench['results']; return {m: 1 / sorted([y['cpu'] / x['cpu'] for x, y in zip(R[m]['ref'], R[m][b])])[1] for m in R}
for lab, col, mv, sp in [('FMA', 'var(--f-a)', {m: v['fma'] for m, v in mvf.items()}, per_model(bf, 'fma')),
                         ('fast-math', 'var(--f-e)', {m: v['fastmath'] for m, v in mvf.items()}, per_model(bf, 'fastmath')),
                         ('warm start', 'var(--f-b)', {m: v['warm'] for m, v in mve2.items()}, per_model(be2, 'warm'))]:
    for m, x in mv.items():
        lf = x.get('linkFlow') or {}
        mv_pts.append((sp[m], lf.get('peakMax') or 0, lab, col))
for m, v in optx.items():
    base = v['variants'][0]['cpu']
    for r in v['variants'][1:]:
        lf = (r.get('diff') or {}).get('linkFlow') or {}
        lab = 'step x2' if r['name'] == 'ROUTING_STEP x2' else 'step x10' if r['name'] == 'ROUTING_STEP x10' else 'other routing options'
        col = 'var(--f-c)' if lab == 'step x2' else 'var(--f-d)' if lab == 'step x10' else 'var(--f-dim)'
        mv_pts.append((base / r['cpu'], lf.get('peakMax') or 0, lab, col))
NUM.update(dict(
    SP_LTO=f"{SP['lto'][0]:.2f}", SP_PGOLTO=f"{SP['pgolto'][0]:.2f}", SP_R4=f"{SP['r4'][0]:.2f}", SP_R4_WQ=f"{SP['r4'][1][6]:.2f}",
    SP_MEMO_WQ=f"{SP['memo'][1][6]:.2f}", SP_FMA=f"{SP['fma'][0]:.2f}", SP_FAST=f"{SP['fastmath'][0]:.2f}", SP_WARM=f"{SP['warm'][0]:.2f}",
    SP_WARM_SHORT=f"{SP['warm'][1][7]:.2f}", SP_OMP1=f"{SP['omp1'][0]:.2f}", SP_OMP1_MAX=f"{max(SP['omp1'][1]):.2f}",
    SP_NOISE=f"{NOISE[0]:.2f}-{NOISE[1]:.2f}",
    OMP_SLOWER=sum(1 for _, r, _n in omp_rows if r > 1.05), OMP_N=len(omp_rows), OMP_MAX=f"{max(r for _, r, _n in omp_rows):.2f}",
    OMP_BEST=f"{min(r for _, r, _n in omp_rows):.2f}", OMP_GAIN=f"{100 * (1 - min(r for _, r, _n in omp_rows)):.0f}"))

swmm6_loc = J(f'{REPO}/docs/swmm6_loc.json')   # tools/loc_buckets.py
FIGS = dict(
    speed_profile=F.fig_speed_profile(cats),
    swmm6_size=F.fig_swmm6_size(swmm6_loc), swmm6_repo=F.fig_swmm6_repo(swmm6_loc),
    speed_identical=F.fig_speed_identical([
        ('EPA C, -O3, hardening off', SP['o3'][0], SP['o3'][1], 'epa'),
        ('EPA C, link-time optimization', SP['lto'][0], SP['lto'][1], 'epa'),
        ('EPA C, profile-guided', SP['pgo'][0], SP['pgo'][1], 'epa'),
        ('EPA C, -O3 + LTO + profile-guided', SP['pgolto'][0], SP['pgolto'][1], 'epa'),
        ('swmm5-cplus be57fec, -O2 -flto', SP['cplus'][0], SP['cplus'][1], 'cplus'),
        ('swmm5-cplus R4, -O2 -flto', SP['r4lto'][0], SP['r4lto'][1], 'cplus'),
        ('swmm5-cplus R4, -O3 LTO + PGO', SP['r4'][0], SP['r4'][1], 'cplus')], NOISE),
    speed_threads=F.fig_speed_threads(omp_rows),
    speed_movement=F.fig_speed_movement(mv_pts),
    state=F.fig_state(lift), lookup=F.fig_lookup(), pipeline=F.fig_pipeline(lift),
    shapes=F.fig_shapes(matrix_txt), multi=F.fig_multi(multi6), coupled=F.fig_coupled(coupled),
    timing=F.fig_timing(b_o2, b_lto), heat=F.fig_heat(cm), gate=F.fig_gate(list(fold.values()), gate_note))

# ------------------------------------------------------------------ SWMM6 comparison (results/swmm6 in the repo)
sys.path.insert(0, f'{AB}/doc')
import build6
_n6, _f6 = build6.build(REPO, NUM, LINKS)
NUM.update(_n6); FIGS.update(_f6)

# ------------------------------------------------------------------ essay -> html + json
def code_html(t): return '<pre class="code"><code>' + html.escape(t) + '</code></pre>'
parts = [f'<h1>{html.escape(E.TITLE)}</h1><p class="dek">{html.escape(E.SUBTITLE)}</p>']
toc = []; essay_json = dict(title=E.TITLE, subtitle=E.SUBTITLE, blocks=[]); fign = 0
for b in E.BLOCKS:
    k = b[0]
    if k == 'h2':
        parts.append(f'<h2 id="{b[2]}">{html.escape(b[1])}</h2>'); toc.append([b[2], b[1]])
        essay_json['blocks'].append(dict(kind='h2', text=b[1]))
    elif k == 'p':
        parts.append('<p>' + sub(b[1]) + '</p>'); essay_json['blocks'].append(dict(kind='p', html=sub(b[1])))
    elif k == 'fig':
        fign += 1
        cap = f'Figure {fign}. ' + sub(b[2])
        parts.append(f'<figure class="fig"><div class="frame">{FIGS[b[1]]}</div><figcaption>{html.escape(cap)}</figcaption></figure>')
        essay_json['blocks'].append(dict(kind='fig', key=b[1], caption=cap))
    elif k == 'code':
        parts.append(code_html(b[2])); essay_json['blocks'].append(dict(kind='code', text=b[2]))
    elif k == 'table':
        rows = [[sub(c) for c in r] for r in b[2]]
        parts.append('<div class="tablewrap"><table class="t"><thead><tr>' + ''.join(f'<th>{h}</th>' for h in b[1]) +
                     '</tr></thead><tbody>' + ''.join('<tr>' + ''.join(f'<td>{c}</td>' for c in r) + '</tr>' for r in rows) + '</tbody></table></div>')
        essay_json['blocks'].append(dict(kind='table', head=b[1], rows=rows))
    elif k in ('ul', 'ol'):
        items = [sub(x) for x in b[1]]
        parts.append(f'<{k}>' + ''.join(f'<li>{x}</li>' for x in items) + f'</{k}>')
        essay_json['blocks'].append(dict(kind=k, items=items))
essay_html = '\n'.join(parts)
_left = sorted(set(re.findall(r'\{[A-Z][A-Z0-9_]+\}', essay_html)))
if _left: raise SystemExit('unfilled placeholders in the essay: ' + ', '.join(_left))
os.makedirs(f'{AB}/doc/out', exist_ok=True)
json.dump(essay_json, open(f'{AB}/doc/out/essay.json', 'w'), indent=1)
for key, s in FIGS.items():
    open(f'{AB}/doc/out/fig_{key}.svg', 'w').write(F.svg_for_docx(s))

# ------------------------------------------------------------------ samples
SAMPLE_SPEC = [
    ('exam79', 'EXTRAN Example 79, tidal boundary'),
    ('extran1_egg', 'EXTRAN Example 1, egg-shaped conduits'),
    ('extran1_irregular', 'EXTRAN Example 1, irregular transects'),
    ('siphon_larger_SW5', 'Surcharged 2-ft siphon'),
    ('controlExample', 'Control rules on a pump'),
    ('small_pump_fm_model', 'Pump into a force main'),
    ('high_elevation_model', 'High-elevation network'),
    ('transect_example', 'Natural channel transects'),
]
def opt(text, key):
    m = re.search(r'^\s*' + key + r'\s+(\S+)', text, re.M | re.I); return m.group(1) if m else None
def parse_dt(d, t):
    for fmt in ('%m/%d/%Y %H:%M:%S', '%m/%d/%Y %H:%M'):
        try: return datetime.datetime.strptime(f'{d} {t}', fmt)
        except Exception: pass
    return None
samples = []
for key, name in SAMPLE_SPEC:
    t = open(f'{CORPUS}/EPA/{key}.inp', encoding='latin-1').read()
    a = parse_dt(opt(t, 'START_DATE'), opt(t, 'START_TIME') or '00:00:00')
    b = parse_dt(opt(t, 'END_DATE'), opt(t, 'END_TIME') or '00:00:00')
    days = (b - a).total_seconds() / 86400 if a and b else 1
    samples.append(dict(key=key, file=key + '.inp', name=name, units=opt(t, 'FLOW_UNITS') or 'CFS', days=days, text=t))
coupling = {k: open(f'{REPO}/examples/models/{f}.inp').read() for k, f in [('up', 'upstream'), ('dn', 'downstream'), ('merged', 'merged')]}

# ------------------------------------------------------------------ schematic
def schematic():
    W, H = 420, 210
    b = ['<defs><marker id="sa" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8 z" fill="var(--epa)"/></marker></defs>']
    def node(x, y, lab, kind='j', below=False):
        if kind == 'o':
            b.append(f'<path d="M{x - 8} {y - 7} L{x + 8} {y} L{x - 8} {y + 7} z" fill="var(--f-face)" stroke="var(--f-ink)" stroke-width="1.5"/>')
        else:
            b.append(f'<circle cx="{x}" cy="{y}" r="6" fill="var(--f-face)" stroke="var(--f-ink)" stroke-width="1.5"/>')
        b.append(F.text(x, y + 22 if below else y - 12, lab, 11, anchor='middle', weight=600))
    def pipe(x1, y1, x2, y2, lab):
        b.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="var(--cpp)" stroke-width="3"/>')
        b.append(F.text((x1 + x2) / 2, (y1 + y2) / 2 + 16, lab, 10, 'var(--f-dim)', 'middle'))
    def sc(x, y, lab, area):
        b.append(f'<rect x="{x - 22}" y="{y - 12}" width="44" height="24" rx="3" fill="var(--f-soft)" stroke="var(--f-line)"/>')
        b.append(F.text(x, y + 4, f'{lab} {area} ac', 9, 'var(--f-dim)', 'middle'))
    b.append(F.text(14, 18, 'upstream.inp', 11, 'var(--f-dim)', weight=600))
    sc(40, 50, 'S1', 40); node(40, 90, 'J1'); pipe(46, 90, 134, 90, 'C1 2 ft'); node(140, 90, 'J2'); sc(140, 50, 'S2', 25)
    pipe(146, 90, 222, 90, 'C2 2.5 ft'); node(230, 90, 'OA', 'o')
    b.append(F.text(14, 136, 'downstream.inp', 11, 'var(--f-dim)', weight=600))
    node(250, 170, 'IN', below=True); pipe(256, 170, 324, 170, 'C3 3 ft'); node(330, 170, 'J3'); sc(330, 132, 'S3', 30)
    pipe(336, 170, 392, 170, 'C4'); node(400, 170, 'OB', 'o')
    b.append('<path d="M238 96 C 260 120, 250 140, 250 158" fill="none" stroke="var(--epa)" stroke-width="2" stroke-dasharray="5 4" marker-end="url(#sa)"/>')
    b.append(F.text(234, 126, 'flow at OA set as', 10, 'var(--epa)', 'end'))
    b.append(F.text(234, 138, 'lateral inflow at IN', 10, 'var(--epa)', 'end'))
    return f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" font-family="{F.FONT}" style="width:100%;height:auto;display:block;margin:6px 0 10px">{"".join(b)}</svg>'

# ------------------------------------------------------------------ inside / defects / verify / prov
inside_tiles = [[str(lift['functions']), 'functions became members of class Project in Stage 1; 870 remain after R2–R3 moved 97 out'],
                [str(NUM['MUTABLE']), 'mutable variables became per-Project fields'],
                ['20', 'switches on a type code; the 8 in cross-section geometry are now 26 classes'],
                [NUM['EDITS'], 'edits made by the generator, each located by libclang']]
ladder = dict(head=['Commit', 'Step', 'Source lines (+/−)', 'Gate'], rows=[
    ['<code>34066dc</code>', 'EPA SWMM 5.2.4 solver, byte-identical to the USEPA tag', '—', 'reference'],
    ['<code>8e0fa39</code>', '<b>Stage 1.</b> Mechanical lift by make_cplus.py: class Project, PIMPL module state, handle API', '+7,346 / −5,369', '40/40 smoke'],
    ['<code>795fd37</code>', '<b>R1.</b> Idempotent close, bounded paths, no descriptor leak; ~Project closes', '+124 / −7', '40/40; ASan clean; 6 models round-robin and threaded'],
    ['<code>adbddee</code>', '<b>R2.</b> findroot templates, OdeSolver object, MathExprParser per formula', '+459 / −548', '187/187 coverage'],
    ['<code>c97a83a</code>', '<b>R3.</b> Cross-section geometry as 26 shape classes, pure functions of the section', '+2,937 / −2,004', '187/187 coverage (all 26 shapes)'],
    [f'<code>{commit[:7]}</code>', '<b>R3b.</b> Shape table at namespace scope; swmm::Simulation; coupling example', '+157 / −57', f'{FULL_SAME:,}/{FULL_COMPARED:,} corpus (so far)'],
    ['<code>aa0ae2b</code>', '<b>R4.</b> Outfall normal and critical depth reused when the flow repeats; profile-guided build (native only; the engines in this app are be57fec)', '+23 / −2', '217/221 smoke and coverage; 4 crash decks crash both alike'],
])
defects = [
    dict(id='S5CP-1', title='swmm_close() leaves its FILE pointers dangling', status='fixed in R1', statusKind='ok',
         body='A second close writes the run-time lines into a freed FILE and closes it again (AddressSanitizer: double free). Each pointer is now cleared after fclose.',
         code="    report_writeSysTime();\n    if ( Finp.file != NULL )\n        fclose(Finp.file);\n    if ( Frpt.file != NULL )\n        fclose(Frpt.file);\n    if ( Fout.file != NULL )\n    {\n        fclose(Fout.file);\n        if ( Fout.mode == SCRATCH_FILE ) remove(Fout.name);\n    }"),
    dict(id='S5CP-2', title='getAbsolutePath() can overrun InpDir (260 bytes)', status='fixed in R1', statusKind='ok',
         body='realpath() needs a PATH_MAX (4,096-byte) buffer, and the absolute branch copies strlen(fname) bytes whatever size says. glibc fortify aborts EPA’s own runswmm on any relative input path. Called as getAbsolutePath(f1, InpDir, sizeof(InpDir)).',
         code="    // --- if fname has a relative path then retrieve its full path\n    if (isRelativePath(fname))\n    {\n        #ifdef WINDOWS\n            GetFullPathName((LPCSTR)fname, (DWORD)size, (LPSTR)absPath, NULL);\n        #else\n            realpath(fname, absPath);\n        #endif\n    }\n\n    // --- otherwise copy fname to absPath\n    else\n    {\n        sstrncpy(absPath, fname, strlen(fname));\n    }"),
    dict(id='S5CP-3', title='One leaked file descriptor per scratch file', status='descriptor fixed; TEMPDIR open', statusKind='hi',
         body='On POSIX, mkstemp() returns an open descriptor that is discarded. Scratch files also go to the working directory, ignoring the TEMPDIR option.',
         code='    sstrncpy(fname, "swmmXXXXXX", MAXFNAME);\n    mkstemp(fname);\n    return fname;'),
    dict(id='S5CP-4', title='swmm_open() over an open project abandons it', status='legacy API unchanged; handles avoid it', statusKind='hi',
         body='The open flag is reset, not checked. The second open returns 0; the first model’s report stays empty and 361,322 bytes in 134 allocations leak (AddressSanitizer; a one-model control leaks none).',
         code='        IsOpenFlag = FALSE;\n        IsStartedFlag = FALSE;\n        ExceptionCount = 0;\n\n        // --- open a SWMM project\n        strcpy(InpDir, "");\n        project_open(f1, f2, f3);'),
    dict(id='S5CP-5', title='Two TINYs: consts.h says 1.E-6, odesolve.c says 1.0e-30', status='neutralized in R2', statusKind='ok',
         body='Correct in C only because odesolve.c never includes consts.h. Combined, the preprocessor would change the integrator’s constant by 24 orders of magnitude. The C++ constants are kTiny and friends.', code=None),
    dict(id='S5CP-6', title='xsect.dat gives 92 variables (49 tables, 43 sizes) external linkage', status='open; single definition since R3', statusKind='panel',
         body='The tables are plain non-const globals, so only one file may include xsect.dat, and nothing but convention stops a write. Since R3 only xsect_geometry.cpp includes it, and the 12 that xsect_setParams needs are declared in its header; the linkage is still external.',
         code='int    N_A_Circ   = 51;\ndouble A_Circ[51] =     // A/Afull v. Y/Yfull'),
    dict(id='S5CP-7', title='Dead declarations in infil.h', status='noted', statusKind='panel',
         body='infil.h declares extern THorton* HortInfil, TGrnAmpt* GAInfil and TCurveNum* CNInfil; no file defines them.', code=None),
    dict(id='S5CP-8', title='Integrator work arrays are linker-visible globals', status='fixed in R2', statusKind='ok',
         body='odesolve.c defines nmax, y, yscal, yerr, ytemp, dydx and ak with external linkage. They now live in an OdeSolver object.', code=None),
    dict(id='S5CP-9', title='Twelve files wire into other modules with local extern declarations', status='open (made explicit)', statusKind='panel',
         body='The mass-balance totals, statistics arrays and output vectors are reached around the headers. The 28 such variables are now named Project members, so the wiring is visible, but it is still there.', code=None),
    dict(id='S5CP-10', title='report.c and error.c disagree on the size of ErrString', status='gone in C++', statusKind='ok',
         body='Found by link-time optimization, which checks every declaration against its definition. Harmless today because the array is only passed on as a pointer, but undefined behavior in C. In swmm5-cplus it is one Project member with one type.',
         code='// report.c, line 72\nextern char   ErrString[81];           // defined in ERROR.C\n\n// error.c, line 28\nchar  ErrString[256];'),
]
parity = (f'<b>Parity contract.</b> swmm5-cplus matches EPA SWMM 5.2.4 built from the official v5.2.4 tag on the same compiler family: '
          f'the <code>.out</code> file byte for byte, and the <code>.rpt</code> byte for byte after removing the three wall-clock lines '
          f'(analysis begun, ended, elapsed). Native: GCC 13.3, EPA at <code>-O2</code> with Ubuntu’s default fortify (so the gate passes absolute paths; see S5CP-2), swmm5-cplus at <code>-O2 -flto</code>, the build that ships. '
          f'WebAssembly: Clang 18 with wasi-libc, EPA at <code>-O2</code>, swmm5-cplus at <code>-O2 -flto</code>. The match holds across that flag difference. '
          f'A run that both engines abandon at the same limit is reported as inconclusive, never as a match.')
verify_tiles = [[f'{FULL_SAME:,} / {FULL_COMPARED - FULL_CRASH:,}', f'full corpus, native, byte-identical; {FULL_CRASH} crash decks crash both alike ({NUM_GATE_SHORT})'],
                ['187 / 187', 'coverage set: all 26 shapes and every feature found, incl. 36 error decks'],
                [f'{WASM_SAME} / {WASM_N}', 'WebAssembly: swmm5-cplus vs EPA C, same toolchain'],
                [NUM['T_LTO'] + '×', 'run time vs EPA with LTO on both (noise floor ' + NUM['T_LTO_FLOOR'] + ')']]
controls = [
    'Null control: each engine run twice on the same input gives the same bytes; byte-identical results are reported as such and the comparison stops there.',
    f'Isolation control: six models stepped round-robin in one instance, and on six threads, each compared with its own solo EPA run: 6/6 identical in each mode, native under AddressSanitizer and in WebAssembly.',
    'Leak control: the two-open experiment leaks 361,322 bytes; the same driver opening one model leaks nothing.',
    f'Platform control: EPA’s C source differs from itself between native and WebAssembly on {PLAT_DIFF} of {WASM_N} models; both WebAssembly engines agree on all of them, so those differences are the platform’s.',
    'Timing control: EPA, C++, EPA again, interleaved, five repetitions; the second EPA run sets the noise floor quoted beside every ratio.',
    f'Crash control: {FULL_CRASH} crash decks from the OpenSWMM code review kill both engines with the same signal and leave no output. They are counted apart, not as identical.',
    f'Error decks: {EMPTY_OUT} of the identical runs stop on an input error and write no .out; for those the .rpt comparison is the whole test.',
    'Census of the test set: the coverage set was chosen by feature, and the shape table on this tab shows how many corpus models use each shape.',
]
limits = ('<b>What this does not show.</b> It is not a new engine and not more accurate: every number is EPA 5.2.4’s, defects included. '
          'It is not faster. It does not promise that native and browser runs give identical bits. It fixes none of EPA’s crashes. '
          + ('' if GATE_DONE else f'The full gate ran only a sample of {N_SAMPLE} of the {GATE_REMAIN:,} models over {CUT_TXT}. ' if GATE_STOPPED else f'The full gate was still running ({FULL_DONE:,} of {corpus_n:,}). ') + f'Thread safety was tested, not audited. The coupling is a one-way demonstration, not a validated method. '
          'The design is a first floor: Project is still one large class and the modules still share the big arrays.')
gate_csv = 'folder,models,identical,both_crash,different,inconclusive\n' + ''.join(f"{f['name']},{f['n']},{f['same']},{f['crash']},{f['diff']},{f['inc']}\n" for f in fold.values())

def sha_file(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
W = f'{REPO}/build/wasm'
prov = [['engine commit', commit], ['swmm5cplus.wasm sha256', sha_file(f'{W}/swmm5cplus.wasm')],
        ['swmm524_epa.wasm sha256', sha_file(f'{W}/swmm524_epa.wasm')],
        ['EPA reference', 'USEPA/Stormwater-Management-Model tag v5.2.4'],
        ['native toolchain', 'GCC 13.3.0. Gate: EPA -O2 (Ubuntu default fortify; absolute paths), swmm5-cplus -O2 -flto. Timing: both -O2, then both -O2 -flto'],
        ['wasm toolchain', 'Clang 18.1.3, wasi-libc 0.0~git20230113, reactor. EPA -O2, swmm5-cplus -O2 -flto'],
        ['in-browser file system', 'MemWasi, 13 WASI calls, in memory (this page)'],
        ['built', '2026-10-28']]

data = dict(samples=samples, coupling=coupling, schematic=schematic(), figs=FIGS, insideTiles=inside_tiles,
            ladder=ladder, matrix=[[l.split()[0], l.split()[1]] for l in matrix_txt.splitlines()
                                   if len(l.split()) == 2 and len(l.split()[1]) == 8 and set(l.split()[1]) <= set('x.')],
            shapeCensus=shape_census, defects=defects,
            verify=dict(parity=parity, tiles=verify_tiles, controls=controls, limits=limits, csv=gate_csv),
            prov=prov, commit=commit, essayHtml=essay_html, toc=toc, leak=dict(bytes=361322, allocs=134))

DOCX = f'{AB}/doc/out/swmm5-cplus-doc.docx'   # built by doc/prep_docx.py + doc/build_docx.js from this run's essay.json
tpl = open(f'{AB}/app_template.html').read()
tpl = tpl.replace('{MUTABLE}', str(NUM['MUTABLE'])).replace('{N}', 'separate')
out = (tpl.replace('/*__WASM_CPLUS__*/', base64.b64encode(open(f'{W}/swmm5cplus.wasm', 'rb').read()).decode())
          .replace('/*__WASM_EPA__*/', base64.b64encode(open(f'{W}/swmm524_epa.wasm', 'rb').read()).decode())
          .replace('/*__ENGINE_CORE__*/', open(f'{AB}/engine_core.js').read())
          .replace('/*__CMP__*/', open(f'{AB}/rpt_compare.js').read())
          .replace('/*__DOCX__*/', base64.b64encode(open(DOCX, 'rb').read()).decode() if os.path.exists(DOCX) else '')
          .replace('/*__DATA__*/', json.dumps(data).replace('</', '<\\/')))
open(f'{AB}/swmm5-cplus.html', 'w').write(out)
print('wrote', len(out) // 1024, 'KB;', 'full', FULL_DONE, FULL_SAME, FULL_DIFF, FULL_INC, '| wasm', WASM_SAME, WASM_N, PLAT_DIFF, '| diffs', [d['inp'] for d in diffs][:5], '| stopped', GATE_STOPPED, 'cut', CUT_TXT, 'extra', GATE_EXTRA, 'remain', GATE_REMAIN, 'est_h', round(EST_H))
