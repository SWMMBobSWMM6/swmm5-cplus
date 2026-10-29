"""Does a patched build give byte-identical output?  buffer_identity.py LIST OUT.json ORIG_EXE PATCHED_EXE [CORPUS]
.out compared byte for byte; .rpt compared without its clock lines."""
import os, sys, subprocess, hashlib, tempfile, json
A, B = os.path.abspath(sys.argv[3]), os.path.abspath(sys.argv[4])
C = sys.argv[5] if len(sys.argv) > 5 else os.environ.get('SWMM_CORPUS', '/home/claude/corpus1729')
CLOCK=(b'Analysis begun', b'Analysis ended', b'Total elapsed', b'Elapsed')
def h(p, rpt=False):
    if not os.path.exists(p): return None
    b=open(p,'rb').read()
    if rpt: b=b'\n'.join(l for l in b.split(b'\n') if not any(c in l for c in CLOCK))
    return hashlib.sha256(b).hexdigest()[:16]
res=[]
for m in [l.strip() for l in open(sys.argv[1]) if l.strip()]:
    inp=os.path.join(C,m); out={}
    for k,exe in (('orig',A),('buf',B)):
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR', '/tmp')) as d:
            try: rc=subprocess.run([exe,inp,d+'/m.rpt',d+'/m.out'],cwd=os.path.dirname(inp),capture_output=True,timeout=300).returncode
            except subprocess.TimeoutExpired: rc='timeout'
            out[k]=(rc,h(d+'/m.out'),h(d+'/m.rpt',True))
    res.append(dict(inp=m, orig=out['orig'], buf=out['buf'], same_out=out['orig'][1]==out['buf'][1], same_rpt=out['orig'][2]==out['buf'][2]))
json.dump(res, open(sys.argv[2],'w'), indent=1)
print(sum(r['same_out'] for r in res), 'of', len(res), '.out identical;', sum(r['same_rpt'] for r in res), '.rpt identical')
for r in res:
    if not (r['same_out'] and r['same_rpt']): print('DIFF', r)
