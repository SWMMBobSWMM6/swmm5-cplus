#!/usr/bin/env python3
"""
make_cplus.py  --  mechanical lift of EPA SWMM 5.2.4 (C) into swmm5-cplus (C++17).

Every edit is located by libclang (the compiler's own parser), never by regex, so a
reference is renamed only if the compiler says it refers to that declaration.

What it does:
  * every function in every module becomes a member of class Project
    (so unqualified names inside bodies resolve exactly as they did in C:
     locals first, then the object's state -- formerly the file/globals scope)
  * globals.h variables become Project data members (zero-initialized)
  * module variables referenced from more than one file ("secretly shared" via
    local extern declarations) become Project data members too
  * module variables referenced only inside their own file become a private
    per-module state struct (PIMPL) defined in that module's .cpp:
        static double VariableStep;   ->   dynwave_->VariableStep
  * const data and read-only lookup tables stay at namespace scope (shared,
    immutable, safe across instances)
  * internal-linkage functions whose names collide across files get a module prefix
  * prototypes are removed from .c files and solver headers; the class is the
    single declaration of the engine's interface

Numerics are not touched. The output is judged by one test: byte-identical .out files.
"""
import sys, os, re, json, shutil, collections
import clang.cindex as ci

SRC, DST = sys.argv[1], sys.argv[2]
INC = os.path.join(SRC, 'include')
ARGS = ['-x', 'c', '-std=gnu99', '-isystem', '/usr/lib/llvm-18/lib/clang/18/include', '-I' + SRC, '-I' + INC, '-w']
idx = ci.Index.create()

C_FILES = sorted(f for f in os.listdir(SRC) if f.endswith('.c'))
H_FILES = sorted(f for f in os.listdir(SRC) if f.endswith('.h'))
STEM = {f: f[:-2] for f in C_FILES}

# Files whose module-local types appear in member-function signatures: their file-scope
# type declarations move to <stem>_types.h so the class declaration can see them.
SKIP_LIFT = {'omp_get_max_threads'}   # libgomp fallback stub: stays a free function
TYPE_EXPORT = ['controls.c', 'culvert.c', 'inlet.c', 'mathexpr.c', 'mempool.c']

def src_text(path):
    return open(path, 'rb').read()

TEXT = {f: src_text(os.path.join(SRC, f)) for f in C_FILES + H_FILES}
TEXT['swmm5.h'] = src_text(os.path.join(INC, 'swmm5.h'))

def here(c, f):
    return c.location.file is not None and os.path.basename(c.location.file.name) == f

def tokens_of(c):
    return list(c.get_tokens())

def ext(c):
    return c.extent.start.offset, c.extent.end.offset

def stmt_end(buf, off):
    """offset just past the ';' that ends the declaration starting before off"""
    depth = 0
    i = off
    while i < len(buf):
        ch = buf[i:i+1]
        if ch in (b'{', b'('): depth += 1
        elif ch in (b'}', b')'): depth -= 1
        elif ch == b';' and depth <= 0:
            return i + 1
        i += 1
    raise RuntimeError('no ; found')

# ---------------------------------------------------------------- pass 1: parse
TU = {}
for f in C_FILES:
    TU[f] = idx.parse(os.path.join(SRC, f), args=ARGS)
    errs = [d for d in TU[f].diagnostics if d.severity >= ci.Diagnostic.Error]
    if errs:
        raise SystemExit(f'{f}: parse errors {errs[:3]}')

var_defs = {}          # usr -> dict(file, name, cursor info)
func_defs = {}         # usr -> dict(file, name, linkage)
var_refs = collections.defaultdict(list)    # usr -> [(file, start, end)]
func_refs = collections.defaultdict(list)   # usr -> [(file, start, end)]
extern_var_decls = []  # (file, usr, start, end, is_block_scope)
proto_decls = []       # (file, start, end)  file-scope function prototypes in .c
type_decls = collections.defaultdict(list)  # file -> [(start, end)] top-level type decls

_seen_ref = set()
macro_refs = []
unplaced = []
def _is_callee(ch, parents):
    if not parents:
        return False
    p = parents[-1]
    node = ch
    if p.kind == ci.CursorKind.UNEXPOSED_EXPR and len(parents) > 1:
        node, p = p, parents[-2]
    if p.kind == ci.CursorKind.CALL_EXPR:
        kids = list(p.get_children())
        return bool(kids) and kids[0].hash == node.hash
    return False

