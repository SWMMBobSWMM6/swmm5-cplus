#!/usr/bin/env python3
"""Find the reentrancy and lift hazards in EPA SWMM5 that a member-lift must handle:
  1. static locals inside functions (hidden per-process state)
  2. functions used as values (callbacks: qsort, odesolve, findroot, mathexpr)
  3. file-scope variables that are written (true mutable state) vs read-only tables
  4. names that would become class members AND are referenced elsewhere as non-member
     entities (libc functions, enum constants, typedefs) -> lookup would change
"""
import sys, os, json, collections
import clang.cindex as ci

SRC = sys.argv[1]
idx = ci.Index.create()
args = ['-x', 'c', '-std=gnu99', '-isystem', '/usr/lib/llvm-18/lib/clang/18/include', '-I' + SRC, '-I' + os.path.join(SRC, 'include'), '-w']
files = sorted(f for f in os.listdir(SRC) if f.endswith('.c'))

static_locals = []
fn_as_value = []
filevar_decls = {}          # usr -> (file, name)
filevar_writes = collections.Counter()
func_usrs = {}              # usr -> name for functions defined in solver .c files
external_refs = collections.defaultdict(set)   # name -> set(kind) for refs to decls NOT in solver .c/globals
WRITE_OPS = {'=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='}

def in_solver(cur):
    loc = cur.location
    return loc.file and os.path.dirname(os.path.abspath(loc.file.name)) == os.path.abspath(SRC)

def walk(c, parents, fname):
    k = c.kind
    if k == ci.CursorKind.VAR_DECL and c.storage_class == ci.StorageClass.STATIC \
       and c.semantic_parent and c.semantic_parent.kind == ci.CursorKind.FUNCTION_DECL:
        static_locals.append((fname, c.semantic_parent.spelling, c.spelling, c.type.spelling))
    if k == ci.CursorKind.DECL_REF_EXPR and c.referenced is not None:
        ref = c.referenced
        if ref.kind == ci.CursorKind.FUNCTION_DECL:
            # is it the callee of a call?
            p = parents[-1] if parents else None
            pp = parents[-2] if len(parents) > 1 else None
            is_callee = False
            if p is not None and p.kind == ci.CursorKind.CALL_EXPR:
                kids = list(p.get_children())
                is_callee = bool(kids) and kids[0].hash == c.hash
            if p is not None and p.kind == ci.CursorKind.UNEXPOSED_EXPR and pp is not None \
               and pp.kind == ci.CursorKind.CALL_EXPR:
                kids = list(pp.get_children())
                is_callee = bool(kids) and kids[0].hash == p.hash
            if not is_callee:
                fn_as_value.append((fname, c.location.line, ref.spelling,
                                    parents[-1].kind.name if parents else ''))
        if ref.kind == ci.CursorKind.VAR_DECL and ref.semantic_parent and \
           ref.semantic_parent.kind == ci.CursorKind.TRANSLATION_UNIT:
            # write detection: walk up through subscripts / member refs / parens / casts
            node = c; i = len(parents) - 1
            while i >= 0 and parents[i].kind in (ci.CursorKind.ARRAY_SUBSCRIPT_EXPR,
                    ci.CursorKind.MEMBER_REF_EXPR, ci.CursorKind.PAREN_EXPR,
                    ci.CursorKind.UNEXPOSED_EXPR):
                node = parents[i]; i -= 1
            p = parents[i] if i >= 0 else None
            written = False
            if p is not None:
                if p.kind in (ci.CursorKind.BINARY_OPERATOR, ci.CursorKind.COMPOUND_ASSIGNMENT_OPERATOR):
                    kids = list(p.get_children())
                    if kids and kids[0].hash == node.hash:
                        toks = [t.spelling for t in p.get_tokens()]
                        lhs_toks = len(list(kids[0].get_tokens()))
                        if lhs_toks < len(toks) and toks[lhs_toks] in WRITE_OPS:
                            written = True
                elif p.kind == ci.CursorKind.UNARY_OPERATOR:
                    toks = [t.spelling for t in p.get_tokens()]
                    if toks and (toks[0] in ('++', '--', '&') or toks[-1] in ('++', '--')):
                        written = True
                elif p.kind == ci.CursorKind.CALL_EXPR:
                    written = True      # passed to a function: may be written through
            if written:
                filevar_writes[ref.get_usr()] += 1
        if ref.kind not in (ci.CursorKind.PARM_DECL,) and not (ref.kind == ci.CursorKind.VAR_DECL
                and ref.semantic_parent and ref.semantic_parent.kind == ci.CursorKind.FUNCTION_DECL):
            external_refs[ref.spelling].add((ref.kind.name, ref.get_usr()))
    if k == ci.CursorKind.TYPE_REF:
        external_refs[c.spelling.replace('struct ', '')].add(('TYPE',''))
    parents.append(c)
    for ch in c.get_children():
        walk(ch, parents, fname)
    parents.pop()

for f in files:
    tu = idx.parse(os.path.join(SRC, f), args=args)
    for c in tu.cursor.get_children():
        if not c.location.file or os.path.basename(c.location.file.name) != f:
            continue
        if c.kind == ci.CursorKind.VAR_DECL:
            filevar_decls[c.get_usr()] = (f, c.spelling, c.type.is_const_qualified())
        if c.kind == ci.CursorKind.FUNCTION_DECL and c.is_definition():
            func_usrs[c.get_usr()] = c.spelling
    walk(tu.cursor, [], f)

print('== static locals (hidden per-process state) ==')
for s in static_locals: print('  ', s)
print('== functions used as values (callbacks) ==')
for s in sorted(set((a, c, d) for a, b, c, d in fn_as_value)): print('  ', s)
print('== file-scope vars never written (read-only tables) ==')
ro = sorted((f, n) for u, (f, n, cq) in filevar_decls.items() if filevar_writes[u] == 0 and not cq)
print('  ', ro)
member_names = set(func_usrs.values())
our_usrs = set(func_usrs.keys())
conf = {}
for n in member_names:
    others = {(k,u) for (k,u) in external_refs.get(n, set()) if k != 'FUNCTION_DECL'}
    if others: conf[n] = others
print('== member-name / external-entity conflicts ==')
print('  ', conf)
json.dump({'static_locals': static_locals, 'fn_as_value': fn_as_value, 'readonly_tables': ro,
           'conflicts': {k: sorted(map(list,v)) for k, v in conf.items()}},
          open(sys.argv[2], 'w'), indent=1)
