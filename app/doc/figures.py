#!/usr/bin/env python3
"""Figures for the swmm5-cplus Doc tab and .docx, drawn from measured data.

Every figure is an SVG string whose colours are CSS custom properties
(--f-ink, --f-dim, --f-line, --f-face, --f-a, --f-b, --f-c, --f-d), so the app
themes them; svg_for_docx() substitutes the light-theme values for rasterising.
"""
import json, math, html

FONT = "IBM Plex Sans, Helvetica, Arial, sans-serif"
MONO = "IBM Plex Mono, Menlo, Consolas, monospace"
LIGHT = {'--f-ink': '#1b2733', '--f-dim': '#5a6878', '--f-line': '#cfd8e3', '--f-face': '#ffffff',
         '--f-soft': '#eef3f8', '--f-a': '#1f6fd1', '--f-b': '#c77700', '--f-c': '#1d8a4c',
         '--f-d': '#c2352b', '--f-e': '#6b4fbb'}

def esc(s): return html.escape(str(s))

def svg(w, h, body, label):
    return (f'<svg viewBox="0 0 {w} {h}" xmlns="http://www.w3.org/2000/svg" role="img" '
            f'aria-label="{esc(label)}" font-family="{FONT}" style="max-width:100%;height:auto">'
            f'<rect x="0" y="0" width="{w}" height="{h}" fill="var(--f-face)"/>{body}</svg>')

def text(x, y, s, size=12, fill='var(--f-ink)', anchor='start', weight=400, family=None, extra=''):
    fam = f' font-family="{family}"' if family else ''
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" '
            f'font-weight="{weight}"{fam} {extra}>{esc(s)}</text>')

def svg_for_docx(s):
    for k, v in LIGHT.items():
        s = s.replace(f'var({k})', v)
    return s.replace('style="max-width:100%;height:auto"', '')

# ----------------------------------------------------------------- F1 state census
def fig_state(lift):
    g = lift['globals_h_members']; sh = len(lift['shared_module_vars'])
    pv = sum(len(v) for v in lift['private_module_vars'].values())
    tb = len(lift['kept_tables']); cs = len(lift['kept_const'])
    W, H = 760, 300
    x0, x1 = 210, 720
    scale = (x1 - x0) / (g + sh + pv)
    b = []
    b.append(text(24, 34, 'Where EPA SWMM 5.2.4 keeps its variables, and where swmm5-cplus put them', 15, weight=600))
    def bar(y, parts, label, sub):
        b.append(text(24, y + 17, label, 13, weight=600))
        b.append(text(24, y + 34, sub, 11, 'var(--f-dim)'))
        x = x0
        for n, col, name in parts:
            w = n * scale
            b.append(f'<rect x="{x:.1f}" y="{y}" width="{w:.1f}" height="34" fill="{col}" rx="2"/>')
            if w > 40:
                b.append(text(x + w / 2, y + 22, f'{n}', 13, '#ffffff', 'middle', 600))
            x += w
        return x
    xe = bar(64, [(g, 'var(--f-a)', 'globals.h'), (sh, 'var(--f-d)', 'shared'), (pv, 'var(--f-c)', 'private')],
             'Mutable state', f'{g + sh + pv} variables, one copy per process')
    # legend under bar 1
    lx = 24; ly = 118
    for col, name in [('var(--f-a)', f'globals.h ({g})'), ('var(--f-d)', f'module variables reached from other files ({sh})'),
                      ('var(--f-c)', f'used in one file only ({pv}, in 32 files)')]:
        b.append(f'<rect x="{lx}" y="{ly - 10}" width="12" height="12" fill="{col}" rx="2"/>')
        b.append(text(lx + 17, ly, name, 11, 'var(--f-dim)'))
        lx += 17 + len(name) * 6.1 + 18
    bar(150, [(tb, 'var(--f-e)', 'tables'), (cs, 'var(--f-dim)', 'const')],
        'Immutable data', f'{tb + cs} tables and constants')
    lx = 24; ly = 204
    for col, name in [('var(--f-e)', f'read-only lookup tables ({tb})'), ('var(--f-dim)', f'const values ({cs})')]:
        b.append(f'<rect x="{lx}" y="{ly - 10}" width="12" height="12" fill="{col}" rx="2"/>')
        b.append(text(lx + 17, ly, name, 11, 'var(--f-dim)'))
        lx += 17 + len(name) * 6.1 + 18
    b.append(f'<line x1="24" y1="232" x2="{W - 24}" y2="232" stroke="var(--f-line)"/>')
    b.append(text(24, 256, f'swmm5-cplus: the {g + sh + pv} mutable variables become fields of one Project object, so each simulation owns its copy.', 12))
    b.append(text(24, 276, f'The {tb + cs} immutable ones stay shared at namespace scope, which is safe because nothing writes them.', 12))
    return svg(W, H, ''.join(b), 'Census of SWMM5 state variables')

