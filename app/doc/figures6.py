#!/usr/bin/env python3
"""Figures for the SWMM6 comparison section, drawn from results/swmm6/*.json."""
import math
from figures import svg, text, esc

SHORT6 = {'Semi_Real_Models/375_H&H_Elements (1).inp': '375-element network', 'XPSWMM/MAYBR.inp': 'XP network (MAYBR)',
          'Hydrology/46_Subs.inp': '46 subcatchments', 'Hydrology/10_Subs_Continuous.inp': 'continuous, 10 subs',
          'SWMM5_NCIMM/lid1d.inp': 'LID network', 'Training/Exercise5_transect.inp': 'natural channels',
          'WQ/Peter Klaver_WQ_Tracer.inp': 'water-quality tracer', 'Hydraulics/short_trap_SW5.inp': 'short trapezoid pipe'}

def kl(n): return f'{n / 1000:,.0f}k'

# ------------------------------------------------------------ size of the three code bases
def fig_s6_size(st):
    L = st['lines']
    rows = [('EPA SWMM 5.2.4 solver (C)', [(L['epa_solver'], 'var(--f-dim)', 'engine')]),
            ('swmm5-cplus (C++17)', [(L['cplus_engine'] + L['cplus_api'], 'var(--f-c)', 'engine')]),
            ('SWMM6 legacy engine (C, 5.3.0-beta.3)', [(L['swmm6_legacy'], 'var(--f-b)', 'engine')]),
            ('SWMM6 new engine (C++20)', [(L['swmm6_engine'], 'var(--f-a)', 'engine'), (L['swmm6_2d'], 'var(--f-e)', '2D'),
                                          (L['swmm6_headers'], 'var(--f-line)', 'public headers')]),
            ('SWMM6 tests', [(L['swmm6_tests'], 'var(--f-soft)', 'tests')])]
    W, LX, RX = 760, 270, 640
    top = max(sum(n for n, _, _ in p) for _, p in rows)
    sc = (RX - LX) / top
    rh, T = 34, 66
    H = T + rh * len(rows) + 50
    b = [text(24, 28, 'How much code each engine is', 14, weight=600),
         text(24, 46, 'Lines in .c/.cpp/.h/.hpp files, counted from the source trees (comments and blank lines included).', 11, 'var(--f-dim)')]
    for i, (lab, parts) in enumerate(rows):
        y = T + i * rh
        b.append(text(24, y + 16, lab, 11, weight=600 if 'new' in lab else 400))
        x = LX
        for n, col, name in parts:
            w = n * sc
            b.append(f'<rect x="{x:.1f}" y="{y + 4}" width="{w:.1f}" height="18" fill="{col}" stroke="var(--f-line)" stroke-width="0.6"/>')
            if w > 46:
                b.append(text(x + w / 2, y + 17, kl(n), 10, 'var(--f-face)' if col not in ('var(--f-line)', 'var(--f-soft)') else 'var(--f-ink)', 'middle', 600))
            x += w
        tot = sum(n for n, _, _ in parts)
        if len(parts) > 1 or tot * sc <= 46:
            b.append(text(x + 6, y + 17, f'{tot:,}' if len(parts) == 1 else f'{tot:,} in all', 10, 'var(--f-dim)'))
    lx, ly = 24, H - 18
    for col, name in [('var(--f-a)', 'engine'), ('var(--f-e)', '2D surface module'), ('var(--f-line)', 'public C headers')]:
        b.append(f'<rect x="{lx}" y="{ly - 10}" width="12" height="12" fill="{col}" stroke="var(--f-line)" stroke-width="0.6"/>')
        b.append(text(lx + 18, ly, name, 11, 'var(--f-dim)'))
        lx += 34 + len(name) * 6.2
    return svg(W, H, ''.join(b), 'Code size of the three engines')

