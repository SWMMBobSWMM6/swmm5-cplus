// What does EPA 5.2.4 do when a host opens a second model in the same instance?
const fs = require('fs'); const core = require('./engine_core.js');
const mE = new WebAssembly.Module(fs.readFileSync('/home/claude/swmm5-cplus/build/wasm/swmm524_epa.wasm'));
const A = fs.readFileSync('/home/claude/corpus1729/EPA/extran1_egg.inp');
const B = fs.readFileSync('/home/claude/corpus1729/EPA/exam79.inp');
const eng = core.Engine(mE, { 'a.inp': A, 'b.inp': B });
const ex = eng.ex, s = eng.cstr;
const r1 = ex.swmm_open(s('/a.inp'), s('/a.rpt'), s('/a.out'));
const nA = ex.swmm_getCount(2);
const r2 = ex.swmm_open(s('/b.inp'), s('/b.rpt'), s('/b.out'));
const nB = ex.swmm_getCount(2);
console.log(JSON.stringify({ openA: r1, nodesAfterA: nA, openB: r2, nodesAfterB: nB, err: eng.errText() }));
// now try to start and step "A" -- which model actually runs?
const r3 = ex.swmm_start(1);
let steps = 0, rc = 0;
do { rc = ex.swmm_step(eng.dblPtr); steps++; } while (!rc && eng.f64() > 0 && steps < 1e7);
ex.swmm_end(); ex.swmm_close();
console.log(JSON.stringify({ start: r3, steps, rc, aRpt: (eng.fs.read('a.rpt')||[]).length, bRpt: (eng.fs.read('b.rpt')||[]).length, aOut: (eng.fs.read('a.out')||[]).length, bOut: (eng.fs.read('b.out')||[]).length }));