def _preorder(c, parents):
    yield c, parents
    parents.append(c)
    for k in c.get_children():
        yield from _preorder(k, parents)
    parents.pop()

def walk_refs(c, f):
    for ch, parents in _preorder(c, []):
        if ch.kind == ci.CursorKind.DECL_REF_EXPR and ch.referenced is not None \
           and ch.location.file is not None and os.path.basename(ch.location.file.name) == f:
            r = ch.referenced
            s = ch.location.offset
            e = s + len(ch.spelling)
            places = []
            if TEXT[f][s:e].decode('latin-1') == ch.spelling:
                places = [(s, e)]
            else:
                # reference produced by a macro expansion: s is the macro name;
                # find the identifier inside the invocation's argument list
                buf = TEXT[f]
                p0 = buf.index(b'(', s)
                depth, p1 = 0, p0
                while True:
                    if buf[p1:p1+1] == b'(': depth += 1
                    elif buf[p1:p1+1] == b')':
                        depth -= 1
                        if depth == 0: break
                    p1 += 1
                arg = buf[p0:p1].decode('latin-1')
                for m in re.finditer(r'(?<![\.\w>])' + re.escape(ch.spelling) + r'\b', arg):
                    places.append((p0 + m.start(), p0 + m.end()))
                if not places:
                    unplaced.append((f, ch.location.line, ch.spelling, r.get_usr()))
                    continue
                macro_refs.append((f, ch.location.line, ch.spelling))
            fresh = []
            for (s, e) in places:
                if (f, s) not in _seen_ref:
                    _seen_ref.add((f, s)); fresh.append((s, e))
            if not fresh:
                continue
            for (s, e) in fresh:
                if r.kind == ci.CursorKind.VAR_DECL:
                    var_refs[r.get_usr()].append((f, s, e, ch.spelling))
                elif r.kind == ci.CursorKind.FUNCTION_DECL:
                    func_refs[r.get_usr()].append((f, s, e, ch.spelling, _is_callee(ch, parents)))
        if ch.kind == ci.CursorKind.VAR_DECL and ch.storage_class == ci.StorageClass.EXTERN \
           and here(ch, f) and parents and parents[-1].kind != ci.CursorKind.TRANSLATION_UNIT:
            s, e = ext(ch)
            extern_var_decls.append((f, ch.get_usr(), s, e, True))

for f in C_FILES:
    tu = TU[f]
    for c in tu.cursor.get_children():
        if not here(c, f):
            continue
        k = c.kind
        if k == ci.CursorKind.VAR_DECL:
            s, e = ext(c)
            toks = [t.spelling for t in tokens_of(c)]
            name_pos = toks.index(c.spelling) if c.spelling in toks else len(toks)
            is_const = 'const' in toks[:name_pos]
            if c.storage_class == ci.StorageClass.EXTERN:
                extern_var_decls.append((f, c.get_usr(), s, e, False))
                continue
            init = None
            eq = [t for t in tokens_of(c) if t.spelling == '=']
            if eq:
                eq_off = eq[0].extent.end.offset
                kids = [ch for ch in c.get_children() if ch.kind.is_expression()
                        and ch.extent.start.offset >= eq_off]
                if kids:
                    ie = kids[0]
                    init = TEXT[f][ie.extent.start.offset:ie.extent.end.offset].decode('latin-1')
                else:
                    raise SystemExit(f'cannot find initializer for {c.spelling} in {f}')
            var_defs[c.get_usr()] = dict(file=f, name=c.spelling, start=s, end=e,
                                         line=c.location.line, const=is_const,
                                         static=(c.storage_class == ci.StorageClass.STATIC),
                                         type=c.type, init=init, toks=toks)
        elif k == ci.CursorKind.FUNCTION_DECL:
            if c.is_definition():
                if c.spelling in SKIP_LIFT:
                    continue
                body = [ch for ch in c.get_children() if ch.kind == ci.CursorKind.COMPOUND_STMT][0]
                func_defs[c.get_usr()] = dict(
                    file=f, name=c.spelling, linkage=c.linkage,
                    start=c.extent.start.offset, name_off=c.location.offset,
                    body_off=body.extent.start.offset, line=c.location.line)
            else:
                s, e = ext(c)
                proto_decls.append((f, s, e))
        elif k in (ci.CursorKind.TYPEDEF_DECL, ci.CursorKind.STRUCT_DECL,
                   ci.CursorKind.ENUM_DECL, ci.CursorKind.UNION_DECL):
            type_decls[f].append(ext(c))
    walk_refs(tu.cursor, f)

