// node test for rpt_compare.js: identical pairs must compare as identical (null control);
// a roughness change must show up (positive control); parsed numbers must match the report text.
const fs = require('fs'), path = require('path');
const C = require('./rpt_compare.js');
const D = process.argv[2] || '/tmp/claude-0/cmpt';
const rd = f => fs.readFileSync(path.join(D, f), 'latin1');
let fail = 0;
const check = (ok, msg) => { console.log((ok ? 'ok   ' : 'FAIL ') + msg); if (!ok) fail++; };

for (const n of ['exam79', 'controlExample', '10_Subs_Continuous', 'extran1_egg']) {
  if (!fs.existsSync(path.join(D, n + '.epa.rpt'))) continue;
  const r = C.compareRpt(rd(n + '.epa.rpt'), rd(n + '.cp.rpt'));
  check(r.diffLines === 0 && r.numbers > 0 && r.sameNumbers === r.numbers && r.maxRel === 0,
    `${n}: ${r.linesA} lines, ${r.numbers} numbers in ${r.sections.length} sections, ${r.sameNumbers} identical, largest relative difference ${r.maxRel}`);
  // every number in a leader row must be a number that appears on that line of the report
  const txt = rd(n + '.epa.rpt').split('\n');
  const bad = r.items.filter(i => i.va !== null && !txt.some(l => l.replace(/\s+/g, ' ').includes(i.label) && l.includes(i.a)));
  check(bad.length === 0, `${n}: ${r.items.length} leader/colon items traced back to report lines`);
  console.log('      sections:', r.sections.map(s => `${s.title} [${s.kind}, ${s.numbers}]`).join('; '));
  console.log('      headline:', C.headline(r).map(h => `${h.name}=${h.a}`).join('; '));
  const lf = r.columns.filter(c => c.section === 'Link Flow Summary' || c.section === 'Node Depth Summary').map(c => c.section.split(' ')[0] + ':' + c.name);
  if (lf.length) console.log('      Link Flow Summary columns:', lf.join(' | '));
}

// positive control: n 0.013 -> 0.016 in controlExample
const p = C.compareRpt(rd('controlExample.epa.rpt'), rd('ctl_n.rpt'));
check(p.diffLines > 0 && p.sameNumbers < p.numbers && p.maxRel > 0,
  `roughness change: ${p.diffLines} lines differ, ${p.numbers - p.sameNumbers} of ${p.numbers} numbers differ, largest relative ${(100 * p.maxRel).toFixed(1)}% at ${p.maxRelWhere}`);
  console.log('      columns that moved:', p.columns.filter(c => c.same < c.n).map(c => `${c.section} / ${c.name}: ${c.n - c.same} of ${c.n}, max |d| ${+c.maxAbs.toPrecision(4)}`).slice(0, 8).join('; '));
const hp = C.headline(p).filter(h => h.a !== h.b);
console.log('      headline rows that moved:', hp.map(h => `${h.name}: ${h.a} -> ${h.b}`).join('; '));

// series statistics against hand-computed values
const s = C.seriesStats([0, 1, 2, 3, 2, 1], [0, 1, 2, 3, 2, 1], 300, 1 / 43560);
check(s.maxAbs === 0 && s.rmse === 0 && s.nse === 1 && Math.abs(s.r2 - 1) < 1e-15 && s.same === 6, 'identical series: max 0, RMSE 0, NSE 1, R2 1');
const t = C.seriesStats([0, 1, 2, 3, 2, 1], [0, 1, 2, 4, 2, 1], 300, 1 / 43560);
// SSE = 1, mean a = 1.5, SST = 2.25+0.25+0.25+2.25+0.25+0.25 = 5.5, NSE = 1 - 1/5.5
check(Math.abs(t.nse - (1 - 1 / 5.5)) < 1e-12 && Math.abs(t.rmse - Math.sqrt(1 / 6)) < 1e-12 && t.maxAbs === 1 && t.maxAbsT === 4 * 300 / 3600 && t.peakDiff === 1 && Math.abs(t.volPct - 100 / 9) < 1e-12,
  `one changed value: NSE ${t.nse.toFixed(6)}, RMSE ${t.rmse.toFixed(6)}, peak +${t.peakDiff}, volume ${t.volPct.toFixed(3)}%`);
const k = C.seriesStats([1, 1, 1], [1, 1, 1], 60, null);
check(k.nse === null && k.r2 === null, 'constant reference: NSE and R2 reported as not defined');
// whole .out comparison: identical pair, then the roughness change
const rb = f => new Uint8Array(fs.readFileSync(path.join(D, f)));
const oa = C.parseOut(rb('controlExample.epa.out')), ob = C.parseOut(rb('controlExample.cp.out')), on = C.parseOut(rb('ctl_n.out'));
const w0 = C.compareOut(oa, ob), w1 = C.compareOut(oa, on);
check(w0.comparable && w0.values > 0 && w0.differ === 0, `.out identical pair: ${w0.values} values over ${w0.nPer} periods, ${w0.differ} differ`);
check(w1.comparable && w1.differ > 0, `.out roughness change: ${w1.differ} of ${w1.values} values differ; links max |d| ${w1.groups.link.maxAbs.toPrecision(4)} at ${w1.groups.link.where}`);
// spot check one value straight from the bytes: link 0 flow, period 1
const off = oa.outPos + 8 + 4 * (oa.nSub * oa.nSubV + oa.nNode * oa.nNodeV);
check(oa.series('link', 0, 0)[0] === oa.dv.getFloat32(off, true), 'series() reads the same bytes as a direct offset');
process.exit(fail ? 1 : 0);