# ----------------------------------------------------------------- F2 name lookup
def fig_lookup():
    W, H = 760, 330
    b = [text(24, 32, 'Why the lift is safe: an unqualified name is looked up in the same order', 15, weight=600)]
    def column(x, title, rows, note):
        b.append(text(x, 66, title, 13, weight=600))
        y = 82
        for i, (head, sub, col) in enumerate(rows):
            b.append(f'<rect x="{x}" y="{y}" width="320" height="52" rx="6" fill="var(--f-soft)" stroke="{col}" stroke-width="1.5"/>')
            b.append(text(x + 14, y + 22, head, 13, weight=600))
            b.append(text(x + 14, y + 40, sub, 11, 'var(--f-dim)'))
            if i < len(rows) - 1:
                b.append(f'<path d="M{x + 160} {y + 52} v14" stroke="var(--f-dim)" stroke-width="1.5" marker-end="url(#ah)"/>')
            y += 66
        b.append(text(x, y + 8, note, 11, 'var(--f-dim)'))
    b.append('<defs><marker id="ah" markerWidth="8" markerHeight="8" refX="4" refY="7" orient="auto">'
             '<path d="M0,0 L8,0 L4,8 z" fill="var(--f-dim)"/></marker></defs>')
    column(24, 'EPA SWMM5: a C function in dynwave.c', [
        ('1. Locals and parameters', 'double dq = ...;', 'var(--f-line)'),
        ('2. File statics of dynwave.c', 'static TXnode* Xnode;', 'var(--f-c)'),
        ('3. Process globals (globals.h)', 'TNode* Node;  int ErrorCode;', 'var(--f-d)')],
        'One copy of 2 and 3 per process: one model at a time.')
    column(416, 'swmm5-cplus: Project::dynwave_execute()', [
        ('1. Locals and parameters', 'unchanged', 'var(--f-line)'),
        ('2. Members of this Project', 'Node, ErrorCode, dynwave_->Xnode', 'var(--f-a)'),
        ('3. Namespace scope', 'read-only tables and constants only', 'var(--f-e)')],
        'One copy of 2 per Project: as many models as you like.')
    return svg(W, H, ''.join(b), 'Name lookup in C and in a C++ member function')

