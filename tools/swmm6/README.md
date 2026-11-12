# Comparing with SWMM6

How the numbers in the essay's SWMM6 sections and in `results/swmm6/` were made. SWMM6 is
OpenSWMM's engine (github.com/HydroCouple/openswmm.engine, branch `swmm6_rel`); it was read and
built at commit `c40580c` (28 October 2026), Release, with its optional GeoPackage, HDF5, 2D and
GPU modules off:

```sh
cmake -S openswmm.engine -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DOPENSWMM_WITH_GEOPACKAGE=OFF -DOPENSWMM_WITH_HDF5=OFF -DOPENSWMM_BUILD_2D=OFF \
  -DOPENSWMM_BUILD_GPU_PLUGIN=OFF -DOPENSWMM_BUILD_PYTHON=OFF -DOPENSWMM_INSTALL=OFF
ninja -C build openswmm openswmm-legacy
```

**Correction (12 November 2026).** That configure uses no preset, so at `c40580c` it leaves
floating-point contraction at GCC's default, and `src/engine/CMakeLists.txt` adds `-mavx2 -mfma`:
the new engine's library got 2,665 fused multiply-add instructions and the legacy engine's none.
The essay first described this build as matching the repository's `-ffp-contract=off`; it did not.
SWMM6's commit `7d7dcbc` (29 October) appends `-ffp-contract=off` when no preset sets it.

## The rerun at db4634a (12 November 2026)

Three builds, the same configure as above:

| Build | Commit | Extra flags | FMA instructions, new / legacy |
|---|---|---|---|
| A | `c40580c` | none (the first build) | 2,665 / 0 |
| B | `c40580c` | `-DCMAKE_C_FLAGS=-ffp-contract=off -DCMAKE_CXX_FLAGS=-ffp-contract=off` | 0 / 0 |
| C | `db4634a` | none (the commit adds `-ffp-contract=off` itself) | 0 / 0 |

A against B is the flag alone; B against C is the 94 commits alone. The bisection build adds
`-DOPENSWMM_ENABLE_LTO=OFF` to B's flags so each commit rebuilds incrementally; at `c40580c` it
wrote the same `.out` bytes as B on all 137 models it runs.

| Script | What it does |
|---|---|
| `cmp6u.py LIST OUT.jsonl --epa --cplus --legacyA --newA --legacyB --newB --legacyC --newC --prev OLD...` | per model: EPA, swmm5-cplus, C's two engines twice each, B's two engines; A only where B's hash differs from A's in the old gate; every pair compared with `cmp4.js`; outputs over 400 MB compared by hash |
| `views6u.py RESULTS OUTDIR PREV... [SKIP]` | B and C as `cmp4.py`-style rows for `summarize.py`, plus `deltas.json` (flag effect, code effect, class moves A to B to C) |
| `moves6u.py RESULTS OUT.json [--skip LIST]` | each model the code changed: distance to EPA 5.2.4 before and after, closer, further or about the same |
| `lngroups6u.py RESULTS OUT.json` | the models where C's two engines still differ by over 10 %, grouped (Type 1 pumps on junctions, code-review decks, other), with the storage-inlet control |
| `flagfeat6u.py RESULTS FEATURES OUT.json` | features of the models the flag moved by over 10 % against those it left alone |
| `bisect6.py WORKTREE BUILD OLD NEW MODELS OUT.jsonl` | builds both engines at commits between OLD and NEW (first parent, touching `src`, `include` or CMake), runs every model, and narrows each change of output to one commit |

Six models read files that other corpus models write (hot-start and interface files), and EPA
5.2.4's own output for them differed between the October and November runs. They are in
`results/swmm6/db4634a/file_dependent.txt` and are left out of comparisons across the two dates.

## Scripts of the first comparison (28 October 2026)

| Script | What it does |
|---|---|
| `census6.py SWMM6_ROOT EPA_SOLVER CPLUS_ROOT OUT.json` | line counts, `PARITY` notes, environment variables, `throw`s, OpenMP pragmas, API size, and the token distance of SWMM6's legacy engine from EPA 5.2.4 |
| `cmp4.py LIST OUT.jsonl --epa --cplus --legacy --new` | six runs per model (EPA, swmm5-cplus, SWMM6 legacy twice, SWMM6 new twice), then `cmp4.js` |
| `cmp4.js SPEC.json` | value-by-value comparison of `.out` files, reporting periods matched by time stamp; per-element movement for pairs that differ |
| `summarize.py FEATURES OUT.json RESULTS...` | controls, identity counts, difference bands, outflow statistics, refusals, time-outs |
| `pair.py LIST OUT.jsonl A=EXE B=EXE` | two engines on a list, for attribution tests |
| `firstdiff.js`, `growth.js` | first differing reporting period; largest difference per period |
| `buffer_identity.py LIST OUT ORIG PATCHED` | byte identity of a patched build against the original |
| `epa524_outfall_like_53.patch` | EPA 5.2.4 with the legacy engine's free/normal outfall depth rule, nothing else |
| `epa524_storage_conveyance_like_53.patch` | the legacy engine's storage-surcharge and slot conveyance-area changes, on top of the outfall patch |
| `swmm6_output_one_fwrite_per_period.patch` | test patch to SWMM6: gather a reporting period and write it with one `fwrite` |

Timing used `tools/perf/bench_variants.py` with the eight models in `tools/perf/models.txt`.
The instruction profile is `valgrind --tool=callgrind` on `Hydrology/10_Subs_Continuous.inp`,
grouped by function name into output, memory, geometry, routing, runoff and math library.

The trace of the first difference between SWMM6's two engines used SWMM6's own switches:
`SWMM_TRACE_RSTEP=file` (one hash of the hydraulic state per routing step),
`SWMM_TRACE_DUMP_STEP=N` (every link and node at step N) and `SWMM_TRACE_LINK=k` with
`SWMM_TRACE_LSTEP=N` (one link's momentum terms).
