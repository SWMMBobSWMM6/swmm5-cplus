#!/usr/bin/env python3
"""
transpose_xsect.py -- R3: cross-section geometry as shape objects.

EPA SWMM5 computes cross-section geometry with eight functions
(xsect_getAofY, getYofA, getWofY, getRofY, getRofA, getSofA, getAofS, getdSdA),
each a switch over 26 shape types. Adding a shape means editing all eight switches
(plus xsect_setParams, enums.h, keywords). This tool transposes the 8 x 26 matrix:
one class per shape, one virtual method per quantity, the base class holding each
switch's default branch.

Case bodies are copied verbatim, with the function's prologue (its local declarations
and early-return guards) repeated at the top of each method, so every shape computes
exactly what the switch computed, operation for operation.

The only edits inside case bodies: IRREGULAR, CUSTOM and STREET used to reach into
the global Transect / Shape+Curve / Street arrays on every call. Their tables are
now bound into the TXsect when the link is validated (xsect->aTbl, rTbl, wTbl,
nTbl), which makes all geometry a pure function of the TXsect -- no Project needed.

  transpose_xsect.py src/engine
"""
import re, sys, os, collections

ENG = sys.argv[1]
src = open(os.path.join(ENG, 'xsect.cpp'), encoding='latin-1').read()

TYPES = ['DUMMY', 'CIRCULAR', 'FILLED_CIRCULAR', 'RECT_CLOSED', 'RECT_OPEN', 'TRAPEZOIDAL',
         'TRIANGULAR', 'PARABOLIC', 'POWERFUNC', 'RECT_TRIANG', 'RECT_ROUND', 'MOD_BASKET',
         'HORIZ_ELLIPSE', 'VERT_ELLIPSE', 'ARCH', 'EGGSHAPED', 'HORSESHOE', 'GOTHIC', 'CATENARY',
         'SEMIELLIPTICAL', 'BASKETHANDLE', 'SEMICIRCULAR', 'IRREGULAR', 'CUSTOM', 'FORCE_MAIN',
         'STREET_XSECT']
DISPATCH = ['xsect_getSofA', 'xsect_getYofA', 'xsect_getAofY', 'xsect_getWofY',
            'xsect_getRofY', 'xsect_getRofA', 'xsect_getAofS', 'xsect_getdSdA']
STAY = {'xsect_isOpen', 'xsect_setParams', 'xsect_setIrregXsectParams',
        'xsect_setStreetXsectParams', 'xsect_setCustomXsectParams'}

# ------------------------------------------------------------ split into functions
def match_brace(s, i):
    assert s[i] == '{'
    depth = 0
    j = i
    in_str = None
    while j < len(s):
        c = s[j]
        if in_str:
            if c == '\\': j += 2; continue
            if c == in_str: in_str = None
        elif s.startswith('//', j):
            j = s.index('\n', j); continue
        elif s.startswith('/*', j):
            j = s.index('*/', j) + 2; continue
        elif c in '"\'':
            in_str = c
        elif c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return j
        j += 1
    raise RuntimeError('unbalanced')

funcs = collections.OrderedDict()
for m in re.finditer(r'^([A-Za-z][\w \*]*?)\bProject::(\w+)\s*\(', src, re.M):
    name = m.group(2)
    start = m.start()
    brace = src.index('{', src.index(')', m.end()))
    # the EPA comment block sits between ')' and '{'
    end = match_brace(src, brace)
    funcs[name] = dict(start=start, brace=brace, end=end + 1,
                       head=src[start:brace], body=src[brace:end + 1],
                       ret=m.group(1).strip())
header = src[:min(f['start'] for f in funcs.values())]

# ------------------------------------------------------------ user-table rewrites
TBL = [
    (r'Transect\[xsect->transect\]\.(areaTbl|hradTbl|widthTbl),\s*N_TRANSECT_TBL', None),
    (r'Shape\[Curve\[xsect->transect\]\.refersTo\]\.(areaTbl|hradTbl|widthTbl),\s*N_SHAPE_TBL', None),
    (r'Street\[xsect->transect\]\.transect\.(areaTbl|hradTbl|widthTbl),\s*'
     r'Street\[xsect->transect\]\.transect\.nTbl', None),
]
FIELD = {'areaTbl': 'aTbl', 'hradTbl': 'rTbl', 'widthTbl': 'wTbl'}
def rebind_tables(code):
    n = 0
    for pat, _ in TBL:
        code, k = re.subn(pat, lambda m: f'xsect->{FIELD[m.group(1)]}, xsect->nTbl', code)
        n += k
    assert not re.search(r'\b(Transect|Shape|Curve|Street)\[', code), code
    return code, n

