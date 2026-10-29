#!/usr/bin/env python3
"""Static facts about an OpenSWMM (SWMM6) checkout, EPA 5.2.4 and swmm5-cplus, for the comparison.

  census6.py SWMM6_ROOT EPA_SOLVER_DIR CPLUS_ROOT OUT.json

Everything here is counted from the source trees, not estimated: lines per subsystem, the number
of `PARITY` notes in the new engine, environment variables it reads, `throw` statements, OpenMP
pragmas, public C API functions, and the legacy engine's version and token-level distance from
EPA 5.2.4 for the routing files.
"""
import os, re, sys, json, glob, subprocess

S6, EPA, CP, OUT = sys.argv[1:5]
SRC = ('.c', '.cpp', '.h', '.hpp')

def files(root, exclude=()):
    for d, dirs, fs in os.walk(root):
        dirs[:] = [x for x in dirs if os.path.join(d, x) not in exclude]
        for f in fs:
            if f.endswith(SRC): yield os.path.join(d, f)

def lines(paths): return sum(sum(1 for _ in open(p, encoding='latin-1')) for p in paths)
def grep(paths, pat): rx = re.compile(pat); return sum(len(rx.findall(open(p, encoding='latin-1').read())) for p in paths)

eng = os.path.join(S6, 'src/engine'); twod = os.path.join(eng, '2d')
new = list(files(eng, exclude={twod}))
res = {'swmm6_commit': subprocess.run(['git', '-C', S6, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip(),
       'swmm6_date': subprocess.run(['git', '-C', S6, 'log', '-1', '--format=%ad', '--date=short'], capture_output=True, text=True).stdout.strip(),
       'cplus_commit': subprocess.run(['git', '-C', CP, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()}
cm = open(os.path.join(S6, 'CMakeLists.txt')).read()
res['versions'] = re.search(r'Version:\s*(.*)', cm).group(1).strip()
res['lines'] = {
    'epa_solver': lines(files(EPA)),
    'cplus_engine': lines(files(os.path.join(CP, 'src/engine'))),
    'cplus_api': lines(files(os.path.join(CP, 'src/api'))),
    'swmm6_engine': lines(new),
    'swmm6_2d': lines(files(twod)),
    'swmm6_headers': lines(files(os.path.join(S6, 'include'))),
    'swmm6_legacy': lines(files(os.path.join(S6, 'src/legacy/engine'))),
    'swmm6_tests': lines(files(os.path.join(S6, 'tests'))),
}
H = os.path.join(eng, 'hydraulics'); D = os.path.join(eng, 'data'); P = os.path.join(CP, 'src/engine')
def L(*globs): return lines([p for g in globs for p in glob.glob(g) if os.path.isfile(p)])
res['subsystems'] = {
    'dynamic wave': [L(f'{EPA}/dynwave.c', f'{EPA}/dwflow.c'), L(f'{P}/dynwave.cpp', f'{P}/dwflow.cpp'), L(f'{H}/DynamicWave.*')],
    'cross sections': [L(f'{EPA}/xsect.c', f'{EPA}/xsect.dat'), L(f'{P}/xsect*', f'{P}/shape.cpp'), L(f'{H}/XSect*', f'{H}/XSection.cpp', f'{H}/xsect_tables.hpp')],
    'links, nodes, outfalls': [L(f'{EPA}/link.c', f'{EPA}/node.c'), L(f'{P}/link.cpp', f'{P}/node.cpp'), L(f'{H}/Link.*', f'{H}/Node.*', f'{H}/Outfall.*', f'{D}/LinkData.hpp', f'{D}/NodeData.hpp')],
    'input parsing': [L(f'{EPA}/input.c', f'{EPA}/project.c', f'{EPA}/keywords.c'), L(f'{P}/input.cpp', f'{P}/project.cpp', f'{P}/keywords.cpp'), lines(files(os.path.join(eng, 'input')))],
}
res['counts'] = {
    'parity_notes': grep(new, r'PARITY'),
    'getenv_calls': grep(new, r'getenv\('),
    'getenv_names': sorted(set(n for p in new for n in re.findall(r'getenv\("([A-Z0-9_]+)"', open(p, encoding='latin-1').read()))),
    'throw': grep(new, r'\bthrow\b'),
    'omp_pragmas': grep(new, r'#pragma omp'),
    'thread_local_files': sorted(os.path.relpath(p, eng) for p in new if 'thread_local' in open(p, encoding='latin-1').read()),
    'api_functions': len(set(re.findall(r'\b(swmm_[a-z0-9_]+)\s*\(', ''.join(open(p, encoding='latin-1').read() for p in files(os.path.join(S6, 'include')))))),
    'epa_api_functions': len(set(re.findall(r'\b(swmm_[A-Za-z]+)\s*\(', open(os.path.join(EPA, 'include/swmm5.h')).read()))),
    'cplus_handle_functions': len(set(re.findall(r'\b(swmm5cp_[a-z_]+)\s*\(', open(os.path.join(CP, 'src/api/swmm5cp.h')).read()))),
}
# the parity corpus and the regression tolerances, as the repository states them
man = [l for l in open(os.path.join(S6, 'tests/parity/MANIFEST')) if l.strip() and not l.lstrip().startswith('#')]
res['parity_corpus'] = {'decks': len(man), 'fv': sum('FV' in l.split('\t', 1)[-1] for l in man),
                        'dynwave': sum('DYNWAVE' in l.split('\t', 1)[-1] for l in man)}
reg = open(os.path.join(S6, 'tests/regression/test_regression_suite.cpp')).read()
res['regression_cases'] = re.findall(r'\{"(\w+)",\s*"([\w.]+)",\s*([\d.]+),\s*([\d.]+)\}', reg)

# token distance of the legacy engine from EPA 5.2.4 (comments and layout removed)
def tokens(p):
    t = subprocess.run(['gcc', '-fpreprocessed', '-dD', '-E', '-P', '-x', 'c', p], capture_output=True, text=True).stdout
    return re.findall(r'[A-Za-z_]\w*|\d+\.?\d*(?:[eE][-+]?\d+)?|==|!=|<=|>=|&&|\|\||->|\+\+|--|[-+*/=<>!&|^%(){}\[\];,.?:~#"]', t)
import difflib
leg = os.path.join(S6, 'src/legacy/engine'); res['legacy_vs_524'] = {}
for f in ('dwflow.c', 'dynwave.c', 'flowrout.c', 'link.c', 'node.c', 'routing.c', 'xsect.c'):
    a, b = tokens(os.path.join(EPA, f)), tokens(os.path.join(leg, f))
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    changed = sum(max(i2 - i1, j2 - j1) for op, i1, i2, j1, j2 in sm.get_opcodes() if op != 'equal')
    res['legacy_vs_524'][f] = {'tokens_524': len(a), 'tokens_changed': changed}
json.dump(res, open(OUT, 'w'), indent=1)
print(json.dumps({k: v for k, v in res.items() if k != 'counts'}, indent=1)[:2500]); print(json.dumps(res['counts'])[:800])