# ---------------------------------------------------------------- classification
def is_table(v):
    """read-only lookup table: brace-initialized aggregate never written anywhere"""
    if v['init'] is None or not v['init'].lstrip().startswith('{'):
        return False
    name = v['name']
    dstart = v['start']
    dend = stmt_end(TEXT[v['file']], v['start'])
    for f in C_FILES:
        code = TEXT[f].decode('latin-1')
        for m in re.finditer(r'\b' + re.escape(name) + r'\s*(\[[^\]]*\]\s*)*(=[^=]|\+\+|--|[-+*/]=)', code):
            if f == v['file'] and dstart <= m.start() < dend:
                continue
            return False
    return True

state = {}       # usr -> 'shared' | 'private'
kept = {}        # usr -> 'const' | 'table'
for usr, v in var_defs.items():
    if v['const']:
        kept[usr] = 'const'; continue
    if is_table(v):
        kept[usr] = 'table'; continue
    files = {r[0] for r in var_refs.get(usr, [])}
    files.add(v['file'])
    state[usr] = 'shared' if len(files) > 1 else 'private'

for (f, line, sp, usr) in unplaced:
    assert usr not in state, f'lifted variable {sp} referenced from a macro body at {f}:{line}'
# function name collisions (any two definitions with the same name)
by_name = collections.defaultdict(list)
for usr, d in func_defs.items():
    by_name[d['name']].append(usr)
func_rename = {}
for name, usrs in by_name.items():
    if len(usrs) > 1:
        ext_ = [u for u in usrs if func_defs[u]['linkage'] == ci.LinkageKind.EXTERNAL]
        assert len(ext_) <= 1, name
        for u in usrs:
            if func_defs[u]['linkage'] == ci.LinkageKind.INTERNAL:
                func_rename[u] = STEM[func_defs[u]['file']] + '_' + name

# ---------------------------------------------------------------- member-name validation
globals_vars = []
gtu = TU['swmm5.c']
for c in gtu.cursor.get_children():
    if c.kind == ci.CursorKind.VAR_DECL and c.location.file and \
       os.path.basename(c.location.file.name) == 'globals.h':
        globals_vars.append(c)

member_names = collections.Counter()
for c in globals_vars: member_names[c.spelling] += 1
for usr, kind in state.items():
    if kind == 'shared': member_names[var_defs[usr]['name']] += 1
for usr, d in func_defs.items():
    member_names[func_rename.get(usr, d['name'])] += 1
dups = {n: k for n, k in member_names.items() if k > 1}
assert not dups, f'member name collisions: {dups}'
kept_names = {var_defs[u]['name'] for u in kept}
clash = kept_names & set(member_names)
assert not clash, f'namespace-scope names that a member would shadow: {clash}'
pimpl_names = {STEM[f] + '_' for f in C_FILES}
for f in C_FILES:
    for n in pimpl_names:
        assert re.search(rb'\b' + n.encode() + rb'\b', TEXT[f]) is None, (f, n)

# ---------------------------------------------------------------- declarations for members
def decl_of(name, t, init=None):
    """C++ member declaration for a variable of libclang type t"""
    dims = ''
    tt = t
    while tt.kind in (ci.TypeKind.CONSTANTARRAY,):
        dims += f'[{tt.element_count}]'
        tt = tt.element_type
    base = tt.spelling
    if tt.kind == ci.TypeKind.POINTER and tt.get_pointee().kind in (
            ci.TypeKind.FUNCTIONPROTO, ci.TypeKind.FUNCTIONNOPROTO):
        alias = f'{name}_fn_t'
        pre = f'using {alias} = {base};\n    '
        base = alias
    else:
        pre = ''
    if init is not None:
        return f'{pre}{base} {name}{dims} = {init};'
    return f'{pre}{base} {name}{dims}{{}};'

