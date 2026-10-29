// growth.js A.out B.out -> per reporting period, the largest |B-A| in node depth and link flow (JSON)
const fs = require('fs'), path = require('path');
const C = require(path.join(__dirname, '../../app/rpt_compare.js'));
const [fa, fb] = process.argv.slice(2);
const pa = C.parseOut(new Uint8Array(fs.readFileSync(fa))), pb = C.parseOut(new Uint8Array(fs.readFileSync(fb)));
const sec = (p, k) => Math.round(p.dv.getFloat64(p.outPos + k * p.bpp, true) * 86400);
const ib = new Map(); for (let k = 0; k < pb.nPer; k++) ib.set(sec(pb, k), k);
const t0 = Math.round(pa.start * 86400), rows = [];
for (let k = 0; k < pa.nPer; k++) {
  const j = ib.get(sec(pa, k)); if (j === undefined) continue;
  let dy = 0, dq = 0, ymax = 0, qmax = 0;
  for (let n = 0; n < pa.nNode; n++) { const o = 8 + 4 * (pa.nSub * pa.nSubV + n * pa.nNodeV);
    const x = pa.dv.getFloat32(pa.outPos + k * pa.bpp + o, true), y = pb.dv.getFloat32(pb.outPos + j * pb.bpp + o, true);
    dy = Math.max(dy, Math.abs(x - y)); ymax = Math.max(ymax, Math.abs(x)); }
  for (let l = 0; l < pa.nLink; l++) { const o = 8 + 4 * (pa.nSub * pa.nSubV + pa.nNode * pa.nNodeV + l * pa.nLinkV);
    const x = pa.dv.getFloat32(pa.outPos + k * pa.bpp + o, true), y = pb.dv.getFloat32(pb.outPos + j * pb.bpp + o, true);
    dq = Math.max(dq, Math.abs(x - y)); qmax = Math.max(qmax, Math.abs(x)); }
  rows.push([(sec(pa, k) - t0) / 60, dy, dq, ymax, qmax]);
}
console.log(JSON.stringify({ cols: ['minutes', 'max |d depth| ft', 'max |d flow| cfs', 'max depth', 'max flow'], rows }));
