import sys, os, re
import clang.cindex as ci
SRC=sys.argv[1]
idx=ci.Index.create(); args=['-x','c','-std=c99','-I'+SRC,'-I'+SRC+'/include','-w']
for f in sorted(x for x in os.listdir(SRC) if x.endswith('.c')):
    tu=idx.parse(os.path.join(SRC,f),args=args)
    local_types=set(); macros=[]
    for c in tu.cursor.get_children():
        if not c.location.file or os.path.basename(c.location.file.name)!=f: continue
        if c.kind in (ci.CursorKind.TYPEDEF_DECL, ci.CursorKind.STRUCT_DECL, ci.CursorKind.ENUM_DECL, ci.CursorKind.UNION_DECL) and c.spelling:
            local_types.add(c.spelling)
    if not local_types: continue
    sig_uses=set()
    for c in tu.cursor.get_children():
        if not c.location.file or os.path.basename(c.location.file.name)!=f: continue
        if c.kind==ci.CursorKind.FUNCTION_DECL:
            toks=[c.result_type.spelling]+[a.type.spelling for a in c.get_arguments()]
            for t in toks:
                for lt in local_types:
                    if re.search(r'\b'+re.escape(lt)+r'\b', t): sig_uses.add((c.spelling, lt))
    print(f, sorted(local_types), 'SIG:', sorted(sig_uses))
