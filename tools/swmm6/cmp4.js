// cmp4.js RUNS.json -> JSON on stdout
// RUNS.json: {"runs": {"E": {"out": ..., "rpt": ...}, ...}, "pairs": [["E","C"], ...], "move": [["E","N"], ...]}
// For every pair: are all reported values (every period's date and every float) the same?
// For every "move" pair that is not identical: how far run B moved from run A, per element,
// normalised to that element's own peak, above a magnitude floor (1 L/s flow, 1 mm depth),
// plus the continuity errors and step statistics the two reports print.
const fs = require('fs');
const path = require('path');
const C = require(process.env.SWCMP || path.join(__dirname, '../../app/rpt_compare.js'));
const spec = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const parsed = {};
for (const [k, r] of Object.entries(spec.runs)) {
  try { parsed[k] = fs.existsSync(r.out) && fs.statSync(r.out).size > 0 ? C.parseOut(new Uint8Array(fs.readFileSync(r.out))) : null; }
  catch (e) { parsed[k] = null; }
}
const rptText = {};
const rpt = k => (rptText[k] !== undefined ? rptText[k] : (rptText[k] = fs.existsSync(spec.runs[k].rpt) ? fs.readFileSync(spec.runs[k].rpt, 'latin1') : ''));

function shapeOf(p) { return p && [p.nSub, p.nNode, p.nLink, p.nSubV, p.nNodeV, p.nLinkV, p.nSysV, p.step].join(','); }
// Reporting periods are matched by their date stamps, to the nearest second. EPA 5.2.4 can drop the
// final period at the end time (a rounding of the report clock); 5.3 and SWMM6 write it. Only
// periods present in both files are compared, and the unmatched ones are counted, not hidden.
function align(pa, pb) {
  const sec = (p, k) => Math.round(p.dv.getFloat64(p.outPos + k * p.bpp, true) * 86400);
  const ib = new Map(); for (let k = 0; k < pb.nPer; k++) ib.set(sec(pb, k), k);
  const idx = []; for (let k = 0; k < pa.nPer; k++) { const j = ib.get(sec(pa, k)); if (j !== undefined) idx.push([k, j]); }
  return { idx, onlyA: pa.nPer - idx.length, onlyB: pb.nPer - idx.length };
}

function varLabel(p, g, v) {
  const base = C.VARS[g] || [];
  if (v < base.length) return base[v];
  const pol = p.ids.pol[v - base.length];
  return pol !== undefined ? 'pollutant ' + pol : 'variable ' + v;
}

function identical(a, b) {
  const pa = parsed[a], pb = parsed[b];
  if (!pa || !pb) return { status: 'missing', a: !!pa, b: !!pb };
  if (shapeOf(pa) !== shapeOf(pb)) return { status: 'not comparable', shapeA: shapeOf(pa), shapeB: shapeOf(pb) };
  const al = align(pa, pb);
  if (!al.idx.length) return { status: 'not comparable', reason: 'no common reporting periods' };
  const nval = (pa.bpp - 8) / 4, groups = [['sub', pa.nSub * pa.nSubV], ['node', pa.nNode * pa.nNodeV], ['link', pa.nLink * pa.nLinkV], ['sys', pa.nSysV]];
  const nv = { sub: pa.nSubV, node: pa.nNodeV, link: pa.nLinkV, sys: pa.nSysV };
  let differ = 0; const maxAbs = {}, byVar = {};
  for (const [ka, kb] of al.idx) {
    const oa = pa.outPos + ka * pa.bpp + 8, ob = pb.outPos + kb * pb.bpp + 8;
    let j = 0;
    for (const [g, n] of groups) {
      for (let i = 0; i < n; i++, j++) {
        const x = pa.dv.getFloat32(oa + 4 * j, true), y = pb.dv.getFloat32(ob + 4 * j, true);
        if (x !== y && !(x !== x && y !== y)) {
          differ++; const d = Math.abs(x - y); if (!maxAbs[g] || d > maxAbs[g]) maxAbs[g] = d;
          const key = g + ':' + varLabel(pa, g, i % nv[g]); byVar[key] = (byVar[key] || 0) + 1;
        }
      }
    }
  }
  return { status: differ === 0 ? 'identical values' : 'differs', values: nval * al.idx.length, differ, periods: al.idx.length,
           onlyA: al.onlyA, onlyB: al.onlyB, verA: pa.ver, verB: pb.ver, maxAbs, byVar };
}

