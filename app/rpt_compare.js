// rpt_compare.js -- compare two SWMM 5.2 reports number by number, and two time series
// statistic by statistic. Used by the app (inlined at build time) and by test_compare.js.
(function (root) {
  'use strict';
  const CLOCK = /Analysis begun on|Analysis ended on|Total elapsed time/;
  const NUM = /^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$/;
  const HHMM = /^\d{1,3}:\d\d$/;

  // value of a report token: plain numbers, "(1.05%)" style, "hh:mm" as minutes
  function tokenValue(t) {
    let s = t.replace(/^\(/, '').replace(/\)$/, '').replace(/%$/, '');
    if (NUM.test(s)) return parseFloat(s);
    if (HHMM.test(t)) { const [h, m] = t.split(':'); return 60 * +h + +m; }
    return null;
  }
  function tokens(line) {
    const out = []; const re = /\S+/g; let m;
    while ((m = re.exec(line))) out.push({ t: m[0], s: m.index, e: m.index + m[0].length, v: tokenValue(m[0]) });
    // "days hr:min" is one quantity in EPA's tables: merge "0  09:05" into one token, valued in minutes
    for (let i = 0; i + 1 < out.length; i++) {
      if (/^\d+$/.test(out[i].t) && HHMM.test(out[i + 1].t) && out[i + 1].s - out[i].e <= 4) {
        const [h, mm] = out[i + 1].t.split(':');
        out.splice(i, 2, { t: out[i].t + ' ' + out[i + 1].t, s: out[i].s, e: out[i + 1].e, v: 1440 * +out[i].t + 60 * +h + +mm, time: true });
      }
    }
    return out;
  }
  const isStars = l => /^\s*\*{5,}/.test(l);
  const isDash = l => /^\s*-{5,}\s*$/.test(l);

  // ---------------------------------------------------------------- parse one report
  function parseRpt(text) {
    const lines = text.split(/\r?\n/).filter(l => !CLOCK.test(l));
    const sections = []; let cur = { title: 'Header', units: [], lines: [] }; sections.push(cur);
    for (let i = 0; i < lines.length; i++) {
      const l = lines[i];
      if (isStars(l) && i + 2 < lines.length && isStars(lines[i + 2]) && !isStars(lines[i + 1])) {
        // a section banner: *** / Title  [units] / ***
        const t = lines[i + 1].replace(/^\s+/, '');
        const parts = t.split(/\s{2,}/);
        const top = l.replace(/^\s*\*+/, '').trim().split(/\s{2,}/).filter(Boolean);
        const units = parts.slice(1).map((u, k) => (top[k] ? top[k] + ' ' : '') + u);
        cur = { title: parts[0].trim(), units, lines: [] }; sections.push(cur); i += 2; continue;
      }
      cur.lines.push(l);
    }
    for (const s of sections) structure(s);
    return sections.filter(s => s.rows.length || s.title !== 'Header');
  }

  // rows: leader ("Label ...... v1 v2"), colon ("Label : v"), table data, or plain text
  function structure(s) {
    s.rows = []; s.cols = null;
    let inTable = 0, header = [], template = null;
    for (const l of s.lines) {
      if (!l.trim()) { if (inTable === 2) inTable = 0; continue; }
      if (isDash(l)) {
        if (inTable === 0) { inTable = 1; header = []; }
        else if (inTable === 1) { inTable = 2; template = null; }
        continue;                                    // a dash line inside data (before "System")
      }
      if (inTable === 1) { header.push(l); continue; }
      const lead = l.match(/^\s*(\S.*?)\s*\.{3,}\s*(.*)$/);
      if (inTable === 2) {
        const tk = tokens(l);
        if (tk.length) tk[0] = { ...tk[0], v: null };            // the element name is a name, even when it is "12"
        if (!template) { template = tk; s.cols = columnNames(header, template); s.header = header; }
        // align each token to the template column whose right edge is nearest
        const cells = new Array(template.length).fill(null);
        for (const t of tk) {
          let best = 0, bd = 1e9;
          template.forEach((c, j) => { const d = Math.abs(c.e - t.e); if (d < bd) { bd = d; best = j; } });
          if (cells[best] === null) cells[best] = t; else if (t.v === null) cells[best] = { ...cells[best], t: cells[best].t + ' ' + t.t };
        }
        s.rows.push({ kind: 'data', key: tk.length ? tk[0].t : '', cells, text: l });
        continue;
      }
      if (lead) {
        const vals = lead[2] ? tokens(lead[2]) : [];
        s.rows.push({ kind: 'leader', key: lead[1].trim(), cells: vals, text: l });
        continue;
      }
      const col = l.match(/^\s*(\S.*?)\s+:\s*(.*)$/);
      if (col) { s.rows.push({ kind: 'colon', key: col[1].replace(/\s+/g, ' ').trim(), cells: tokens(col[2]), text: l }); continue; }
      s.rows.push({ kind: 'text', key: l.trim(), cells: tokens(l), text: l });
    }
  }
  // header words belong to the first column whose right edge reaches them (EPA right-aligns)
  function columnNames(header, template) {
    const names = template.map(() => []);
    for (const h of header) {
      const re = /\S+/g; let m;
      while ((m = re.exec(h))) {
        const ws = m.index, we = m.index + m[0].length;
        // a word over a column's own characters belongs to it; otherwise to the first column
        // whose right edge reaches it (EPA right-aligns numeric headers)
        let j = -1, best = 0;
        template.forEach((c, k) => { const ov = Math.min(c.e, we) - Math.max(c.s, ws); if (ov > best) { best = ov; j = k; } });
        if (j < 0) j = template.findIndex(c => c.e >= we - 1);
        if (j < 0) j = template.length - 1;
        if (!/^-+$/.test(m[0])) names[j].push(m[0]);
      }
    }
    return names.map((n, j) => n.join(' ') || 'column ' + (j + 1));
  }

  // ---------------------------------------------------------------- compare two reports
  function compareRpt(textA, textB) {
    const la = textA.split(/\r?\n/).filter(l => !CLOCK.test(l)), lb = textB.split(/\r?\n/).filter(l => !CLOCK.test(l));
    let sameLines = 0; const n = Math.max(la.length, lb.length);
    for (let i = 0; i < Math.min(la.length, lb.length); i++) if (la[i] === lb[i]) sameLines++;
    const A = parseRpt(textA), B = parseRpt(textB);
    const out = { linesA: la.length, linesB: lb.length, sameLines, diffLines: n - sameLines, sections: [], items: [], columns: [], numbers: 0, sameNumbers: 0, maxRel: 0, maxRelWhere: '' };
    const seen = {};
    for (const sa of A) {
      const k = sa.title; seen[k] = (seen[k] || 0) + 1;
      const sb = B.filter(s => s.title === k)[seen[k] - 1];
      const sec = { title: k, rows: sa.rows.length, rowsB: sb ? sb.rows.length : 0, numbers: 0, same: 0, textDiff: 0, maxAbs: 0, maxRel: 0, kind: sa.cols ? 'table' : (sa.rows.some(r => r.kind === 'leader' || r.kind === 'colon') ? 'items' : 'text'), a: sa, b: sb };
      out.sections.push(sec);
      if (!sb) continue;
      const colStats = sa.cols ? sa.cols.map(name => ({ section: k, name, n: 0, same: 0, maxAbs: 0, maxA: -Infinity, maxB: -Infinity, sumA: 0, sumB: 0 })) : null;
      for (let r = 0; r < sa.rows.length; r++) {
        const ra = sa.rows[r], rb = sb.rows[r];
        if (!rb || ra.key !== rb.key) { sec.textDiff++; continue; }
        const m = Math.max(ra.cells.length, rb.cells.length);
        for (let c = 0; c < m; c++) {
          const ta = ra.cells[c], tb = rb.cells[c];
          if (!ta || !tb) { if (ta || tb) sec.textDiff++; continue; }
          if (ta.v === null || tb.v === null) { if (ta.t !== tb.t) sec.textDiff++; continue; }
          const d = Math.abs(ta.v - tb.v), rel = d > 0 ? d / Math.max(Math.abs(ta.v), Math.abs(tb.v)) : 0;   // symmetric, at most 1
          sec.numbers++; out.numbers++;
          if (ta.t === tb.t) { sec.same++; out.sameNumbers++; }
          if (d > sec.maxAbs) sec.maxAbs = d;
          if (rel > sec.maxRel && d > 0) sec.maxRel = rel;
          if (d > 0 && rel > out.maxRel) { out.maxRel = rel; out.maxRelWhere = k + ': ' + ra.key + ' (' + ta.t + ' vs ' + tb.t + ')'; }
          if (colStats && colStats[c]) {
            const cs = colStats[c]; cs.time = !!ta.time; cs.n++; if (ta.t === tb.t) cs.same++; cs.maxAbs = Math.max(cs.maxAbs, d);
            cs.maxA = Math.max(cs.maxA, ta.v); cs.maxB = Math.max(cs.maxB, tb.v); cs.sumA += ta.v; cs.sumB += tb.v;
          }
        }
        if (ra.kind === 'leader' || ra.kind === 'colon') {
          const units = sa.units && sa.units.length ? sa.units : [];
          ra.cells.forEach((ta, c) => {
            const tb = rb.cells[c];
            if (!ta || ta.v === null) return;
            // unit: "(%)" in the label, else a word printed after the number ("9.50 sec"), else the table's column unit
            const after = ra.cells[c + 1] && ra.cells[c + 1].v === null ? ra.cells[c + 1].t : '';
            const unit = /\(%\)/.test(ra.key) ? '%' : after || units[c] || '';
            out.items.push({ section: k, label: ra.key, unit, a: ta.t, b: tb ? tb.t : '', va: ta.v, vb: tb ? tb.v : null });
          });
        }
      }
      if (colStats) colStats.forEach(cs => { if (cs.n) out.columns.push(cs); });
    }
    return out;
  }

  // a few headline numbers a modeller looks at first, pulled from the parsed reports
  function headline(cmp) {
    const rows = [];
    const item = (sec, label, col, name) => {
      const it = cmp.items.filter(i => i.section === sec && i.label.startsWith(label));
      const x = it[col || 0]; if (x) rows.push({ name: name || `${label} (${x.unit || sec})`, a: x.a, b: x.b, va: x.va, vb: x.vb });
    };
    item('Runoff Quantity Continuity', 'Total Precipitation', 0, 'Total precipitation' + unitOf(cmp, 'Runoff Quantity Continuity', 0));
    item('Runoff Quantity Continuity', 'Surface Runoff', 0, 'Surface runoff' + unitOf(cmp, 'Runoff Quantity Continuity', 0));
    item('Runoff Quantity Continuity', 'Continuity Error', 0, 'Runoff continuity error (%)');
    item('Flow Routing Continuity', 'External Outflow', 0, 'Routing external outflow' + unitOf(cmp, 'Flow Routing Continuity', 0));
    item('Flow Routing Continuity', 'Flooding Loss', 0, 'Flooding loss' + unitOf(cmp, 'Flow Routing Continuity', 0));
    item('Flow Routing Continuity', 'Continuity Error', 0, 'Flow routing continuity error (%)');
    const col = (sec, re, name) => {
      const c = cmp.columns.find(c => c.section === sec && re.test(c.name));
      if (c) rows.push({ name: name + ' (' + c.name + ')', a: fmtN(c.maxA), b: fmtN(c.maxB), va: c.maxA, vb: c.maxB });
    };
    col('Link Flow Summary', /Maximum \|Flow\|/, 'Largest link flow');
    col('Node Depth Summary', /Maximum Depth/, 'Deepest node');
    col('Node Flooding Summary', /Total Flood Volume/, 'Largest node flood volume');
    col('Subcatchment Runoff Summary', /Peak Runoff/, 'Largest subcatchment peak');
    for (const [sec, name] of [['Node Flooding Summary', 'Nodes flooded'], ['Conduit Surcharge Summary', 'Conduits surcharged']]) {
      const s = cmp.sections.find(s => s.title === sec);
      if (s) { const ca = s.a.rows.filter(r => r.kind === 'data').length, cb = s.b ? s.b.rows.filter(r => r.kind === 'data').length : 0; rows.push({ name, a: String(ca), b: String(cb), va: ca, vb: cb }); }
    }
    item('Routing Time Step Summary', 'Minimum Time Step', 0, 'Minimum routing time step (sec)');
    item('Routing Time Step Summary', 'Average Time Step', 0, 'Average routing time step (sec)');
    item('Routing Time Step Summary', 'Average Iterations per Step', 0, 'Average iterations per step');
    item('Routing Time Step Summary', '% of Steps Not Converging', 0, 'Steps not converging (%)');
    return rows;
  }
  function unitOf(cmp, sec, c) { const it = cmp.items.find(i => i.section === sec && i.unit); return it ? ' (' + it.unit.replace(/^Volume |^Depth /, '') + ')' : ''; }
  function fmtN(v) { return isFinite(v) ? String(+v.toPrecision(6)) : ''; }

  // ---------------------------------------------------------------- compare two time series
  // ya is the reference. dtSec is the reporting step. volFactor converts sum(v*dt) to a volume unit.
  function seriesStats(ya, yb, dtSec, volFactor) {
    const n = Math.min(ya.length, yb.length);
    const one = y => {
      let pk = -Infinity, ipk = 0, mn = Infinity, sum = 0;
      for (let k = 0; k < n; k++) { const v = y[k]; if (v > pk) { pk = v; ipk = k; } if (v < mn) mn = v; sum += v; }
      return { peak: pk, peakT: (ipk + 1) * dtSec / 3600, min: mn, mean: sum / n, vol: volFactor ? sum * dtSec * volFactor : null };
    };
    const A = one(ya), B = one(yb);
    let sse = 0, sst = 0, sad = 0, bias = 0, maxAbs = 0, kMax = -1, same = 0, sab = 0, sbb = 0, saa = 0;
    for (let k = 0; k < n; k++) {
      const d = yb[k] - ya[k];
      if (Object.is(ya[k], yb[k]) || ya[k] === yb[k]) same++;
      sse += d * d; sad += Math.abs(d); bias += d;
      if (Math.abs(d) > maxAbs) { maxAbs = Math.abs(d); kMax = k; }
      const da = ya[k] - A.mean, db = yb[k] - B.mean; sst += da * da; saa += da * da; sbb += db * db; sab += da * db;
    }
    const r2 = (saa > 0 && sbb > 0) ? (sab * sab) / (saa * sbb) : null;
    return {
      n, dtSec, A, B, same, differ: n - same,
      maxAbs, maxAbsT: kMax >= 0 ? (kMax + 1) * dtSec / 3600 : null,
      mae: sad / n, rmse: Math.sqrt(sse / n), bias: bias / n,
      nse: sst > 0 ? 1 - sse / sst : null, r2,
      peakDiff: B.peak - A.peak, peakPct: A.peak !== 0 ? 100 * (B.peak - A.peak) / Math.abs(A.peak) : null,
      peakDtMin: 60 * (B.peakT - A.peakT),
      volPct: (A.vol !== null && A.vol !== 0) ? 100 * (B.vol - A.vol) / Math.abs(A.vol) : null,
    };
  }

  // volume of sum(flow * seconds) in the unit EPA's continuity tables use for that flow unit
  const VOL = [
    { f: 1 / 43560, u: 'acre-ft' },            // CFS: ft3 -> acre-ft
    { f: 1 / 60 / 1e6, u: '10^6 gal' },        // GPM
    { f: 1 / 86400, u: '10^6 gal' },           // MGD (Mgal/day * s / 86400 = Mgal)
    { f: 1 / 1e4, u: 'hectare-m' },            // CMS: m3 -> ha-m
    { f: 1 / 1e6, u: '10^6 ltr' },             // LPS
    { f: 1 / 86400, u: '10^6 ltr' },           // MLD
  ];

  // ---------------------------------------------------------------- .out reader and whole-file compare
  function parseOut(bytes) {
    if (!bytes || bytes.length < 60) return null;
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const I = o => dv.getInt32(o, true), F = o => dv.getFloat32(o, true), Dd = o => dv.getFloat64(o, true);
    const n = bytes.length;
    if (I(0) !== 516114522 || I(n - 4) !== 516114522) return null;
    const idPos = I(n - 24), inPos = I(n - 20), outPos = I(n - 16), nPer = I(n - 12);
    const ver = I(4), flowUnits = I(8), nSub = I(12), nNode = I(16), nLink = I(20), nPol = I(24);
    let p = idPos; const ids = { sub: [], node: [], link: [], pol: [] };
    const rd = () => { const l = I(p); p += 4; let s = ''; for (let i = 0; i < l; i++) s += String.fromCharCode(bytes[p + i]); p += l; return s; };
    for (let i = 0; i < nSub; i++) ids.sub.push(rd());
    for (let i = 0; i < nNode; i++) ids.node.push(rd());
    for (let i = 0; i < nLink; i++) ids.link.push(rd());
    for (let i = 0; i < nPol; i++) ids.pol.push(rd());
    p = inPos;
    const sv = I(p); p += 4 + 4 * sv + 4 * nSub * sv;
    const nv = I(p); p += 4 + 4 * nv + 4 * nNode * nv;
    const lv = I(p); p += 4 + 4 * lv + 4 * nLink * lv;
    const nSubV = I(p); p += 4 + 4 * nSubV;
    const nNodeV = I(p); p += 4 + 4 * nNodeV;
    const nLinkV = I(p); p += 4 + 4 * nLinkV;
    const nSysV = I(p); p += 4 + 4 * nSysV;
    const start = Dd(p); const step = I(p + 8);
    const bpp = 8 + 4 * (nSub * nSubV + nNode * nNodeV + nLink * nLinkV + nSysV);
    function series(kind, idx, v) {
      const off = kind === 'sub' ? 8 + 4 * (idx * nSubV + v)
                : kind === 'node' ? 8 + 4 * (nSub * nSubV + idx * nNodeV + v)
                : kind === 'sys' ? 8 + 4 * (nSub * nSubV + nNode * nNodeV + nLink * nLinkV + v)
                : 8 + 4 * (nSub * nSubV + nNode * nNodeV + idx * nLinkV + v);
      const a = new Float32Array(nPer);
      for (let k = 0; k < nPer; k++) a[k] = F(outPos + k * bpp + off);
      return a;
    }
    return { ver, flowUnits, nSub, nNode, nLink, nPol, ids, nPer, start, step, series,
             nSubV, nNodeV, nLinkV, nSysV, outPos, bpp, dv };
  }
  const VARS = {
    sub: ['rainfall', 'snow depth', 'evaporation loss', 'infiltration loss', 'runoff', 'groundwater flow', 'groundwater elevation', 'soil moisture'],
    node: ['depth', 'head', 'volume', 'lateral inflow', 'total inflow', 'flooding'],
    link: ['flow', 'depth', 'velocity', 'volume', 'capacity'],
    sys: ['air temperature', 'rainfall', 'snow depth', 'evaporation + infiltration', 'runoff', 'dry weather inflow', 'groundwater inflow',
          'RDII inflow', 'external inflow', 'total lateral inflow', 'flooding', 'outflow', 'storage volume', 'evaporation', 'potential ET'],
  };
  const varName = (kind, v, pa) => VARS[kind][v] || (pa.ids.pol[v - VARS[kind].length] ? pa.ids.pol[v - VARS[kind].length] + (kind === 'sub' ? ' washoff' : ' concentration') : 'variable ' + v);
  // every value of every element in every reporting period
  function compareOut(pa, pb) {
    if (!pa || !pb) return null;
    const same = ['nSub', 'nNode', 'nLink', 'nSubV', 'nNodeV', 'nLinkV', 'nSysV', 'nPer', 'step'].every(k => pa[k] === pb[k]);
    if (!same) return { comparable: false };
    const groups = { sub: [pa.nSub, pa.nSubV, 0], node: [pa.nNode, pa.nNodeV, pa.nSub * pa.nSubV], link: [pa.nLink, pa.nLinkV, pa.nSub * pa.nSubV + pa.nNode * pa.nNodeV],
                     sys: [1, pa.nSysV, pa.nSub * pa.nSubV + pa.nNode * pa.nNodeV + pa.nLink * pa.nLinkV] };
    const res = { comparable: true, nPer: pa.nPer, dates: 0, groups: {} };
    for (let k = 0; k < pa.nPer; k++) if (pa.dv.getFloat64(pa.outPos + k * pa.bpp, true) !== pb.dv.getFloat64(pb.outPos + k * pb.bpp, true)) res.dates++;
    for (const [g, [ne, nv, base]] of Object.entries(groups)) {
      const r = { elements: ne, vars: nv, values: ne * nv * pa.nPer, differ: 0, maxAbs: 0, where: '' };
      for (let k = 0; k < pa.nPer; k++) {
        const oa = pa.outPos + k * pa.bpp + 8 + 4 * base, ob = pb.outPos + k * pb.bpp + 8 + 4 * base;
        for (let j = 0; j < ne * nv; j++) {
          const x = pa.dv.getFloat32(oa + 4 * j, true), y = pb.dv.getFloat32(ob + 4 * j, true);
          if (x !== y && !(x !== x && y !== y)) {
            r.differ++; const d = Math.abs(x - y);
            if (d > r.maxAbs || r.maxAbs !== r.maxAbs) {
              r.maxAbs = d; const e = Math.floor(j / nv), v = j % nv;
              r.where = (g === 'sys' ? 'system' : pa.ids[g][e]) + ' ' + varName(g, v, pa) + ', period ' + (k + 1);
            }
          }
        }
      }
      res.groups[g] = r;
    }
    res.values = Object.values(res.groups).reduce((a, r) => a + r.values, 0);
    res.differ = Object.values(res.groups).reduce((a, r) => a + r.differ, 0);
    return res;
  }

  const API = { parseRpt, compareRpt, headline, seriesStats, tokenValue, VOL, parseOut, compareOut, VARS };
  if (typeof module !== 'undefined' && module.exports) module.exports = API; else root.SWCMP = API;
})(typeof self !== 'undefined' ? self : this);
