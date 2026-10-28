#!/usr/bin/env python3
"""Census of EPA SWMM5 solver structure using libclang (exact, not regex).

For every .c file: file-scope variables (static / global definitions / const tables),
functions (static / external), and counts of structural smells. Writes census.json.
"""
import sys, os, json, re, collections
import clang.cindex as ci

SRC = sys.argv[1]
OUT = sys.argv[2]
idx = ci.Index.create()
args = ['-x', 'c', '-std=gnu99', '-isystem', '/usr/lib/llvm-18/lib/clang/18/include', '-I' + SRC, '-I' + os.path.join(SRC, 'include'), '-w']

def is_const(t):
    t = t.get_canonical()
    while t.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
        t = t.element_type.get_canonical()
    return t.is_const_qualified()

files = sorted(f for f in os.listdir(SRC) if f.endswith('.c'))
census = {'files': {}, 'globals_h': [], 'totals': {}}
all_static_funcs = collections.defaultdict(list)
all_static_vars = collections.defaultdict(list)

for f in files:
    path = os.path.join(SRC, f)
    tu = idx.parse(path, args=args)
    rec = {'lines': sum(1 for _ in open(path, errors='replace')),
           'static_vars': [], 'mutable_static_vars': [], 'const_tables': [],
           'global_defs': [], 'static_funcs': [], 'extern_funcs': []}
    for c in tu.cursor.get_children():
        if not c.location.file or os.path.basename(c.location.file.name) != f:
            continue
        if c.kind == ci.CursorKind.VAR_DECL:
            sc = c.storage_class
            const = is_const(c.type)
            if c.linkage == ci.LinkageKind.INTERNAL and not const or sc == ci.StorageClass.STATIC:
                rec['static_vars'].append(c.spelling)
                (rec['const_tables'] if const else rec['mutable_static_vars']).append(c.spelling)
                all_static_vars[c.spelling].append(f)
            elif sc == ci.StorageClass.NONE:
                if const:
                    rec['const_tables'].append(c.spelling)
                else:
                    rec['global_defs'].append(c.spelling)
        elif c.kind == ci.CursorKind.FUNCTION_DECL and c.is_definition():
            if c.linkage == ci.LinkageKind.INTERNAL:
                rec['static_funcs'].append(c.spelling)
                all_static_funcs[c.spelling].append(f)
            else:
                rec['extern_funcs'].append(c.spelling)
    txt = open(path, errors='replace').read()
    code = re.sub(r'//.*', '', txt)
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'"(\\.|[^"\\])*"', '""', code)
    rec['switch_on_type'] = len(re.findall(r'switch\s*\(\s*[^)]*\.type\s*\)', code))
    rec['switch_total'] = len(re.findall(r'\bswitch\s*\(', code))
    rec['errorcode_refs'] = len(re.findall(r'\bErrorCode\b', code))
    rec['alloc_calls'] = len(re.findall(r'\b(malloc|calloc|realloc)\s*\(', code))
    rec['free_calls'] = len(re.findall(r'\b(free|FREE)\s*\(', code))
    rec['report_io'] = len(re.findall(r'\b(fprintf|report_write\w*|report_Write\w*)\s*\(', code))
    census['files'][f] = rec

# globals.h variables
tu = idx.parse(os.path.join(SRC, 'swmm5.c'), args=args)
for c in tu.cursor.get_children():
    if c.kind == ci.CursorKind.VAR_DECL and c.location.file and \
       os.path.basename(c.location.file.name) == 'globals.h':
        census['globals_h'].append(c.spelling)

F = census['files']
T = census['totals']
T['c_files'] = len(F)
T['c_lines'] = sum(r['lines'] for r in F.values())
T['globals_h_vars'] = len(census['globals_h'])
T['mutable_file_statics'] = sum(len(r['mutable_static_vars']) for r in F.values())
T['files_with_mutable_statics'] = sum(1 for r in F.values() if r['mutable_static_vars'])
T['const_tables'] = sum(len(r['const_tables']) for r in F.values())
T['extra_global_defs'] = sum(len(r['global_defs']) for r in F.values())
T['static_funcs'] = sum(len(r['static_funcs']) for r in F.values())
T['extern_funcs'] = sum(len(r['extern_funcs']) for r in F.values())
T['switch_on_type'] = sum(r['switch_on_type'] for r in F.values())
T['switch_total'] = sum(r['switch_total'] for r in F.values())
T['errorcode_refs'] = sum(r['errorcode_refs'] for r in F.values())
T['alloc_calls'] = sum(r['alloc_calls'] for r in F.values())
T['free_calls'] = sum(r['free_calls'] for r in F.values())
census['static_func_collisions'] = {k: v for k, v in all_static_funcs.items() if len(v) > 1}
census['static_var_collisions'] = {k: v for k, v in all_static_vars.items() if len(v) > 1}
T['static_func_name_collisions'] = len(census['static_func_collisions'])
T['static_var_name_collisions'] = len(census['static_var_collisions'])
json.dump(census, open(OUT, 'w'), indent=1)
print(json.dumps(T, indent=1))
print('func collisions:', census['static_func_collisions'])
print('var collisions:', census['static_var_collisions'])