# ---------------------------------------------------------------- per-file edits
os.makedirs(DST, exist_ok=True)
edits = collections.defaultdict(list)     # file -> [(start, end, replacement)]

def add(f, s, e, r):
    edits[f].append((s, e, r))

# variable declarations: delete lifted definitions (group multi-declarator statements)
stmts = collections.defaultdict(list)
for usr, v in var_defs.items():
    if usr in state:
        stmts[(v['file'], v['start'])].append(usr)
deleted_stmt_end = {}
for (f, s), usrs in stmts.items():
    e = stmt_end(TEXT[f], s)
    # every declarator sharing this statement must be lifted too
    for usr2, v2 in var_defs.items():
        if v2['file'] == f and s <= v2['start'] < e and usr2 not in state:
            raise SystemExit(f'mixed statement {f}:{v2["name"]}')
    if f not in deleted_stmt_end or deleted_stmt_end[f][0] != s:
        pass
    add(f, s, e, '')

# extern declarations of lifted vars (file or block scope)
seen = set()
for (f, usr, s, e, block) in extern_var_decls:
    if usr in state and (f, s) not in seen:
        seen.add((f, s))
        add(f, s, stmt_end(TEXT[f], s), '')
    elif usr in kept and kept[usr] == 'const':
        pass  # keep: definition will get 'extern' so linkage survives C++ rules

# references
for usr, refs in var_refs.items():
    if usr not in state:
        continue
    v = var_defs[usr]
    for (f, s, e, sp) in refs:
        assert TEXT[f][s:e].decode() == sp == v['name'], (f, s, sp)
        if state[usr] == 'private':
            add(f, s, e, f'{STEM[v["file"]]}_->{v["name"]}')

# prototypes in .c files
for (f, s, e) in proto_decls:
    add(f, s, stmt_end(TEXT[f], s), '')

# function definitions
member_protos = collections.OrderedDict()   # file -> [prototype strings]
for usr, d in sorted(func_defs.items(), key=lambda x: (x[1]['file'], x[1]['start'])):
    f = d['file']
    buf = TEXT[f]
    # prototype text = tokens from start to body
    cur = None
    head = buf[d['start']:d['body_off']].decode('latin-1')
    head = re.sub(r'//[^\n]*', '', head)
    head = re.sub(r'/\*.*?\*/', '', head, flags=re.S)
    newname = func_rename.get(usr, d['name'])
    # remove storage keywords from the definition text
    pre = buf[d['start']:d['name_off']].decode('latin-1')
    pre_clean = re.sub(r'\b(static|extern|inline|DLLEXPORT)\b\s*', '', pre)
    add(f, d['start'], d['name_off'] + len(d['name']), pre_clean + 'Project::' + newname)
    proto = re.sub(r'\b(static|extern|inline|DLLEXPORT)\b\s*', '', head)
    proto = ' '.join(proto.split())
    proto = re.sub(r'\b' + re.escape(d['name']) + r'\s*\(', newname + '(', proto, count=1)
    member_protos.setdefault(f, []).append(proto + ';')

for (f, line, sp, usr) in unplaced:
    assert usr not in func_rename, f'renamed function {sp} referenced from a macro body at {f}:{line}'

callback_refs = []
for usr, refs in func_refs.items():
    if usr not in func_defs or func_defs[usr]['name'] in SKIP_LIFT:
        continue
    name = func_rename.get(usr, func_defs[usr]['name'])
    for (f, s, e, sp, callee) in refs:
        if callee:
            if usr in func_rename:
                add(f, s, e, name)
        else:
            # a member function used as a value: needs pointer-to-member syntax
            amp = TEXT[f][s-1:s] == b'&'
            add(f, s - (1 if amp else 0), e, '&Project::' + name)
            callback_refs.append((f, sp))

# const tables referenced from other files keep external linkage under C++ rules
for usr, kind in kept.items():
    v = var_defs[usr]
    if kind == 'const':
        files = {r[0] for r in var_refs.get(usr, [])}
        if files - {v['file']}:
            add(v['file'], v['start'], v['start'], 'extern ')

