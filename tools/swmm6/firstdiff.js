// firstdiff.js A.out B.out [n] -> the first reporting periods where two runs differ, and which values
const fs = require('fs'), path = require('path');
const C = require(path.join(__dirname, '../../app/rpt_compare.js'));
const [fa, fb, nmax] = process.argv.slice(2);
const pa = C.parseOut(new Uint8Array(fs.readFileSync(fa))), pb = C.parseOut(new Uint8Array(fs.readFileSync(fb)));
const sec = (p, k) => Math.round(p.dv.getFloat64(p.outPos + k * p.bpp, true) * 86400);
const ib = new Map(); for (let k = 0; k < pb.nPer; k++) ib.set(sec(pb, k), k);
const groups = [['sub', pa.nSub, pa.nSubV], ['node', pa.nNode, pa.nNodeV], ['link', pa.nLink, pa.nLinkV], ['sys', 1, pa.nSysV]];
let shown = 0;
for (let k = 0; k < pa.nPer && shown < (+nmax || 3); k++) {
  const j = ib.get(sec(pa, k)); if (j === undefined) continue;
  const oa = pa.outPos + k * pa.bpp + 8, ob = pb.outPos + j * pb.bpp + 8; let idx = 0; const d = [];
  for (const [g, ne, nv] of groups) for (let e = 0; e < ne; e++) for (let v = 0; v < nv; v++, idx++) {
    const x = pa.dv.getFloat32(oa + 4 * idx, true), y = pb.dv.getFloat32(ob + 4 * idx, true);
    if (x !== y) d.push(`${g === 'sys' ? 'system' : pa.ids[g][e]} ${(C.VARS[g] || [])[v] || 'v' + v}: ${x} -> ${y}`);
  }
  if (d.length) { const t = new Date((pa.dv.getFloat64(pa.outPos + k * pa.bpp, true) - 25569) * 86400000).toISOString().slice(0, 19);
    console.log(`period ${k + 1} (${t}): ${d.length} values differ`); d.slice(0, 12).forEach(s => console.log('   ' + s)); shown++; }
}