# ------------------------------------------------------------ parse the 8 switches
def split_switch(body):
    """returns (prologue, {label: code}, default_code) for a body with one
    'switch ( xsect->type )' at its top level"""
    sw = re.search(r'switch\s*\(\s*xsect->type\s*\)\s*\{', body)
    assert sw, 'no switch'
    prologue = body[1:sw.start()]
    sb = sw.end() - 1
    se = match_brace(body, sb)
    epilogue = body[se + 1:body.rindex('}')]
    assert epilogue.strip() == '', ('code after switch', epilogue)
    inner = body[sb + 1:se]
    # labels at statement starts
    lab = list(re.finditer(r'(?m)^\s*(case\s+(\w+)\s*:|default\s*:)', inner))
    cases = {}
    default = None
    pending = []
    for i, m in enumerate(lab):
        code = inner[m.end(): lab[i + 1].start() if i + 1 < len(lab) else len(inner)]
        label = m.group(2) if m.group(2) else 'default'
        pending.append(label)
        if code.strip() == '':
            continue            # fall through to the next label's code
        for l in pending:
            if l == 'default': default = code
            else: cases[l] = code
        pending = []
    assert not pending, pending
    assert default is not None
    return prologue, cases, default

def constify(params):
    # table arguments are read-only; const lets bound user tables pass through
    return re.sub(r'\bdouble\s*\*\s*(table)\b', r'const double *\1', params)

def sig_of(name):
    f = funcs[name]
    head = re.sub(r'//[^\n]*', '', f['head'])
    head = ' '.join(head.split())
    params = head[head.index('(') + 1: head.rindex(')')]
    return f['ret'], params

matrix = {}
for d in DISPATCH:
    pro, cases, dflt = split_switch(funcs[d]['body'])
    unknown = set(cases) - set(TYPES)
    assert not unknown, (d, unknown)
    matrix[d] = (pro, cases, dflt)

# ------------------------------------------------------------ emit geometry header
H = []
H.append('''//-----------------------------------------------------------------------------
//   xsect_geometry.hpp  --  swmm5-cplus
//
//   Cross-section geometry as shape objects (generated by tools/transpose_xsect.py
//   from EPA SWMM 5.2.4's xsect.c, then compiled and gated for byte-identical output).
//
//   class XsectShape: one virtual method per geometric quantity; the base class
//   holds what EPA's switch statements did in their default branch. One final
//   subclass per shape type overrides exactly the quantities EPA's switches gave
//   that shape its own case for. A new shape is one new class and one table entry.
//
//   Everything here is a pure function of the TXsect: no Project, no globals.
//-----------------------------------------------------------------------------
#ifndef SWMM5CPLUS_XSECT_GEOMETRY_HPP
#define SWMM5CPLUS_XSECT_GEOMETRY_HPP

#include "consts.h"
#include "macros.h"
#include "enums.h"
#include "objects.h"

extern double Amax[];          // max. area / full area, by shape type (EPA table)

// xsect.dat tables used by xsect_setParams (defined in xsect_geometry.cpp)
extern int    N_R_Circ;
extern double R_Circ[51];
extern int    NumCodesEllipse;
extern double MinorAxis_Ellipse[23], MajorAxis_Ellipse[23], Afull_Ellipse[23], Rfull_Ellipse[23];
extern int    NumCodesArch;
extern double Yfull_Arch[102], Wmax_Arch[102], Afull_Arch[102], Rfull_Arch[102];

namespace swmm { namespace xs {

class XsectShape
{
public:
    virtual ~XsectShape() = default;
    virtual const char* name() const { return "DEFAULT"; }
''')
for d in DISPATCH:
    ret, params = sig_of(d)
    H.append(f'    virtual {ret} {d[6:]}({params}) const;')