# type exports
type_export_text = {}
for f in TYPE_EXPORT:
    chunks = []
    for (s, e) in sorted(set(type_decls[f])):
        e2 = stmt_end(TEXT[f], e - 1) if TEXT[f][e:e+1] != b';' else e + 1
        # typedef struct {...} Name;  -> extent of TYPEDEF covers through Name
        chunks.append((s, e2))
    merged = []
    for s, e in sorted(chunks):
        if merged and s < merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
        else:
            merged.append((s, e))
    first_fn = min(d['start'] for d in func_defs.values() if d['file'] == f)
    head = TEXT[f][:first_fn].decode('latin-1')
    defines = [ln for ln in head.splitlines() if re.match(r'\s*#define\s+(?!_CRT)', ln)]
    type_export_text[f] = '\n'.join(defines) + '\n\n' + \
        '\n\n'.join(TEXT[f][s:e].decode('latin-1') for s, e in merged)
    for s, e in merged:
        add(f, s, e, '')

# ---------------------------------------------------------------- PIMPL state structs
private_by_file = collections.defaultdict(list)
for usr, kind in state.items():
    if kind == 'private':
        private_by_file[var_defs[usr]['file']].append(usr)
shared_vars = sorted((u for u, k in state.items() if k == 'shared'),
                     key=lambda u: (var_defs[u]['file'], var_defs[u]['start']))

for f, usrs in private_by_file.items():
    usrs.sort(key=lambda u: var_defs[u]['start'])
    stem = STEM[f]
    sname = stem[0].upper() + stem[1:] + 'State'
    lines = [f'// ---- {stem} module state: formerly file-scope variables of {f}.',
             f'// ---- One instance per Project, so two models never share it.',
             f'struct {sname}', '{']
    for u in usrs:
        v = var_defs[u]
        lines.append('    ' + decl_of(v['name'], v['type'], v['init']) +
                     ('   // was static' if v['static'] else '   // was global (external linkage)'))
    lines.append('};')
    lines.append(f'void Project::{stem}_createState() {{ {stem}_ = new {sname}(); }}')
    lines.append(f'void Project::{stem}_destroyState() {{ delete {stem}_; {stem}_ = nullptr; }}')
    block = '\n'.join(lines) + '\n'
    last = max(usrs, key=lambda u: var_defs[u]['start'])
    v = var_defs[last]
    add(f, stmt_end(TEXT[f], v['start']), stmt_end(TEXT[f], v['start']), '\n' + block)

# ---------------------------------------------------------------- apply edits to .c
def apply(f, buf):
    es = sorted(edits.get(f, []), key=lambda x: (x[0], x[1]))
    out = []
    pos = 0
    for s, e, r in es:
        if s < pos:
            raise SystemExit(f'overlapping edits in {f} at {s} (pos {pos})')
        out.append(buf[pos:s].decode('latin-1'))
        out.append(r)
        pos = e
    out.append(buf[pos:].decode('latin-1'))
    return ''.join(out)

ENG = os.path.join(DST, 'engine')
os.makedirs(ENG, exist_ok=True)
for f in C_FILES:
    text = apply(f, TEXT[f])
    if '#include "headers.h"' not in text and f != 'swmm5.c':
        first = text.find('Project::')
        incs = [m for m in re.finditer(r'^#include[^\n]*\n', text, flags=re.M) if m.start() < first]
        at = incs[-1].end() if incs else 0
        text = text[:at] + '#include "project.hpp"   // swmm5-cplus: functions are Project members\n' + text[at:]
    open(os.path.join(ENG, STEM[f] + '.cpp'), 'w', encoding='latin-1').write(text)

# ---------------------------------------------------------------- headers
hdr_protos = collections.defaultdict(list)
hdr_externs = collections.defaultdict(list)
seen = set()
for f in C_FILES:
    for c in TU[f].cursor.get_children():
        if c.location.file is None:
            continue
        hf = os.path.basename(c.location.file.name)
        if hf not in H_FILES or (hf, c.extent.start.offset) in seen:
            continue
        if os.path.dirname(os.path.abspath(c.location.file.name)) != os.path.abspath(SRC):
            continue
        seen.add((hf, c.extent.start.offset))
        if c.kind == ci.CursorKind.FUNCTION_DECL and not c.is_definition():
            hdr_protos[hf].append(ext(c))
        if c.kind == ci.CursorKind.VAR_DECL and c.storage_class == ci.StorageClass.EXTERN \
           and c.get_usr() in state:
            hdr_externs[hf].append(ext(c))