# ------------------------------------------------------------ array of structs vs struct of arrays
def fig_s6_layout():
    W, H = 760, 330
    b = [text(24, 28, 'Same numbers, laid out two ways in memory', 14, weight=600),
         text(24, 46, 'A diagram, not a measurement. Each cell is one 8-byte double; the highlighted cells are what a loop over node depths reads.', 11, 'var(--f-dim)')]
    fields = ['depth', 'head', 'volume', 'inflow', 'outflow', 'crown', '...']
    cw, ch = 28, 26
    # array of structs
    y = 92
    b.append(text(24, y - 14, 'EPA 5.2.4 and swmm5-cplus: one record per node', 12, weight=600))
    b.append(text(24, y + 36 + 14, 'Node[j].newDepth   ->  read one double, skip the rest of the record', 11, 'var(--f-dim)', family='IBM Plex Mono, Menlo, monospace'))
    x = 24
    for j in range(3):
        b.append(text(x + len(fields) * cw / 2, y + 2, f'Node[{j}]', 10, 'var(--f-dim)', 'middle'))
        for k, f in enumerate(fields):
            hi = k == 0
            b.append(f'<rect x="{x + k * cw}" y="{y + 6}" width="{cw}" height="{ch}" fill="{"var(--f-a)" if hi else "var(--f-face)"}" stroke="var(--f-line)"/>')
            b.append(text(x + k * cw + cw / 2, y + 23, f[0] if f != '...' else '..', 9, 'var(--f-face)' if hi else 'var(--f-dim)', 'middle'))
        x += len(fields) * cw + 12
    b.append(text(x + 2, y + 23, '...', 10, 'var(--f-dim)'))
    # struct of arrays
    y = 196
    b.append(text(24, y - 14, 'SWMM6: one array per quantity', 12, weight=600))
    names = ['depth', 'head', 'volume']
    for r, nm in enumerate(names):
        yy = y + 6 + r * (ch + 6)
        b.append(text(24, yy + 17, f'ctx.nodes.{nm}', 10, 'var(--f-dim)', family='IBM Plex Mono, Menlo, monospace'))
        for j in range(14):
            hi = r == 0
            b.append(f'<rect x="{150 + j * cw}" y="{yy}" width="{cw}" height="{ch}" fill="{"var(--f-a)" if hi else "var(--f-face)"}" stroke="var(--f-line)"/>')
            b.append(text(150 + j * cw + cw / 2, yy + 17, str(j), 9, 'var(--f-face)' if hi else 'var(--f-dim)', 'middle'))
        b.append(text(150 + 14 * cw + 8, yy + 17, '...', 10, 'var(--f-dim)'))
    b.append(text(24, H - 12, 'ctx.nodes.depth[j]  ->  consecutive depths share cache lines, and the compiler can vectorize the loop', 11, 'var(--f-dim)', family='IBM Plex Mono, Menlo, monospace'))
    return svg(W, H, ''.join(b), 'Array of structures against structure of arrays')

