// engine_core.js -- run SWMM wasm reactors (EPA 5.2.4 C, or swmm5-cplus) in memory.
// Shared verbatim by the app's Web Worker and the node test harness.
//
// MemWasi: a 13-call WASI preview1 layer over an in-memory file table with
// geometric buffer growth (a 50 MB .out costs ~26 reallocations, not 13,000).
(function (root) {
  'use strict';
  const E = { SUCCESS: 0, BADF: 8, EXIST: 20, INVAL: 28, ISDIR: 31, NOENT: 44, NOSYS: 52 };

  function MemWasi(initial) {
    const FS = new Map();                     // path (no leading '/') -> {buf, len}
    if (initial) for (const [k, v] of Object.entries(initial)) {
      const b = v instanceof Uint8Array ? v : new TextEncoder().encode(String(v));
      FS.set(norm(k), { buf: b, len: b.length });
    }
    const fds = new Map();
    fds.set(0, { kind: 'stdio' }); fds.set(1, { kind: 'stdio' }); fds.set(2, { kind: 'stdio' });
    fds.set(3, { kind: 'dir' });
    let nextFd = 4, memory = null, stdout = '';
    const dec = new TextDecoder('latin1');
    function norm(p) { return String(p).replace(/^\/+/, '').replace(/^\.\//, ''); }
    function dv() { return new DataView(memory.buffer); }
    function u8() { return new Uint8Array(memory.buffer); }
    function str(ptr, len) { return dec.decode(u8().subarray(ptr, ptr + len)); }
    function ensure(f, need) {
      if (need <= f.buf.length) return;
      let cap = Math.max(need, f.buf.length * 2, 4096);
      const nb = new Uint8Array(cap); nb.set(f.buf.subarray(0, f.len)); f.buf = nb;
    }
    class ExitError extends Error { constructor(code) { super('exit ' + code); this.code = code; } }
    const imports = {
      fd_prestat_get(fd, buf) {
        if (fd !== 3) return E.BADF;
        const d = dv(); d.setUint8(buf, 0); d.setUint32(buf + 4, 1, true); return E.SUCCESS;
      },
      fd_prestat_dir_name(fd, path, len) {
        if (fd !== 3) return E.BADF; u8()[path] = 47; return E.SUCCESS;
      },
      fd_fdstat_get(fd, buf) {
        const h = fds.get(fd); if (!h) return E.BADF;
        const d = dv();
        d.setUint8(buf, h.kind === 'stdio' ? 2 : h.kind === 'dir' ? 3 : 4);
        d.setUint16(buf + 2, 0, true);
        d.setBigUint64(buf + 8, 0xFFFFFFFFFFFFFFFFn, true);
        d.setBigUint64(buf + 16, 0xFFFFFFFFFFFFFFFFn, true);
        return E.SUCCESS;
      },
      fd_fdstat_set_flags() { return E.SUCCESS; },
      path_open(dirfd, dirflags, pathPtr, pathLen, oflags, rb, ri, fdflags, fdOut) {
        const name = norm(str(pathPtr, pathLen));
        if (oflags & 2) return E.NOSYS;                      // O_DIRECTORY: not needed by SWMM
        let f = FS.get(name);
        if (f && (oflags & 4)) return E.EXIST;               // O_EXCL
        if (!f) {
          if (!(oflags & 1)) return E.NOENT;                 // no O_CREAT
          f = { buf: new Uint8Array(0), len: 0 }; FS.set(name, f);
        }
        if (oflags & 8) f.len = 0;                           // O_TRUNC
        const fd = nextFd++;
        fds.set(fd, { kind: 'file', name, file: f, pos: 0, append: !!(fdflags & 1) });
        dv().setUint32(fdOut, fd, true);
        return E.SUCCESS;
      },
      fd_close(fd) { if (fd > 3) fds.delete(fd); return E.SUCCESS; },
      fd_read(fd, iovs, n, nreadPtr) {
        const h = fds.get(fd); if (!h) return E.BADF;
        const d = dv(); let total = 0;
        if (h.kind === 'file') {
          const m = u8();
          for (let i = 0; i < n; i++) {
            const p = d.getUint32(iovs + 8 * i, true), l = d.getUint32(iovs + 8 * i + 4, true);
            const k = Math.max(0, Math.min(l, h.file.len - h.pos));
            m.set(h.file.buf.subarray(h.pos, h.pos + k), p);
            h.pos += k; total += k;
            if (k < l) break;
          }
        }
        d.setUint32(nreadPtr, total, true); return E.SUCCESS;
      },
      fd_write(fd, iovs, n, nwPtr) {
        const h = fds.get(fd); if (!h) return E.BADF;
        const d = dv(), m = u8(); let total = 0;
        for (let i = 0; i < n; i++) {
          const p = d.getUint32(iovs + 8 * i, true), l = d.getUint32(iovs + 8 * i + 4, true);
          if (h.kind === 'stdio') { if (stdout.length < 20000) stdout += dec.decode(m.subarray(p, p + l)); }
          else if (h.kind === 'file') {
            if (h.append) h.pos = h.file.len;
            ensure(h.file, h.pos + l);
            h.file.buf.set(m.subarray(p, p + l), h.pos);
            h.pos += l; if (h.pos > h.file.len) h.file.len = h.pos;
          }
          total += l;
        }
        d.setUint32(nwPtr, total, true); return E.SUCCESS;
      },
      fd_seek(fd, offset, whence, outPtr) {
        const h = fds.get(fd); if (!h || h.kind !== 'file') return E.BADF;
        const off = Number(offset);
        let np = whence === 0 ? off : whence === 1 ? h.pos + off : h.file.len + off;
        if (np < 0) return E.INVAL;
        h.pos = np; dv().setBigUint64(outPtr, BigInt(np), true); return E.SUCCESS;
      },
      path_unlink_file(dirfd, p, l) {
        const name = norm(str(p, l)); return FS.delete(name) ? E.SUCCESS : E.NOENT;
      },
      path_remove_directory() { return E.NOENT; },
      clock_time_get(id, prec, outPtr) {
        dv().setBigUint64(outPtr, BigInt(Math.round(Date.now())) * 1000000n, true); return E.SUCCESS;
      },
      proc_exit(code) { throw new ExitError(code); },
    };
    return {
      imports, FS,
      bind(mem) { memory = mem; },
      read(name) { const f = FS.get(norm(name)); return f ? f.buf.slice(0, f.len) : null; },
      write(name, data) {
        const b = data instanceof Uint8Array ? data : new TextEncoder().encode(String(data));
        FS.set(norm(name), { buf: b, len: b.length });
      },
      unlink(name) { FS.delete(norm(name)); },
      get stdout() { return stdout; },
    };
  }

  // one wasm instance with its own memory file system
  function Engine(module, files) {
    const fs = MemWasi(files);
    const inst = new WebAssembly.Instance(module, { wasi_snapshot_preview1: fs.imports });
    fs.bind(inst.exports.memory);
    if (inst.exports._initialize) inst.exports._initialize();
    const ex = inst.exports;
    const enc = new TextEncoder();
    function cstr(s) {
      const b = enc.encode(s + '\0'); const p = ex.malloc(b.length);
      new Uint8Array(ex.memory.buffer, p, b.length).set(b); return p;
    }
    function errText(h) {
      const p = ex.malloc(256);
      if (h === undefined) ex.swmm_getError(p, 255); else ex.swmm5cp_getError(h, p, 255);
      const m = new Uint8Array(ex.memory.buffer, p, 256);
      let s = ''; for (let i = 0; i < 256 && m[i]; i++) s += String.fromCharCode(m[i]);
      ex.free(p); return s.trim();
    }
    const dbl = ex.malloc(8);
    function f64() { return new Float64Array(ex.memory.buffer, dbl, 1)[0]; }
    return { ex, fs, cstr, errText, dblPtr: dbl, f64, isCplus: typeof ex.swmm5cp_create === 'function' };
  }

  // whole run through the legacy API (works on both engines)
  function runLegacy(module, inpName, files) {
    const eng = Engine(module, files);
    const t0 = (typeof performance !== 'undefined' ? performance : Date).now();
    let rc;
    try {
      rc = eng.ex.swmm_run(eng.cstr('/' + inpName), eng.cstr('/m.rpt'), eng.cstr('/m.out'));
    } catch (e) { rc = e && e.code !== undefined ? 'exit ' + e.code : 'trap: ' + e.message; }
    const ms = (typeof performance !== 'undefined' ? performance : Date).now() - t0;
    return { rc, ms, rpt: eng.fs.read('m.rpt'), out: eng.fs.read('m.out'),
             version: eng.ex.swmm_getVersion ? eng.ex.swmm_getVersion() : 0 };
  }



  // whole run through a swmm5-cplus handle (the new API)
  function runHandle(module, inpName, files) {
    const eng = Engine(module, files);
    const ex = eng.ex;
    const t0 = (typeof performance !== 'undefined' ? performance : Date).now();
    const h = ex.swmm5cp_create();
    let rc;
    try { rc = ex.swmm5cp_run(h, eng.cstr('/' + inpName), eng.cstr('/m.rpt'), eng.cstr('/m.out')); }
    catch (e) { rc = e && e.code !== undefined ? 'exit ' + e.code : 'trap: ' + e.message; }
    const ms = (typeof performance !== 'undefined' ? performance : Date).now() - t0;
    try { ex.swmm5cp_destroy(h); } catch (e) {}
    return { rc, ms, rpt: eng.fs.read('m.rpt'), out: eng.fs.read('m.out'), version: ex.swmm5cp_getVersion() };
  }

  // ---------------------------------------------------------------- multi-model
  // Open every model in ONE swmm5-cplus instance (one handle each) and step them
  // round-robin. onProgress(i, steps, elapsedDays) is called every `every` steps.
  function runMulti(module, models, onProgress, every) {
    const fsFiles = {};
    models.forEach((m, i) => { fsFiles['m' + i + '.inp'] = m.inp; Object.assign(fsFiles, m.extra || {}); });
    const eng = Engine(module, fsFiles);
    const ex = eng.ex, H = [];
    for (let i = 0; i < models.length; i++) {
      const h = ex.swmm5cp_create();
      let rc = ex.swmm5cp_open(h, eng.cstr('/m' + i + '.inp'), eng.cstr('/m' + i + '.rpt'), eng.cstr('/m' + i + '.out'));
      if (!rc) rc = ex.swmm5cp_start(h, 1);
      H.push({ h, rc, steps: 0, done: rc !== 0, t: 0 });
    }
    const live = ex.swmm5cp_count();
    every = every || 2000;
    let anyLeft = true, round = 0;
    while (anyLeft) {
      anyLeft = false;
      for (let i = 0; i < H.length; i++) {
        const x = H[i]; if (x.done) continue;
        const rc = ex.swmm5cp_step(x.h, eng.dblPtr);
        x.steps++; x.t = eng.f64();
        if (rc || x.t <= 0) { x.done = true; if (rc) x.rc = rc; }
        else anyLeft = true;
        if (onProgress && (x.steps % every === 0 || x.done)) onProgress(i, x.steps, x.t, x.done);
      }
      round++;
    }
    const res = H.map((x, i) => {
      ex.swmm5cp_end(x.h);
      const err = x.rc ? eng.errText(x.h) : '';
      ex.swmm5cp_close(x.h);
      ex.swmm5cp_destroy(x.h);
      return { rc: x.rc, steps: x.steps, err, rpt: eng.fs.read('m' + i + '.rpt'), out: eng.fs.read('m' + i + '.out') };
    });
    return { live, after: ex.swmm5cp_count(), rounds: round, res };
  }

  // ---------------------------------------------------------------- coupling
  // upstream outfall OA -> downstream node IN, lock-step, then the merged network
  function runCoupled(module, upInp, dnInp, mergedInp, opts) {
    opts = opts || {};
    const eng = Engine(module, { 'up.inp': upInp, 'dn.inp': dnInp, 'merged.inp': mergedInp });
    const ex = eng.ex, s = eng.cstr;
    const NODE = 2, NODE_INFLOW = 307, NODE_LATFLOW = 306;
    const up = ex.swmm5cp_create(), dn = ex.swmm5cp_create(), mg = ex.swmm5cp_create();
    let rc = ex.swmm5cp_open(up, s('/up.inp'), s('/up.rpt'), s('/up.out')) ||
             ex.swmm5cp_start(up, 1) ||
             ex.swmm5cp_open(dn, s('/dn.inp'), s('/dn.rpt'), s('/dn.out')) ||
             ex.swmm5cp_start(dn, 1);
    if (rc) return { rc, err: eng.errText(up) || eng.errText(dn) };
    const oa = ex.swmm5cp_getIndex(up, NODE, s('OA')), inn = ex.swmm5cp_getIndex(dn, NODE, s('IN'));
    const tp2 = ex.malloc(8);
    let steps = 0, maxSkew = 0;
    for (;;) {
      const ra = ex.swmm5cp_step(up, eng.dblPtr); const ta = eng.f64();
      const q = ex.swmm5cp_getValue(up, NODE_INFLOW, oa);
      ex.swmm5cp_setValue(dn, NODE_LATFLOW, inn, q);
      const rb = ex.swmm5cp_step(dn, tp2); const tb = new Float64Array(ex.memory.buffer, tp2, 1)[0];
      steps++; maxSkew = Math.max(maxSkew, Math.abs(ta - tb));
      if (ra || rb || ta <= 0 || tb <= 0) break;
    }
    ex.swmm5cp_end(up); ex.swmm5cp_end(dn);
    rc = ex.swmm5cp_open(mg, s('/merged.inp'), s('/merged.rpt'), s('/merged.out')) || ex.swmm5cp_start(mg, 1);
    if (!rc) { do { rc = ex.swmm5cp_step(mg, eng.dblPtr); } while (!rc && eng.f64() > 0); ex.swmm5cp_end(mg); }
    const obD = ex.swmm5cp_getIndex(dn, NODE, s('OB')), obM = ex.swmm5cp_getIndex(mg, NODE, s('OB'));
    const n = opts.periods || 144, coupled = [], merged = [], upq = [];
    for (let k = 1; k <= n; k++) {
      coupled.push(ex.swmm5cp_getSavedValue(dn, NODE_INFLOW, obD, k));
      merged.push(ex.swmm5cp_getSavedValue(mg, NODE_INFLOW, obM, k));
      upq.push(ex.swmm5cp_getSavedValue(up, NODE_INFLOW, oa, k));
    }
    const live = ex.swmm5cp_count();
    [up, dn, mg].forEach(h => { ex.swmm5cp_close(h); ex.swmm5cp_destroy(h); });
    return { rc: 0, steps, maxSkew, live, coupled, merged, upstream: upq };
  }

  // ---------------------------------------------------------------- EPA two-open experiment
  function epaTwoOpen(moduleEpa, aInp, bInp) {
    const eng = Engine(moduleEpa, { 'a.inp': aInp, 'b.inp': bInp });
    const ex = eng.ex, s = eng.cstr;
    const openA = ex.swmm_open(s('/a.inp'), s('/a.rpt'), s('/a.out'));
    const nodesA = ex.swmm_getCount(2);
    const openB = ex.swmm_open(s('/b.inp'), s('/b.rpt'), s('/b.out'));
    const nodesAfter = ex.swmm_getCount(2);
    ex.swmm_start(1);
    let steps = 0, rc = 0;
    do { rc = ex.swmm_step(eng.dblPtr); steps++; } while (!rc && eng.f64() > 0);
    ex.swmm_end(); ex.swmm_close();
    const sz = n => (eng.fs.read(n) || []).length;
    return { openA, nodesA, openB, nodesAfter, steps,
             aRpt: sz('a.rpt'), aOut: sz('a.out'), bRpt: sz('b.rpt'), bOut: sz('b.out') };
  }

  root.SwmmEngineCore = { MemWasi, Engine, runLegacy, runHandle, runMulti, runCoupled, epaTwoOpen };
  if (typeof module !== 'undefined' && module.exports) module.exports = root.SwmmEngineCore;
})(typeof self !== 'undefined' ? self : globalThis);