H.append('''};

// the shape object for a TXsect type code (unknown codes get the base class,
// as EPA's switch statements fell to their default branch)
const XsectShape& shapeOf(int type);
int shapeOverrides(int type);     // bit i set: overrides quantity i (for documentation)

// public geometry: what EPA's xsect_get... functions were, now pure
''')
pure_public = ['xsect_getAmax', 'xsect_getYcrit'] + DISPATCH
for d in pure_public:
    ret, params = sig_of(d)
    H.append(f'{ret} {d}({params});')
H.append('')
H.append('// helpers (formerly file-static in xsect.c); xsect_setParams uses some of them')
for n in [n for n in funcs if n not in STAY and n not in DISPATCH and n not in pure_public]:
    ret, params = sig_of(n)
    H.append(f'{ret} {n}({constify(params)});')
H.append('''
}} // namespace swmm::xs
#endif
''')
open(os.path.join(ENG, 'xsect_geometry.hpp'), 'w', encoding='latin-1').write('\n'.join(H))

# ------------------------------------------------------------ emit geometry source
G = []
hdr_body = header
hdr_body = hdr_body.replace('#include "headers.h"', '#include "xsect_geometry.hpp"')
hdr_body = hdr_body.replace('double  Amax[] =', 'double  Amax[] =', 1)
G.append(hdr_body)
G.append('namespace swmm { namespace xs {\n')
# forward declarations of every moved helper
moved = [n for n in funcs if n not in STAY and n not in DISPATCH]
G.append('')

# shape classes
overrides = {}
G.append('// ---- the base class: EPA\'s default branches')
for d in DISPATCH:
    pro, cases, dflt = matrix[d]
    ret, params = sig_of(d)
    body, _ = rebind_tables('{' + pro + dflt + '}')
    G.append(f'{ret} XsectShape::{d[6:]}({params}) const\n{body}\n')
for ti, t in enumerate(TYPES):
    mine = [d for d in DISPATCH if t in matrix[d][1]]
    overrides[t] = sum(1 << i for i, d in enumerate(DISPATCH) if d in mine)
    G.append(f'// ---- {t}')
    G.append(f'class Shape_{t} final : public XsectShape\n{{\npublic:')
    G.append(f'    const char* name() const override {{ return "{t}"; }}')
    for d in mine:
        pro, cases, dflt = matrix[d]
        ret, params = sig_of(d)
        body, _ = rebind_tables('{' + pro + cases[t] + '}')
        body = '\n'.join('    ' + l if l.strip() else l for l in body.split('\n'))
        G.append(f'    {ret} {d[6:]}({params}) const override\n    {body.strip()}')
    G.append('};\n')
# one immutable, stateless instance per shape, shared by every Project
G.append('// the shape objects: immutable and stateless, so one set serves every Project')
G.append('static const XsectShape s_base;')
for t in TYPES:
    G.append(f'static const Shape_{t} s_{t};')
G.append('static const XsectShape* const ShapeTable[] = {')
G.append('    ' + ',\n    '.join(f'&s_{t}' for t in TYPES))
G.append('};\n')
G.append('const XsectShape& shapeOf(int type)\n{')
G.append(f'    if ( type < 0 || type >= {len(TYPES)} ) return s_base;')
G.append('    return *ShapeTable[type];\n}\n')
G.append('int shapeOverrides(int type)\n{')
G.append('    static const int bits[] = {' + ', '.join(str(overrides[t]) for t in TYPES) + '};')
G.append(f'    return ( type < 0 || type >= {len(TYPES)} ) ? 0 : bits[type];\n}}\n')
# dispatchers
G.append('// ---- public entry points: one virtual call replaces each switch')
for d in DISPATCH:
    ret, params = sig_of(d)
    names = [p.split()[-1].lstrip('*') for p in params.split(',')]
    G.append(f'{ret} {d}({params})\n{{\n    return shapeOf(xsect->type).{d[6:]}({", ".join(names)});\n}}\n')
# moved functions, verbatim
G.append('// ---- remaining geometry, verbatim from xsect.c')
for n in moved:
    f = funcs[n]
    text = src[f['start']:f['end']]
    text = text.replace('Project::', '', 1)
    hd = text[:text.index(')') + 1]
    text = constify(hd) + text[len(hd):]
    text, _ = rebind_tables(text)
    G.append(text + '\n')