# ------------------------------------------------------------ who is checked against whom
def fig_s6_parity(st, sm):
    W, H = 760, 356
    S = sm['status']
    def frac(p):
        s = S[p]; n = s.get('identical values', 0) + s.get('differs', 0)
        return s.get('identical values', 0), n
    b = ['<defs><marker id="ah6" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
         '<path d="M0,0 L10,5 L0,10 z" fill="var(--f-dim)"/></marker></defs>',
         text(24, 28, 'What each engine is checked against', 14, weight=600),
         text(24, 46, f'Solid: this gate, {sm["models"]:,} corpus models, counted where both engines wrote results. Dashed: checks in the SWMM6 repository.', 11, 'var(--f-dim)')]
    def box(x, y, w, l1, l2, col):
        b.append(f'<rect x="{x}" y="{y}" width="{w}" height="50" rx="6" fill="var(--f-face)" stroke="{col}" stroke-width="2"/>')
        b.append(text(x + w / 2, y + 21, l1, 12, 'var(--f-ink)', 'middle', 600))
        b.append(text(x + w / 2, y + 38, l2, 10, 'var(--f-dim)', 'middle'))
    def line(x1, y1, x2, y2, col, dash=''):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        b.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" stroke-width="2"{d} marker-end="url(#ah6)"/>')
    box(24, 150, 160, 'EPA SWMM 5.2.4', 'the reference, C', 'var(--f-dim)')
    box(300, 80, 180, 'swmm5-cplus', 'restructured 5.2.4, C++17', 'var(--f-c)')
    box(300, 220, 180, 'SWMM6 legacy engine', '5.3.0-beta.3, C', 'var(--f-b)')
    box(566, 220, 170, 'SWMM6 new engine', '6.0.0-alpha.4, C++20', 'var(--f-a)')
    i, n = frac('E|C'); line(184, 166, 298, 112, 'var(--f-c)')
    b.append(text(24, 128, f'to swmm5-cplus: identical on {i:,} of {n:,}', 10, 'var(--f-ink)', 'start', 600))
    i, n = frac('E|L'); line(184, 186, 298, 238, 'var(--f-b)')
    b.append(text(24, 222, f'to SWMM6 legacy: identical', 10, 'var(--f-ink)', 'start', 600))
    b.append(text(24, 235, f'on {i:,} of {n:,}', 10, 'var(--f-ink)', 'start', 600))
    i, n = frac('L|N'); line(480, 245, 564, 245, 'var(--f-a)')
    b.append(text(522, 236, f'{i:,} of {n:,}', 10, 'var(--f-ink)', 'middle', 600))
    i, n = frac('E|N')
    b.append(f'<path d="M 104 200 C 104 330, 651 330, 651 274" fill="none" stroke="var(--f-a)" stroke-width="1.4" stroke-dasharray="2 3" marker-end="url(#ah6)"/>')
    b.append(text(380, 342, f'EPA 5.2.4 against SWMM6 new: identical on {i:,} of {n:,}', 10, 'var(--f-dim)', 'middle'))
    rc = st['regression_cases']; pc = st['parity_corpus']
    b.append(f'<path d="M 600 218 C 590 180, 420 180, 410 218" fill="none" stroke="var(--f-ink)" stroke-width="1.4" stroke-dasharray="6 4" marker-end="url(#ah6)"/>')
    b.append(text(470, 176, f'regression test: {len(rc)} decks, within {rc[0][2]} or {float(rc[0][3]) * 100:g} %', 10, 'var(--f-ink)', 'middle'))
    b.append(f'<path d="M 690 218 C 690 170, 730 170, 726 218" fill="none" stroke="var(--f-ink)" stroke-width="1.4" stroke-dasharray="6 4" marker-end="url(#ah6)"/>')
    b.append(text(640, 138, f'bit-identity corpus: {pc["decks"]} decks,', 10, 'var(--f-ink)', 'middle'))
    b.append(text(640, 151, 'each build against the build before it', 10, 'var(--f-ink)', 'middle'))
    return svg(W, H, ''.join(b), 'Parity relationships between the engines')

# ------------------------------------------------------------ how big the differences are
CLS = [('identical', 'var(--f-c)'), ('last digits', '#7fb98f'), ('under 1 %', 'var(--f-b)'), ('1 to 10 %', '#e08a3c'),
       ('over 10 %', 'var(--f-d)'), ('off-network', 'var(--f-e)')]
def fig_s6_classes(sm):
    rows = [('swmm5-cplus vs EPA 5.2.4', 'E|C'), ('SWMM6 new vs itself, rerun', 'N|N2'), ('SWMM6 legacy vs itself', 'L|L2'),
            ('SWMM6 legacy vs EPA 5.2.4', 'E|L'), ('SWMM6 new vs SWMM6 legacy', 'L|N'), ('SWMM6 new vs EPA 5.2.4', 'E|N')]
    W, LX, RX = 760, 230, 730
    rh, T = 30, 74
    H = T + rh * len(rows) + 64
    b = [text(24, 28, 'How far apart the engines land, model by model', 14, weight=600),
         text(24, 46, 'Each model goes in one band by the largest change in any link flow or node depth, as a share of that element\'s peak.', 11, 'var(--f-dim)'),
         text(24, 60, 'Only models both engines ran to the same element set are counted.', 11, 'var(--f-dim)')]
    for i, (lab, p) in enumerate(rows):
        y = T + i * rh
        if p in sm['classes']:
            c = sm['classes'][p]
        else:
            s = sm['status'][p]; c = {'identical': s.get('identical values', 0), 'over 10 %': s.get('differs', 0)}
        n = sum(c.get(k, 0) for k, _ in CLS)
        b.append(text(24, y + 15, lab, 11, weight=600 if p in ('E|C', 'L|N') else 400))
        x = LX
        for k, col in CLS:
            v = c.get(k, 0)
            if not v: continue
            w = (RX - LX) * v / n
            b.append(f'<rect x="{x:.1f}" y="{y + 3}" width="{w:.1f}" height="17" fill="{col}"/>')
            if w > 28 and not (p == 'E|C' or p.startswith(('N|', 'L|L'))): b.append(text(x + w / 2, y + 15, f'{v:,}', 9, 'var(--f-face)', 'middle', 600))
            x += w
        if p == 'E|C' or p.startswith(('N|', 'L|L')):
            b.append(text(RX - 4, y + 15, f'{c.get("identical", 0):,} of {n:,}', 9, 'var(--f-face)', 'end', 600))
    lx, ly = 24, H - 30
    for k, col in CLS:
        lab = {'off-network': 'only totals or tiny elements differ'}.get(k, k)
        b.append(f'<rect x="{lx}" y="{ly - 10}" width="12" height="12" fill="{col}"/>')
        b.append(text(lx + 16, ly, lab, 10, 'var(--f-dim)'))
        lx += 28 + len(lab) * 5.6
        if lx > 640: lx, ly = 24, ly + 18
    return svg(W, H, ''.join(b), 'Difference classes by engine pair')

# ------------------------------------------------------------ last bit to visible: growth over a run
def fig_s6_growth(g, title_model):
    rows = g['rows']
    W, H = 760, 340
    L, R, T, B = 80, 700, 70, 272
    tmax = rows[-1][0]
    X = lambda t: L + (R - L) * t / tmax
    lo, hi = 1e-6, 1e3
    Y = lambda v: B - (B - T) * (math.log10(max(v, lo)) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    qmax = max(r[4] for r in rows)
    YQ = lambda v: B - (B - T) * v / qmax
    b = [text(24, 28, f'From the last bit to a visible difference, and back: {title_model}', 14, weight=600),
         text(24, 46, 'SWMM6 new engine against its legacy engine. Shaded: largest link flow (right scale). Line: largest flow difference.', 11, 'var(--f-dim)')]
    area = ' '.join(f'{X(r[0]):.1f},{YQ(r[4]):.1f}' for r in rows)
    b.append(f'<polygon points="{X(rows[0][0]):.1f},{B} {area} {X(rows[-1][0]):.1f},{B}" fill="var(--f-soft)"/>')
    for v in [1e-6, 1e-4, 1e-2, 1, 100]:
        b.append(f'<line x1="{L}" y1="{Y(v):.1f}" x2="{R}" y2="{Y(v):.1f}" stroke="var(--f-line)" stroke-width="0.8"/>')
        b.append(text(L - 6, Y(v) + 4, ('%g' % v), 10, 'var(--f-dim)', 'end'))
    b.append(text(L - 6, T - 8, 'cfs', 10, 'var(--f-dim)', 'end'))
    for h in range(0, int(tmax / 60) + 1, 3):
        b.append(text(X(h * 60), B + 16, f'{h} h', 10, 'var(--f-dim)', 'middle'))
    b.append(text(R + 6, T + 4, f'{qmax:,.0f}', 10, 'var(--f-dim)'))
    b.append(text(R + 6, B, '0', 10, 'var(--f-dim)'))
    pts = []
    for r in rows:
        pts.append(f'{X(r[0]):.1f},{Y(r[2] if r[2] > 0 else lo):.1f}')
    b.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="var(--f-d)" stroke-width="1.6"/>')
    b.append(text(24, H - 24, 'Differences of 1e-6 cfs or less are drawn on the bottom line. The .out file stores 32-bit numbers, so', 10, 'var(--f-dim)'))
    b.append(text(24, H - 10, 'differences in the last bits of the 64-bit solution do not show until they grow.', 10, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'Growth of the difference between SWMM6 legacy and new')

# ------------------------------------------------------------ speed of four engines
def fig_s6_speed(rows, noise):
    """rows: [(label, geo, [per-model ratios], colour)]"""
    W, L, R = 760, 290, 730
    lo, hi = 0.25, 8.0
    X = lambda v: L + (R - L) * (math.log2(v) - math.log2(lo)) / (math.log2(hi) - math.log2(lo))
    rh, T = 32, 72
    H = T + rh * len(rows) + 56
    B = T + rh * len(rows)
    b = [text(24, 28, 'CPU time against EPA 5.2.4, eight models', 14, weight=600),
         text(24, 46, "Each engine as its own project builds it. EPA: Ubuntu's default gcc -O2. Bar: geometric mean; circles: models; log scale.", 11, 'var(--f-dim)')]
    b.append(f'<rect x="{X(noise[0]):.1f}" y="{T - 6}" width="{X(noise[1]) - X(noise[0]):.1f}" height="{B - T + 6}" fill="var(--f-soft)"/>')
    for v in [0.25, 0.5, 1, 2, 4, 8]:
        b.append(f'<line x1="{X(v):.1f}" y1="{T - 6}" x2="{X(v):.1f}" y2="{B + 4}" stroke="var(--f-line)" stroke-width="{1.6 if v == 1 else 0.8}"/>')
        b.append(text(X(v), B + 18, f'{v:g}x', 10, 'var(--f-dim)', 'middle'))
    for i, (label, geo, per, col) in enumerate(rows):
        y = T + i * rh
        b.append(text(24, y + 15, label, 11))
        x0, x1 = sorted([X(1.0), X(geo)])
        b.append(f'<rect x="{x0:.1f}" y="{y + 4}" width="{max(2, x1 - x0):.1f}" height="14" fill="{col}" opacity="0.85"/>')
        b.append(text(L - 8, y + 15, f'{geo:.2f}', 11, 'var(--f-ink)', 'end', 700))
        for p in per:
            b.append(f'<circle cx="{X(min(max(p, lo), hi)):.1f}" cy="{y + 11}" r="3" fill="none" stroke="var(--f-ink)" stroke-width="1.2"/>')
    b.append(f'<rect x="24" y="{H - 24}" width="12" height="12" fill="var(--f-soft)" stroke="var(--f-line)"/>')
    b.append(text(42, H - 14, f'noise: EPA timed against itself, {noise[0]:.2f}-{noise[1]:.2f} per model. Left of 1 is faster than EPA.', 11, 'var(--f-dim)'))
    return svg(W, H, ''.join(b), 'Engine speed against EPA 5.2.4')