# ----------------------------------------------------------------- F3 pipeline
def fig_pipeline(lift):
    W, H = 760, 250
    b = ['<defs><marker id="ap" markerWidth="9" markerHeight="9" refX="8" refY="4.5" orient="auto">'
         '<path d="M0,0 L9,4.5 L0,9 z" fill="var(--f-dim)"/></marker></defs>']
    b.append(text(24, 30, 'The refactor is a program: every edit located by the compiler, every step gated by the output bytes', 14, weight=600))
    boxes = [('EPA 5.2.4', '54 .c files', '46,467 lines'),
             ('libclang', 'parse as C', 'resolve every name'),
             ('make_cplus.py', f'{lift["edits"]:,} edits', f'{lift["functions"]} functions'),
             ('C++17 tree', 'class Project', '+ hand redesigns'),
             ('compile', 'g++ 13 native', 'clang 18 wasm32')]
    x = 24; w = 128; gap = 22; y = 64
    for i, (t1, t2, t3) in enumerate(boxes):
        b.append(f'<rect x="{x}" y="{y}" width="{w}" height="74" rx="8" fill="var(--f-soft)" stroke="var(--f-a)" stroke-width="1.4"/>')
        b.append(text(x + w / 2, y + 24, t1, 13, anchor='middle', weight=600))
        b.append(text(x + w / 2, y + 44, t2, 11, 'var(--f-dim)', 'middle'))
        b.append(text(x + w / 2, y + 60, t3, 11, 'var(--f-dim)', 'middle'))
        if i < len(boxes) - 1:
            b.append(f'<path d="M{x + w + 2} {y + 37} h{gap - 6}" stroke="var(--f-dim)" stroke-width="1.6" marker-end="url(#ap)"/>')
        x += w + gap
    # gate box
    gx, gy = 24 + 4 * (w + gap), 170
    b.append(f'<rect x="{gx - 40}" y="{gy}" width="{w + 40}" height="56" rx="8" fill="var(--f-soft)" stroke="var(--f-c)" stroke-width="1.6"/>')
    b.append(text(gx - 40 + (w + 40) / 2, gy + 23, 'gate: cmp .out, .rpt', 12, anchor='middle', weight=600))
    b.append(text(gx - 40 + (w + 40) / 2, gy + 41, 'vs EPA 5.2.4, byte for byte', 11, 'var(--f-dim)', 'middle'))
    b.append(f'<path d="M{gx + w / 2} {y + 76} v{gy - y - 80}" stroke="var(--f-dim)" stroke-width="1.6" marker-end="url(#ap)"/>')
    b.append(f'<path d="M{gx - 42} {gy + 28} H{24 + 3 * (w + gap) - 10} q-8 0 -8 -8 V{y + 78}" fill="none" stroke="var(--f-d)" stroke-width="1.4" stroke-dasharray="5 4" marker-end="url(#ap)"/>')
    b.append(text(24, gy + 22, 'Any byte that differs stops the line;', 11, 'var(--f-dim)'))
    b.append(text(24, gy + 38, 'the change is fixed or thrown away.', 11, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'The build and verification pipeline')

# ----------------------------------------------------------------- F4 shape matrix
def fig_shapes(matrix_txt):
    rows = []
    for line in matrix_txt.splitlines():
        parts = line.split()
        if len(parts) == 2 and set(parts[1]) <= set('x.') and len(parts[1]) == 8:
            rows.append((parts[0], parts[1]))
    q = ['S of A', 'Y of A', 'A of Y', 'W of Y', 'R of Y', 'R of A', 'A of S', 'dS/dA']
    W = 760; cw = 52; x0 = 210; y0 = 92; rh = 17
    H = y0 + rh * len(rows) + 50
    b = [text(24, 30, 'Eight switch statements, transposed: one class per shape, one method per quantity', 14, weight=600),
         text(24, 50, 'Filled: the shape had its own case in EPA\'s switch and now overrides the method. Open: it fell to the default branch, now the base class.', 11, 'var(--f-dim)')]
    for j, name in enumerate(q):
        b.append(text(x0 + j * cw + cw / 2, y0 - 12, name, 11, 'var(--f-dim)', 'middle', 600))
    total = 0
    for i, (name, bits) in enumerate(rows):
        y = y0 + i * rh
        if i % 2 == 0:
            b.append(f'<rect x="20" y="{y - 1}" width="{W - 40}" height="{rh}" fill="var(--f-soft)"/>')
        b.append(text(24, y + 12, name, 11, family=MONO))
        for j, c in enumerate(bits):
            cx = x0 + j * cw + cw / 2
            if c == 'x':
                total += 1
                b.append(f'<circle cx="{cx}" cy="{y + 7.5}" r="5.2" fill="var(--f-a)"/>')
            else:
                b.append(f'<circle cx="{cx}" cy="{y + 7.5}" r="4.6" fill="none" stroke="var(--f-line)" stroke-width="1.4"/>')
        b.append(text(x0 + 8 * cw + 16, y + 12, str(bits.count('x')), 11, 'var(--f-dim)'))
    yb = y0 + rh * len(rows) + 22
    b.append(text(24, yb, f'{len(rows)} shapes x 8 quantities = {8 * len(rows)} cells; {total} are shape-specific code, {8 * len(rows) - total} inherit the default.', 12))
    b.append(text(24, yb + 18, 'A new shape is one new class and one table entry instead of edits to eight switches.', 11, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'Cross-section shape override matrix')

# ----------------------------------------------------------------- F5 many models in one instance
def fig_multi(runs):
    W, H = 760, 300
    b = [text(24, 30, 'Six models, one WebAssembly instance, stepped in turn', 14, weight=600),
         text(24, 50, 'Routing steps each model took (log scale). Every one of the six .out files is byte-identical to its own solo EPA run.', 11, 'var(--f-dim)')]
    x0, x1 = 230, 700; y = 74
    mx = math.log10(max(r['steps'] for r in runs))
    for t in [1, 2, 3, 4, 5, 6]:
        if t > mx + 0.2: break
        x = x0 + (x1 - x0) * t / math.ceil(mx)
        b.append(f'<line x1="{x:.1f}" y1="{y - 6}" x2="{x:.1f}" y2="{y + 6 + 30 * len(runs)}" stroke="var(--f-line)"/>')
        b.append(text(x, y + 22 + 30 * len(runs), f'{10 ** t:,}', 10, 'var(--f-dim)', 'middle'))
    for i, r in enumerate(runs):
        yy = y + i * 30
        w = (x1 - x0) * math.log10(r['steps']) / math.ceil(mx)
        b.append(text(24, yy + 15, r['name'], 12, family=MONO))
        b.append(f'<rect x="{x0}" y="{yy + 2}" width="{w:.1f}" height="18" rx="3" fill="var(--f-a)"/>')
        b.append(text(x0 + w + 8, yy + 16, f'{r["steps"]:,}', 11, 'var(--f-dim)'))
    b.append(text(24, H - 16, 'EPA SWMM 5.2.4 can hold one model per process; asked to open a second, it returns success and abandons the first.', 11, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'Round-robin run of six models in one instance')

# ----------------------------------------------------------------- F6 coupling hydrograph
def fig_coupled(c):
    W, H = 760, 340
    L, R, T, B = 70, 730, 60, 290
    n = 72  # first 6 hours of 5-minute periods
    t = [(k + 1) * 5 / 60 for k in range(n)]
    ser = [('merged network (one model)', c['merged'][:n], 'var(--f-ink)', ''),
           ('coupled: two models, one process', c['coupled'][:n], 'var(--f-a)', 'stroke-dasharray="6 4"'),
           ('upstream outfall OA (handed across)', c['upstream'][:n], 'var(--f-b)', '')]
    ymax = math.ceil(max(max(s[1]) for s in ser) / 20) * 20
    X = lambda v: L + (R - L) * v / 6.0
    Y = lambda v: B - (B - T) * v / ymax
    b = [text(24, 28, 'Two models coupled at a node reproduce the merged network', 14, weight=600),
         text(24, 46, 'Inflow to outfall OB, cfs, every 5 minutes. Measured in this build (swmm5-cplus wasm), not sketched.', 11, 'var(--f-dim)')]
    for v in range(0, ymax + 1, 20):
        b.append(f'<line x1="{L}" y1="{Y(v):.1f}" x2="{R}" y2="{Y(v):.1f}" stroke="var(--f-line)" stroke-width="0.8"/>')
        b.append(text(L - 8, Y(v) + 4, v, 10, 'var(--f-dim)', 'end'))
    for h in range(0, 7):
        b.append(text(X(h), B + 16, f'{h} h', 10, 'var(--f-dim)', 'middle'))
    b.append(text(14, (T + B) / 2, 'cfs', 11, 'var(--f-dim)', 'middle', extra=f'transform="rotate(-90 14 {(T + B) / 2})"'))
    for name, s, col, dash in ser:
        pts = ' '.join(f'{X(tt):.1f},{Y(v):.1f}' for tt, v in zip(t, s))
        b.append(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="2" {dash}/>')
    ly = 74
    for name, s, col, dash in ser:
        b.append(f'<line x1="430" y1="{ly}" x2="458" y2="{ly}" stroke="{col}" stroke-width="2" {dash}/>')
        b.append(text(466, ly + 4, name, 11))
        ly += 18
    pk = lambda a: max(a)
    b.append(text(430, ly + 10, f'peak {pk(c["coupled"]):.1f} vs {pk(c["merged"]):.1f} cfs ({100 * (pk(c["coupled"]) - pk(c["merged"])) / pk(c["merged"]):+.1f}%)', 11, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'Coupled versus merged outfall hydrograph')

# ----------------------------------------------------------------- F7 timing
def fig_timing(o2, lto):
    W, H = 760, 330
    L, R, T, B = 230, 720, 70, 270
    lo, hi = 0.90, 1.12
    X = lambda v: L + (R - L) * (v - lo) / (hi - lo)
    b = [text(24, 28, 'What the restructure costs in time: about nothing once the compiler can see across files', 14, weight=600),
         text(24, 46, 'Median of 5 interleaved runs per model, C++ time / EPA C time. Bars: the A\'/A noise floor (EPA run twice).', 11, 'var(--f-dim)')]
    for v in [0.90, 0.95, 1.00, 1.05, 1.10]:
        b.append(f'<line x1="{X(v):.1f}" y1="{T - 6}" x2="{X(v):.1f}" y2="{B + 4}" stroke="var(--f-line)" stroke-width="{1.6 if v == 1 else 0.8}"/>')
        b.append(text(X(v), B + 20, f'{v:.2f}', 10, 'var(--f-dim)', 'middle'))
    rows = o2['rows']
    dy = (B - T) / len(rows)
    for i, (a, l) in enumerate(zip(rows, lto['rows'])):
        y = T + i * dy + dy / 2
        b.append(text(24, y + 4, a['model'].replace('.inp', '')[:26], 11, family=MONO))
        f0, f1 = sorted([1.0, a['floor']])
        b.append(f'<rect x="{X(f0):.1f}" y="{y - 7}" width="{max(2, X(f1) - X(f0)):.1f}" height="14" fill="var(--f-soft)" stroke="var(--f-line)"/>')
        b.append(f'<circle cx="{X(a["ratio"]):.1f}" cy="{y}" r="5" fill="var(--f-d)"/>')
        b.append(f'<rect x="{X(l["ratio"]) - 4.5:.1f}" y="{y - 4.5}" width="9" height="9" fill="var(--f-c)"/>')
    b.append(f'<circle cx="{L}" cy="{H - 26}" r="5" fill="var(--f-d)"/>')
    b.append(text(L + 10, H - 22, f'-O2: median {o2["summary"]["median_ratio"]:.3f}', 11))
    b.append(f'<rect x="{L + 160}" y="{H - 30}" width="9" height="9" fill="var(--f-c)"/>')
    b.append(text(L + 175, H - 22, f'-O2 -flto, both engines: median {lto["summary"]["median_ratio"]:.3f}', 11))
    return svg(W, H, ''.join(b), 'Run-time ratio with noise floor')

# ----------------------------------------------------------------- F8 coupling heatmap
def fig_heat(cm):
    mods = sorted(cm['modules'], key=lambda m: -sum(cm['modules'][m].values()))[:14]
    gl = [g for g, _ in sorted(cm['totals'].items(), key=lambda x: -x[1])[:12]]
    W = 760; x0 = 120; y0 = 96; cw = 50; rh = 19
    H = y0 + rh * len(mods) + 56
    b = [text(24, 28, 'Why the next step is harder: the modules all reach into the same few arrays', 14, weight=600),
         text(24, 46, 'References from each module to each globals.h variable in EPA 5.2.4 (libclang count). Darker = more.', 11, 'var(--f-dim)')]
    mxv = max(cm['modules'][m].get(g, 0) for m in mods for g in gl)
    for j, g in enumerate(gl):
        b.append(text(x0 + j * cw + cw / 2, y0 - 10, g, 10, 'var(--f-dim)', 'middle', 600))
    for i, m in enumerate(mods):
        y = y0 + i * rh
        b.append(text(24, y + 13, m + '.c', 11, family=MONO))
        for j, g in enumerate(gl):
            v = cm['modules'][m].get(g, 0)
            if v:
                op = 0.15 + 0.85 * math.log1p(v) / math.log1p(mxv)
                b.append(f'<rect x="{x0 + j * cw + 2}" y="{y + 1}" width="{cw - 4}" height="{rh - 2}" rx="2" fill="var(--f-a)" fill-opacity="{op:.2f}"/>')
                if v >= 40:
                    b.append(text(x0 + j * cw + cw / 2, y + 13, v, 9, 'var(--f-face)' if op > 0.55 else 'var(--f-ink)', 'middle', 600))
            else:
                b.append(f'<rect x="{x0 + j * cw + 2}" y="{y + 1}" width="{cw - 4}" height="{rh - 2}" rx="2" fill="none" stroke="var(--f-line)" stroke-width="0.6"/>')
    yb = y0 + rh * len(mods) + 24
    t = cm['totals']; u = cm['users']
    b.append(text(24, yb, f'Link: {t["Link"]} references from {u["Link"]} modules.  Node: {t["Node"]} from {u["Node"]}.  Nobjects: {t["Nobjects"]} from {u["Nobjects"]}.  {len(cm["modules"])} of 54 modules touch globals.h.', 11))
    return svg(W, H, ''.join(b), 'Module to global-variable coupling heatmap')

# ----------------------------------------------------------------- F9 gate results
def fig_gate(folders, title_note):
    W = 820; rh = 16; x0 = 260; x1 = 680
    H = 80 + rh * len(folders) + 40
    b = [text(24, 28, 'The corpus gate: every model run by both engines, outputs compared byte for byte', 14, weight=600),
         text(24, 46, title_note, 11, 'var(--f-dim)')]
    mx = max(f['n'] for f in folders)
    sc = lambda v: (x1 - x0) * math.log10(1 + v) / math.log10(1 + mx)
    for i, f in enumerate(folders):
        y = 66 + i * rh
        b.append(text(24, y + 11, f['name'][:34], 10, family=MONO))
        w = sc(f['n'])
        b.append(f'<rect x="{x0}" y="{y + 2}" width="{w:.1f}" height="11" fill="var(--f-c)" rx="1.5"/>')
        if f.get('crash'):
            xa, xb = x0 + sc(f['n'] - f['inc'] - f['crash']), x0 + sc(f['n'] - f['inc'])
            b.append(f'<rect x="{xa:.1f}" y="{y + 2}" width="{max(xb - xa, 3):.1f}" height="11" fill="var(--f-e)"/>')
        if f['inc']:
            b.append(f'<rect x="{x0 + sc(f["n"] - f["inc"]):.1f}" y="{y + 2}" width="{max(w - sc(f["n"] - f["inc"]), 3):.1f}" height="11" fill="var(--f-b)" rx="1.5"/>')
        if f['diff']:
            b.append(f'<rect x="{x0}" y="{y + 2}" width="{sc(f["diff"]):.1f}" height="11" fill="var(--f-d)"/>')
        extra = ', '.join(([f'{f["crash"]} crash'] if f.get('crash') else []) + ([f'{f["inc"]} inconclusive'] if f['inc'] else []))
        b.append(text(x0 + w + 6, y + 11, f'{f["same"]}/{f["n"]}' + (f'  ({extra})' if extra else ''), 10, 'var(--f-dim)'))
    yb = 66 + rh * len(folders) + 22
    b.append(f'<rect x="24" y="{yb - 9}" width="11" height="11" fill="var(--f-c)"/>')
    b.append(text(40, yb, 'byte-identical .out and .rpt', 11))
    b.append(f'<rect x="220" y="{yb - 9}" width="11" height="11" fill="var(--f-e)"/>')
    b.append(text(236, yb, 'both crash, same signal', 11))
    b.append(f'<rect x="390" y="{yb - 9}" width="11" height="11" fill="var(--f-b)"/>')
    b.append(text(406, yb, 'inconclusive: both hit the 300 s or 2 GiB cap', 11))
    b.append(f'<rect x="660" y="{yb - 9}" width="11" height="11" fill="var(--f-d)"/>')
    b.append(text(676, yb, 'different', 11))
    return svg(W, H, ''.join(b), 'Corpus gate results by folder')