G.append('}} // namespace swmm::xs\n')
open(os.path.join(ENG, 'xsect_geometry.cpp'), 'w', encoding='latin-1').write('\n'.join(G))

# ------------------------------------------------------------ new xsect.cpp
X = []
xh = header
amax_m = re.search(r'double\s+Amax\[\]\s*=\s*\{.*?\};[^\n]*\n', xh, re.S)
xh = xh[:amax_m.start()] + '// Amax[] now lives in xsect_geometry.cpp (declared in xsect_geometry.hpp)\n' + xh[amax_m.end():]
ts = re.search(r'typedef struct\s*\{[^}]*\}\s*TXsectStar;\s*\n', xh, re.S)
if ts:
    xh = xh[:ts.start()] + xh[ts.end():]
xh = xh.replace('#include "findroot.hpp"', '#include "xsect_geometry.hpp"\nusing namespace swmm::xs;')
# the geometry tables are defined once, in xsect_geometry.cpp (xsect.dat gives them
# external linkage, so two including files would define them twice)
xh = re.sub(r'#include "xsect.dat"[^\n]*\n', '// xsect.dat tables: defined in xsect_geometry.cpp; '
            'the few that xsect_setParams needs are declared in xsect_geometry.hpp\n', xh)
X.append(xh)
BIND = {
 'xsect_setIrregXsectParams': '''
    // swmm5-cplus: bind the transect's geometry tables to the cross section,
    // so geometry never has to look up the global Transect array again
    xsect->aTbl = Transect[index].areaTbl;
    xsect->rTbl = Transect[index].hradTbl;
    xsect->wTbl = Transect[index].widthTbl;
    xsect->nTbl = N_TRANSECT_TBL;
''',
 'xsect_setStreetXsectParams': '''
    // swmm5-cplus: bind the street transect's geometry tables
    xsect->aTbl = Street[index].transect.areaTbl;
    xsect->rTbl = Street[index].transect.hradTbl;
    xsect->wTbl = Street[index].transect.widthTbl;
    xsect->nTbl = Street[index].transect.nTbl;
''',
 'xsect_setCustomXsectParams': '''
    // swmm5-cplus: bind the custom shape's geometry tables
    xsect->aTbl = Shape[index].areaTbl;
    xsect->rTbl = Shape[index].hradTbl;
    xsect->wTbl = Shape[index].widthTbl;
    xsect->nTbl = N_SHAPE_TBL;
''',
}
for n in funcs:
    f = funcs[n]
    if n in STAY:
        text = src[f['start']:f['end']]
        if n in BIND:
            text = text[:text.rindex('}')] + BIND[n] + '}'
        X.append(text + '\n')
for d in pure_public:
    ret, params = sig_of(d)
    names = [p.split()[-1].lstrip('*') for p in params.split(',')]
    X.append(f'{ret} Project::{d}({params})\n//  forwards to the pure geometry in xsect_geometry.cpp\n{{\n'
             f'    return swmm::xs::{d}({", ".join(names)});\n}}\n')
open(os.path.join(ENG, 'xsect.cpp'), 'w', encoding='latin-1').write('\n'.join(X))

# ------------------------------------------------------------ project.hpp: drop moved helpers
P = open(os.path.join(ENG, 'project.hpp'), encoding='latin-1').read()
i = P.index('    // ---- xsect.c\n')
j = P.find('    // ----', i + 10)
j = j if j > 0 else P.index('};', i)
keep = ['    // ---- xsect.c  (geometry itself lives in xsect_geometry.cpp, pure)']
for line in P[i:j].split('\n')[1:]:
    m = re.search(r'\b(\w+)\s*\(', line)
    if m and (m.group(1) in STAY or m.group(1) in pure_public):
        keep.append(line)
P = P[:i] + '\n'.join(keep) + '\n' + P[j:]
open(os.path.join(ENG, 'project.hpp'), 'w', encoding='latin-1').write(P)

print('shapes:', len(TYPES), 'quantities:', len(DISPATCH))
for t in TYPES:
    print(f'  {t:16s}', ''.join('x' if overrides[t] >> i & 1 else '.' for i in range(len(DISPATCH))))
print('moved helpers:', len(moved))
