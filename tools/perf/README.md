# Speed study tools

How the numbers in `results/perf/` were made. Set `SWMM_CORPUS` to the corpus folder,
`EPA_SRC` to EPA's `src` folder (v5.2.4 tag), and run from a scratch folder.

| Script | What it measures |
|---|---|
| `build_epa.sh OUT "CFLAGS" ["LDFLAGS"]` | builds EPA 5.2.4's C (or `SRC=` a patched copy) |
| `build_variants.sh` | the compiler-flag variants (hardening off, -O3, LTO, -march=native, FMA, fast-math, OpenMP) |
| `gprof_all.sh`, `callgrind_all.sh` | function profiles of the models in `models.txt` |
| `bench_variants.py` | interleaved CPU-time ratios against a reference build, with a repeat of the reference as noise floor, and .out/.rpt identity |
| `movement.py`, `outdiff.js` | how far results move when a build changes the arithmetic |
| `options_exp.py` | speed against accuracy for one routing option changed at a time |
| `costmodel.py` | CPU time per link-iteration from the reports' step and iteration counts |
| `powbench.c` | cost of `pow` and its alternatives on this machine |

`e1_outfall_memo_epa.patch` is the outfall-depth memo applied to EPA's C (bit-identical);
`e2_warm_start_on_top_of_e1.patch` is the warm-started normal-depth solve (changes results).
`train.txt` is the profile-guided build's training set, disjoint from `models.txt`.