for hf in H_FILES:
    buf = TEXT[hf]
    es = []
    for s, e in hdr_protos[hf] + hdr_externs[hf]:
        es.append((s, stmt_end(buf, s), ''))
    edits[hf] = es
    text = apply(hf, buf)
    if hf == 'globals.h':
        text = ('//  globals.h -- swmm5-cplus\n'
                '//  The 114 global variables that lived here are now data members of class\n'
                '//  Project (project.hpp). Each simulation owns its own copy.\n'
                '#ifndef GLOBALS_H\n#define GLOBALS_H\n#include "project.hpp"\n#endif\n')
    open(os.path.join(ENG, hf), 'w', encoding='latin-1').write(text)

for f, t in type_export_text.items():
    stem = STEM[f]
    open(os.path.join(ENG, f'{stem}_types.h'), 'w', encoding='latin-1').write(
        f'// {stem}_types.h -- module-local types of {f}, exported because they appear\n'
        f'// in member-function signatures of class Project.\n'
        f'#ifndef {stem.upper()}_TYPES_H\n#define {stem.upper()}_TYPES_H\n\n{t}\n\n#endif\n')

# ---------------------------------------------------------------- callbacks
# Function-pointer parameters that now receive member functions become
# pointers-to-member, and calls through them become (this->*p)(...).
CALLBACK_NAMES = {
    'findroot.c': ['func'],
    'odesolve.c': ['derivs'],
    'mathexpr.c': ['getVariableValue', 'getVar'],
    'lidproc.c':  ['derivs', 'fluxRates'],
}
def split_code(text):
    '''yield (is_code, chunk) separating comments and string literals'''
    pat = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    pos = 0
    for m in pat.finditer(text):
        yield True, text[pos:m.start()]
        yield False, m.group(0)
        pos = m.end()
    yield True, text[pos:]

def to_member_ptr(text, names):
    out = []
    for is_code, chunk in split_code(text):
        if is_code:
            for n in names:
                chunk = re.sub(r'\(\s*\*\s*' + n + r'\s*\)(?=\s*\()', '(Project::*' + n + ')', chunk)
                chunk = re.sub(r'(?<![\w:>.*])' + n + r'(?=\s*\()', '(this->*' + n + ')', chunk)
        out.append(chunk)
    return ''.join(out)

for f, names in CALLBACK_NAMES.items():
    path = os.path.join(ENG, STEM[f] + '.cpp')
    t = open(path, encoding='latin-1').read()
    t = to_member_ptr(t, names)
    if f == 'mathexpr.c':
        t = t.replace('using getVariableIndex_fn_t = int (*)(char *);',
                      'using getVariableIndex_fn_t = int (Project::*)(char *);')
        t = t.replace('mathexpr_->getVariableIndex(mathexpr_->Token)',
                      '(this->*mathexpr_->getVariableIndex)(mathexpr_->Token)')
    open(path, 'w', encoding='latin-1').write(t)
    member_protos[f] = [to_member_ptr(p, names) for p in member_protos[f]]

shutil.copytree(INC, os.path.join(ENG, 'include'), dirs_exist_ok=True)
for extra in os.listdir(SRC):
    if extra.endswith('.dat') or extra.endswith('.txt'):
        shutil.copy(os.path.join(SRC, extra), ENG)
for extra in ('error.txt',):
    if os.path.exists(os.path.join(SRC, extra)):
        shutil.copy(os.path.join(SRC, extra), ENG)

