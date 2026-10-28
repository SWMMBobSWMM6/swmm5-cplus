// wasm_run.mjs -- drive the swmm5-cplus and EPA 5.2.4 wasm reactors from node.
//
//   node wasm_run.mjs single <engine.wasm> <model.inp> <outdir>       one run, legacy swmm_run
//   node wasm_run.mjs multi  <swmm5cplus.wasm> <outdir> <a.inp> <b.inp> ...
//        all models opened in ONE wasm instance, stepped round-robin, one handle each
//
// Writes <name>.rpt / <name>.out into outdir. Prints JSON lines.
import { WASI } from 'node:wasi';
import fs from 'node:fs';
import path from 'node:path';
process.removeAllListeners('warning');

function load(wasmPath, preopens) {
  const wasi = new WASI({ version: 'preview1', args: [], env: {}, preopens });
  const mod = new WebAssembly.Module(fs.readFileSync(wasmPath));
  const inst = new WebAssembly.Instance(mod, wasi.getImportObject());
  wasi.initialize(inst);
  const ex = inst.exports;
  const enc = new TextEncoder();
  const cstr = (s) => {
    const b = enc.encode(s + '\0');
    const p = ex.malloc(b.length);
    new Uint8Array(ex.memory.buffer, p, b.length).set(b);
    return p;
  };
  const getErr = (h) => {
    const p = ex.malloc(256);
    if (h === undefined) ex.swmm_getError(p, 255); else ex.swmm5cp_getError(h, p, 255);
    const m = new Uint8Array(ex.memory.buffer, p, 256);
    let s = ''; for (let i = 0; i < 256 && m[i]; i++) s += String.fromCharCode(m[i]);
    ex.free(p);
    return s;
  };
  return { ex, cstr, getErr };
}

const [mode, ...rest] = process.argv.slice(2);
if (mode === 'single') {
  const [wasmPath, inp, outdir] = rest;
  fs.mkdirSync(outdir, { recursive: true });
  const name = path.basename(inp).replace(/\.inp$/i, '');
  const { ex, cstr, getErr } = load(wasmPath, { '/': outdir, '/model': path.dirname(path.resolve(inp)) });
  const t0 = performance.now();
  const rc = ex.swmm_run(cstr('/model/' + path.basename(inp)), cstr('/' + name + '.rpt'), cstr('/' + name + '.out'));
  console.log(JSON.stringify({ mode, name, rc, ms: +(performance.now() - t0).toFixed(1), err: rc ? getErr() : '' }));
} else if (mode === 'multi') {
  const [wasmPath, outdir, ...inps] = rest;
  fs.mkdirSync(outdir, { recursive: true });
  const pre = { '/': outdir };
  inps.forEach((p, i) => { pre['/m' + i] = path.dirname(path.resolve(p)); });
  const { ex, cstr, getErr } = load(wasmPath, pre);
  const H = inps.map((p, i) => {
    const h = ex.swmm5cp_create();
    const name = path.basename(p).replace(/\.inp$/i, '');
    let rc = ex.swmm5cp_open(h, cstr(`/m${i}/` + path.basename(p)), cstr(`/${name}.rpt`), cstr(`/${name}.out`));
    if (!rc) rc = ex.swmm5cp_start(h, 1);
    return { h, name, rc, steps: 0, done: rc !== 0 };
  });
  const live = ex.swmm5cp_count();
  const tp = ex.malloc(8);
  const f64 = () => new Float64Array(ex.memory.buffer, tp, 1);
  // round-robin: one step of each open model in turn
  while (H.some(x => !x.done)) {
    for (const x of H) {
      if (x.done) continue;
      const rc = ex.swmm5cp_step(x.h, tp);
      x.steps++;
      if (rc || f64()[0] <= 0) { x.done = true; x.rc = rc || x.rc; }
    }
  }
  for (const x of H) {
    ex.swmm5cp_end(x.h);   // like swmm_run: no time-series report when a .out file is saved
    if (x.rc) x.err = getErr(x.h);
    ex.swmm5cp_close(x.h);
    ex.swmm5cp_destroy(x.h);
  }
  console.log(JSON.stringify({ mode, live, after: ex.swmm5cp_count(),
    runs: H.map(x => ({ name: x.name, rc: x.rc, steps: x.steps, err: x.err || '' })) }));
}
