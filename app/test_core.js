// node test: engine_core.js (the app's own runner) vs native EPA 5.2.4 outputs
const fs = require('fs'), path = require('path'), cp = require('child_process');
const core = require('./engine_core.js');
const W = '/home/claude/swmm5-cplus/build/wasm/';
const mC = new WebAssembly.Module(fs.readFileSync(W + 'swmm5cplus.wasm'));
const mE = new WebAssembly.Module(fs.readFileSync(W + 'swmm524_epa.wasm'));
const CLOCK = /Analysis begun on|Analysis ended on|Total elapsed time/;
const norm = b => Buffer.from(b || []).toString('latin1').split('\n').filter(l => !CLOCK.test(l)).join('\n');
const list = fs.readFileSync(process.argv[2], 'utf8').split('\n').filter(Boolean);
let ok = 0, n = 0;
for (const rel of list) {
  const inp = path.resolve('/home/claude', rel);
  const dir = path.dirname(inp);
  // stage the .inp and every small sibling data file into the memory fs
  const files = { 'm.inp': fs.readFileSync(inp) };
  for (const f of fs.readdirSync(dir)) {
    if (/\.(dat|txt|csv|rff|hsf)$/i.test(f) && fs.statSync(path.join(dir, f)).size < 5e6) files[f] = fs.readFileSync(path.join(dir, f));
  }
  const tmp = fs.mkdtempSync('/tmp/claude-0/tc');
  cp.spawnSync('/home/claude/ref/runswmm', [inp, tmp + '/r.rpt', tmp + '/r.out'], { cwd: dir });
  const ro = fs.existsSync(tmp + '/r.out') ? fs.readFileSync(tmp + '/r.out') : Buffer.alloc(0);
  const rr = fs.readFileSync(tmp + '/r.rpt');
  const c = core.runLegacy(mC, 'm.inp', files), e = core.runLegacy(mE, 'm.inp', files);
  const sameC = Buffer.compare(Buffer.from(c.out || []), ro) === 0 && norm(c.rpt) === norm(rr);
  const sameE = Buffer.compare(Buffer.from(e.out || []), ro) === 0 && norm(e.rpt) === norm(rr);
  n++; if (sameC && sameE) ok++;
  else console.log('DIFF', rel, sameC, sameE, c.rc, e.rc, (c.out||[]).length, ro.length);
  fs.rmSync(tmp, { recursive: true });
}
console.log(`${ok}/${n} identical (cplus wasm, EPA wasm, native EPA) via engine_core.js`);