function movement(a, b) {
  const pa = parsed[a], pb = parsed[b], res = {};
  if (!pa || !pb || shapeOf(pa) !== shapeOf(pb)) return null;
  const al = align(pa, pb);
  const A = (kind, i, v) => { const x = pa.series(kind, i, v); return Float32Array.from(al.idx, ([k]) => x[k]); };
  const B = (kind, i, v) => { const y = pb.series(kind, i, v); return Float32Array.from(al.idx, ([, k]) => y[k]); };
  const us = pa.flowUnits < 3, FLOOR_Q = us ? 0.0353 : 0.001, FLOOR_Y = us ? 0.00328 : 0.001;
  const sa = A('sys', 0, 11), sb = B('sys', 0, 11);
  const st = C.seriesStats(sa, sb, pa.step, (C.VOL[pa.flowUnits] || {}).f || null);
  res.outflow = { nse: st.nse, peakPct: st.peakPct, volPct: st.volPct, maxAbs: st.maxAbs };
  function peaks(kind, v, floor) {
    const n = kind === 'link' ? pa.nLink : pa.nNode, out = [];
    let worst = null;
    for (let i = 0; i < n; i++) {
      const x = A(kind, i, v), y = B(kind, i, v);
      let pk = 0, pkb = 0, md = 0, sum = 0;
      for (let k = 0; k < x.length; k++) { pk = Math.max(pk, Math.abs(x[k])); pkb = Math.max(pkb, Math.abs(y[k])); const d = Math.abs(x[k] - y[k]); md = Math.max(md, d); sum += d; }
      // floor and scale on the larger of the two peaks, so an element one engine leaves near zero
      // and the other moves well past the floor is counted, not skipped
      const pm = Math.max(pk, pkb);
      if (pm < floor) continue;
      const o = { peakPct: 100 * (pkb - pk) / pm, shapePct: 100 * md / pm, meanPct: 100 * sum / x.length / pm };
      out.push(o);
      if (!worst || o.shapePct > worst.shapePct) worst = { id: pa.ids[kind][i], ...o };
    }
    const abs = out.map(o => Math.abs(o.peakPct)).sort((p, q) => p - q), sh = out.map(o => o.shapePct).sort((p, q) => p - q);
    const q = (arr, f) => arr.length ? arr[Math.min(arr.length - 1, Math.floor(f * arr.length))] : null;
    return { n: out.length, peakMedian: q(abs, 0.5), peakP90: q(abs, 0.9), peakMax: abs.length ? abs[abs.length - 1] : null,
             shapeMedian: q(sh, 0.5), shapeP90: q(sh, 0.9), shapeMax: sh.length ? sh[sh.length - 1] : null,
             shapeOver1: sh.filter(s => s > 1).length, shapeOver10: sh.filter(s => s > 10).length, worst };
  }
  res.linkFlow = peaks('link', 0, FLOOR_Q);
  res.nodeDepth = peaks('node', 0, FLOOR_Y);
  return res;
}

function rptFacts(k) {
  const t = rpt(k);
  if (!t) return null;
  const err = (t.match(/^\s*ERROR\s+\d+.*$/m) || [null])[0];
  const ver = (t.match(/VERSION\s+([0-9][^\s]*)/i) || t.match(/SWMM\s+([0-9]+\.[0-9]+\.[0-9]+[^\s]*)/) || [null, null])[1];
  const num = re => { const m = t.match(re); return m ? parseFloat(m[1]) : null; };
  return { error: err && err.trim().slice(0, 120), version: ver,
           routingContinuity: num(/Flow Routing Continuity[\s\S]*?Continuity Error \(%\)\s*\.+\s*(-?[\d.]+)/),
           runoffContinuity: num(/Runoff Quantity Continuity[\s\S]*?Continuity Error \(%\)\s*\.+\s*(-?[\d.]+)/),
           avgStep: num(/Average Time Step\s*:\s*([\d.]+)/), avgIter: num(/Average Iterations per Step\s*:\s*([\d.]+)/),
           warnings: (t.match(/^\s*WARNING\s+\d+/gm) || []).length };
}

const out = { pairs: {}, move: {}, rpt: {} };
for (const k of Object.keys(spec.runs)) out.rpt[k] = rptFacts(k);
for (const [a, b] of spec.pairs) out.pairs[a + '|' + b] = identical(a, b);
for (const [a, b] of spec.move || []) {
  const id = out.pairs[a + '|' + b] || identical(a, b);
  if (id.status === 'differs') out.move[a + '|' + b] = movement(a, b);
}
console.log(JSON.stringify(out));
