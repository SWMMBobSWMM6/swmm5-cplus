#!/usr/bin/env python3
"""wasm parity: native EPA 5.2.4 vs swmm5-cplus.wasm vs EPA wasm, byte for byte.

  wasm_gate.py REF_EXE CPLUS_WASM EPA_WASM WASM_RUN_MJS LIST OUT.jsonl
"""
import sys, os, subprocess, json, tempfile, hashlib
ref, cpw, epw, mjs, lst, outp = [os.path.abspath(a) for a in sys.argv[1:7]]
CLOCK = (b'Analysis begun on', b'Analysis ended on', b'Total elapsed time')
def norm(b): return b'\n'.join(l for l in b.split(b'\n') if not any(c in l for c in CLOCK))
def rd(p): return open(p, 'rb').read() if os.path.exists(p) else b''
models = [os.path.abspath(l.strip()) for l in open(lst) if l.strip()]
done = set()
if os.path.exists(outp):
    done = {json.loads(l)['inp'] for l in open(outp)}
with open(outp, 'a') as fo:
    for m in models:
        if m in done: continue
        n = os.path.basename(m)[:-4]
        with tempfile.TemporaryDirectory(dir='/tmp/claude-0') as d:
            try:
                subprocess.run([ref, m, d + '/ref.rpt', d + '/ref.out'], cwd=os.path.dirname(m),
                               capture_output=True, timeout=300)
                for tag, w in (('cp', cpw), ('epa', epw)):
                    subprocess.run(['node', mjs, 'single', w, m, d + '/' + tag], capture_output=True, timeout=900)
            except subprocess.TimeoutExpired:
                fo.write(json.dumps(dict(inp=m, timeout=True)) + '\n'); fo.flush(); continue
            ro, rr = rd(d + '/ref.out'), rd(d + '/ref.rpt')
            co, cr = rd(f'{d}/cp/{n}.out'), rd(f'{d}/cp/{n}.rpt')
            eo, er = rd(f'{d}/epa/{n}.out'), rd(f'{d}/epa/{n}.rpt')
            rec = dict(inp=m, out_bytes=len(eo),
                       # the restructure claim: same toolchain, C vs C++
                       wasm_out=(co == eo), wasm_rpt=(norm(cr) == norm(er)),
                       # the platform note: same C source, glibc native vs wasi-libc wasm
                       native_out=(eo == ro), native_rpt=(norm(er) == norm(rr)),
                       cp_native_out=(co == ro),
                       sha=hashlib.sha256(eo).hexdigest()[:16])
        fo.write(json.dumps(rec) + '\n'); fo.flush()
        if not (rec['wasm_out'] and rec['wasm_rpt']):
            print('WASM C vs C++ DIFF', m, rec, flush=True)
print('done')
