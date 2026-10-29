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