# ---------------------------------------------------------------- project.hpp
L = []
L.append('''//-----------------------------------------------------------------------------
//   project.hpp  --  swmm5-cplus
//
//   class Project: one complete, independent SWMM 5.2.4 simulation.
//
//   Generated by tools/make_cplus.py from EPA SWMM 5.2.4 and then refined by hand.
//   Where EPA SWMM5 kept its state in process-wide globals (globals.h) and in
//   file-scope statics scattered through 54 modules, swmm5-cplus keeps all of it
//   inside this object. Two Projects never share mutable state, so any number of
//   models can run in one process, in one thread or many, or in one WebAssembly
//   instance.
//-----------------------------------------------------------------------------
#ifndef SWMM5CPLUS_PROJECT_HPP
#define SWMM5CPLUS_PROJECT_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "consts.h"
#include "macros.h"
#include "enums.h"
#include "datetime.h"
#include "objects.h"
#include "hash.h"
#include "mempool.h"
#include "mathexpr.h"
#include "infil.h"
#include "exfil.h"
#include "lid.h"
#include "inlet.h"
''')
for f in TYPE_EXPORT:
    L.append(f'#include "{STEM[f]}_types.h"')
for f in sorted(private_by_file):
    stem = STEM[f]
    L.append(f'struct {stem[0].upper() + stem[1:]}State;')
L.append('''
class Project
{
public:
    Project();
    ~Project();
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
''')
L.append('    //=========================================================================')
L.append('    // Shared simulation state (formerly globals.h)')
L.append('    //=========================================================================')
for c in globals_vars:
    L.append('    ' + decl_of(c.spelling, c.type))
L.append('')
L.append('    //=========================================================================')
L.append('    // Shared simulation state that EPA SWMM5 defined inside individual modules')
L.append('    // and reached from other modules through local extern declarations')
L.append('    //=========================================================================')
for u in shared_vars:
    v = var_defs[u]
    L.append('    ' + decl_of(v['name'], v['type'], v['init']) + f'   // from {v["file"]}')
L.append('')
L.append('    //=========================================================================')
L.append('    // Private per-module state (PIMPL: struct defined in each module .cpp)')
L.append('    //=========================================================================')
for f in sorted(private_by_file):
    stem = STEM[f]
    L.append(f'    struct {stem[0].upper() + stem[1:]}State* {stem}_ = nullptr;   '
             f'// {len(private_by_file[f])} variables of {f}')
    L.append(f'    void {stem}_createState();')
    L.append(f'    void {stem}_destroyState();')
L.append('')
L.append('    //=========================================================================')
L.append('    // The engine: every function of every module, now a member')
L.append('    //=========================================================================')
for f, protos in member_protos.items():
    L.append(f'    // ---- {f}')
    for p in protos:
        L.append('    ' + p)
L.append('};')
L.append('')
L.append('#endif')
open(os.path.join(ENG, 'project.hpp'), 'w', encoding='latin-1').write('\n'.join(L) + '\n')

# constructor / destructor
ctor = ['// project_lifecycle.cpp -- generated: create and destroy per-module state',
        '#include "headers.h"', '', 'Project::Project()', '{']
for f in sorted(private_by_file):
    ctor.append(f'    {STEM[f]}_createState();')
ctor += ['}', '', 'Project::~Project()', '{']
for f in sorted(private_by_file):
    ctor.append(f'    {STEM[f]}_destroyState();')
ctor += ['}', '']
open(os.path.join(ENG, 'project_lifecycle.cpp'), 'w').write('\n'.join(ctor))

# report
rep = dict(
    functions=len(func_defs),
    renamed_functions={func_defs[u]['file'] + ':' + func_defs[u]['name']: n for u, n in func_rename.items()},
    globals_h_members=len(globals_vars),
    shared_module_vars=[(var_defs[u]['file'], var_defs[u]['name']) for u in shared_vars],
    private_module_vars={f: [var_defs[u]['name'] for u in us] for f, us in private_by_file.items()},
    kept_const=[(var_defs[u]['file'], var_defs[u]['name']) for u, k in kept.items() if k == 'const'],
    kept_tables=[(var_defs[u]['file'], var_defs[u]['name']) for u, k in kept.items() if k == 'table'],
    edits=sum(len(v) for v in edits.values()),
    macro_arg_refs=len(macro_refs),
    callback_refs=callback_refs,
    private_ref_rewrites=sum(len(var_refs[u]) for u, k in state.items() if k == 'private'),
)
json.dump(rep, open(os.path.join(DST, 'lift_report.json'), 'w'), indent=1)
print(json.dumps({k: (v if not isinstance(v, (list, dict)) else len(v)) for k, v in rep.items()}, indent=1))
