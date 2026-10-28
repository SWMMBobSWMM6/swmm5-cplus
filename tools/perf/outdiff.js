// outdiff.js A.out B.out A.rpt B.rpt -> JSON: how far run B moved from run A (the reference).
// System outflow statistics, every link's peak flow and every node's peak depth, with a floor,
// and the two continuity errors from the reports.
const fs = require('fs');
const C = require(require('path').join(__dirname, '../../app/rpt_compare.js'));
const [fa, fb, ra, rb] = process.argv.slice(2);
const pa = C.parseOut(new Uint8Array(fs.readFileSync(fa))), pb = C.parseOut(new Uint8Array(fs.readFileSync(fb)));
const res = {};
if (!pa || !pb) { console.log(JSON.stringify({ error: 'no .out' })); process.exit(0); }
// resample B onto A's reporting times when the report steps differ (they do not here; check)
res.sameShape = pa.nPer === pb.nPer && pa.step === pb.step && pa.nLink === pb.nLink && pa.nNode === pb.nNode;
const us = pa.flowUnits < 3, FLOOR_Q = us ? 0.0353 : 0.001, FLOOR_Y = us ? 0.00328 : 0.001;  // 1 L/s, 1 mm
const sa = pa.series('sys', 0, 11), sb = pb.series('sys', 0, 11);
const st = C.seriesStats(sa, sb, pa.step, (C.VOL[pa.flowUnits] || {}).f || null);
res.outflow = { nse: st.nse, peakPct: st.peakPct, volPct: st.volPct, maxAbs: st.maxAbs, differ: st.differ, n: st.n };
function peaks(kind, v, floor) {
  const n = kind === 'link' ? pa.nLink : pa.nNode, out = [];
  for (let i = 0; i < n; i++) {
    const a = pa.series(kind, i, v), b = pb.series(kind, i, v);
    let pk = 0, pkb = 0, md = 0;
    for (let k = 0; k < a.length; k++) { pk = Math.max(pk, Math.abs(a[k])); pkb = Math.max(pkb, Math.abs(b[k])); md = Math.max(md, Math.abs(a[k] - b[k])); }
    if (pk < floor) continue;
    out.push({ peakPct: 100 * (pkb - pk) / pk, shapePct: 100 * md / pk });
  }
  const abs = out.map(o => Math.abs(o.peakPct)).sort((x, y) => x - y), sh = out.map(o => o.shapePct).sort((x, y) => x - y);
  const q = (arr, f) => arr.length ? arr[Math.min(arr.length - 1, Math.floor(f * arr.length))] : null;
  return { n: out.length, peakMedian: q(abs, 0.5), peakP90: q(abs, 0.9), peakMax: abs.length ? abs[abs.length - 1] : null,
           over1: abs.filter(x => x > 1).length, over5: abs.filter(x => x > 5).length,
           shapeMedian: q(sh, 0.5), shapeP90: q(sh, 0.9), shapeMax: sh.length ? sh[sh.length - 1] : null };
}
if (res.sameShape) { res.linkFlow = peaks('link', 0, FLOOR_Q); res.nodeDepth = peaks('node', 0, FLOOR_Y); }
const cmp = C.compareRpt(fs.readFileSync(ra, 'latin1'), fs.readFileSync(rb, 'latin1'));
const ce = (sec) => cmp.items.find(i => i.section === sec && i.label.startsWith('Continuity Error'));
const ro = ce('Flow Routing Continuity'), ru = ce('Runoff Quantity Continuity');
res.routingContinuity = ro ? [ro.va, ro.vb] : null;
res.runoffContinuity = ru ? [ru.va, ru.vb] : null;
const avg = cmp.items.find(i => i.section === 'Routing Time Step Summary' && i.label.startsWith('Average Time Step'));
const it = cmp.items.find(i => i.section === 'Routing Time Step Summary' && i.label.startsWith('Average Iterations'));
res.avgStep = avg ? [avg.va, avg.vb] : null; res.avgIter = it ? [it.va, it.vb] : null;
console.log(JSON.stringify(res));
