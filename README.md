# swmm5-cplus

EPA SWMM 5.2.4 restructured as C++17 so that each simulation is an object. Nothing it
computes differs from EPA's engine. What changes is that a program can hold, step and
couple any number of models at once: in one process, on one thread each, or inside one
WebAssembly instance in a browser.

EPA 5.2.4 keeps 415 mutable variables at file or program scope, so a process can hold one
model. Open a second one over the first and `swmm_open()` returns 0, the first model's
report stays empty, and AddressSanitizer counts 361,322 bytes in 134 allocations that are
never freed. In swmm5-cplus those 415 variables are fields of `class Project`. Stage 1 made
all 967 engine functions its members; R2 and R3 moved 97 general-purpose ones (cross-section
geometry, the expression parser, the integrator, the root finders) back out as code that needs
no model, and 870 remain members.

## The parity contract

swmm5-cplus is judged against EPA SWMM 5.2.4 built from the
[USEPA v5.2.4 tag](https://github.com/USEPA/Stormwater-Management-Model/tree/v5.2.4):

- the `.out` file byte for byte, and
- the `.rpt` file byte for byte after removing the three wall-clock lines
  (`Analysis begun on`, `Analysis ended on`, `Total elapsed time`),
- on the same compiler family: GCC 13.3 natively (EPA at `-O2` with Ubuntu's default
  fortify, so the gate passes absolute paths, see S5CP-2; swmm5-cplus at `-O2 -flto`), and
  Clang 18 with wasi-libc for WebAssembly (EPA at `-O2`, swmm5-cplus at `-O2 -flto`).

A run that both engines end with the same signal (the crash decks from the OpenSWMM code
review) is counted as "both crash", not as identical.

A run that both engines abandon at the same time or file-size limit counts as
inconclusive, never as a match.

| Gate | Result |
|---|---|
| Smoke set, every commit | 40 / 40 identical |
| Coverage set (all 26 shapes, every feature found, 36 error decks) | 187 / 187 identical |
| Full corpus, native: every model under 125 KB of input, plus a seeded sample of 72 of the 853 larger (18 per size quartile, to 88 MB) | 3,743 identical, 0 different, 4 inconclusive (time cap), 12 crash decks crash both alike; 3,759 of 4,540 run |
| WebAssembly, swmm5-cplus vs EPA C (smoke + coverage sets) | 221 / 221 identical |
| Six models round-robin in one instance; six threads | each identical to its solo EPA run (native, ASan, wasm) |
| Run time, both with LTO, 8 models × 5 interleaved reps | 1.003 × EPA (noise floor ±4.6 %) |

The corpus is [SWMMBobSWMM6/1729-SWMM5-Models-2030](https://github.com/SWMMBobSWMM6/1729-SWMM5-Models-2030) (4,540 `.inp` files).
The other 781 large models have not been run; at the sample's average run time they need at
least 8 more hours on a two-core machine. Of the identical runs, 435 stop on an input error and
write no `.out`, so the report is the whole comparison for them. Raw rows are in `results/`.

## The history is the method

Each commit is one gated step on top of EPA's pristine source:

| Commit | Step |
|---|---|
| `34066dc` | EPA SWMM 5.2.4 solver, unmodified |
| `8e0fa39` | Stage 1. Mechanical lift by `tools/make_cplus.py`: class `Project`, per-module PIMPL state, handle API |
| `795fd37` | R1. Resource safety: idempotent close, bounded paths, no descriptor leak; `~Project` closes |
| `adbddee` | R2. Root finders as templates, `OdeSolver` object, one `MathExprParser` per formula |
| `c97a83a` | R3. Cross-section geometry as 26 shape classes (`tools/transpose_xsect.py`) |
| `be57fec` | R3b. Shape table at namespace scope; `swmm::Simulation`; two-model coupling example |

Stage 1 is a program, not a hand edit. `make_cplus.py` parses EPA's C with libclang and
makes 5,710 edits, each at a location the compiler reported: a reference is renamed only
where libclang says it refers to that declaration. Inside a macro argument (1,707 references
of all kinds) libclang reports the macro's name, and the generator finds the identifier in
the argument text by spelling. It works because C++ looks up an unqualified name in the same order C
does: locals, then (now) class members, then namespace scope. Function bodies keep their
text and their meaning.

## Defects found (register S5CP)

| ID | Finding | Status |
|---|---|---|
| S5CP-1 | `swmm_close()` leaves its `FILE*` pointers dangling; a second close is a double free | fixed in R1 |
| S5CP-2 | `getAbsolutePath()` can overrun `InpDir` (260 bytes); `realpath()` needs `PATH_MAX`; glibc fortify aborts EPA's `runswmm` on relative paths | fixed in R1 |
| S5CP-3 | `mkstemp()` descriptor discarded (one leak per scratch file); TEMPDIR ignored | descriptor fixed; TEMPDIR open |
| S5CP-4 | `swmm_open()` over an open project abandons it (361,322 B leak) | legacy API unchanged; handles avoid it |
| S5CP-5 | Two `TINY`s: 1.E-6 in consts.h, 1.0e-30 in odesolve.c | neutralized in R2 (`kTiny`) |
| S5CP-6 | `xsect.dat` defines 92 non-const variables (49 tables, 43 sizes) with external linkage | open; single definition since R3 |
| S5CP-7 | infil.h declares `HortInfil`, `GAInfil`, `CNInfil`; nothing defines them | noted |
| S5CP-8 | odesolve.c work arrays (`nmax, y, yscal, yerr, ytemp, dydx, ak`) are linker-visible globals | fixed in R2 |
| S5CP-9 | Twelve files reach other modules' variables through local `extern` declarations | open (made explicit) |
| S5CP-10 | report.c declares `extern char ErrString[81]`; error.c defines `char ErrString[256]` (found by link-time optimization's type check) | gone in C++: one member, one type |

## Speed (R4)

Measured on 8 corpus models chosen to span big dynamic-wave networks, continuous runoff, LID,
water quality and a short-conduit case; CPU time of the child process, three interleaved
repetitions, a repeat of the reference build as noise floor (±7–11 % per model). Raw rows are in
`results/perf/`, the scripts in `tools/perf/`.

| Build | CPU time vs EPA 5.2.4 (Ubuntu's default gcc -O2) | Output |
|---|---|---|
| EPA C, -O3 + LTO + PGO | 0.90 | byte-identical 8/8 |
| swmm5-cplus be57fec, -O2 -flto | 0.89–0.93 (two runs) | byte-identical 8/8 |
| swmm5-cplus R4 (outfall memo), -O3 -flto + PGO (`tools/build_pgo.sh`) | 0.79 (0.46 on the water-quality model) | byte-identical 8/8; gate: 217 of 221 smoke and coverage models identical, 4 crash decks crash both alike |
| EPA C with OpenMP, 2 threads | wall time slower on 6 of 8 models, up to 1.75× | .out identical |
| EPA C, -march=native (FMA) / -ffast-math | 0.96 / 0.91 | changed; the largest link-peak change was 6 % (FMA) and 11 % (fast-math) |

R4 adds one change to the engine: the outfall conduit's normal and critical depth are pure
functions of its flow, so a repeated flow reuses the last answer instead of re-running the
Newton solve. A sampling profile (gprof) put cross-section geometry plus the math library at
25–72 % of run time and `pow` alone at 10–39 % in four of the eight models; the outfall solve was
up to half the time of short-step models.



C, with a handle:

```c
swmm5cp_handle h = swmm5cp_create();
int rc = swmm5cp_run(h, "model.inp", "model.rpt", "model.out");
swmm5cp_destroy(h);
```

Every `swmm_` function has a `swmm5cp_` twin that takes the handle. EPA's handle-less
functions are still exported, each driving one default project per thread, so programs
that call `swmm_run()` link unchanged.

C++, header-only, closes and frees however it leaves scope:

```cpp
swmm::Simulation up, dn;
up.open("upstream.inp", "up.rpt", "up.out"); up.start();
dn.open("downstream.inp", "dn.rpt", "dn.out"); dn.start();
```

`examples/coupled.cpp` runs two models in lock step and hands the flow at one model's
outfall to the other's inlet node; `examples/models/merged.inp` is the same network as one
model, for the check.

## Building

Native (`tools/build_native.sh ENG API OUT [FLAGS]`):

```sh
tools/build_native.sh src/engine src/api build/native "-O2 -flto"
```

compiles `src/engine/*.cpp` and `src/api/api.cpp` with `g++ -std=c++17`, links EPA's own
`src/run/main.c` (from the EPA tag) as the `runswmm_cp` command line, and builds
`tests/multi_native`. Set `MAINC` to EPA's `main.c` if your EPA checkout is not at `../epa524`.

WebAssembly (Clang 18, wasi-libc, `wasm32-wasi` reactor):

```sh
./build_wasm.sh        # build/wasm/swmm5cplus.wasm
./build_wasm_epa.sh    # build/wasm/swmm524_epa.wasm, EPA's C for comparison
node tests/wasm_run.mjs single build/wasm/swmm5cplus.wasm model.inp outdir
```

## Re-running the checks

```sh
python3 tools/gate.py REF_EXE TEST_EXE CORPUS_DIR out.jsonl --list list.txt --timeout 300 --jobs 2
python3 tools/wasm_gate.py REF_EXE swmm5cplus.wasm swmm524_epa.wasm tests/wasm_run.mjs list.txt out.jsonl
python3 tools/bench.py EPA_EXE CPLUS_EXE list.txt out.json --reps 5
```

`gate.py` streams the hashes (a 1,000-year simulation's output never has to fit in memory),
caps each run at 3 GiB of address space and 2 GiB per file, deletes outputs as it goes,
and resumes from its own `.jsonl`. It runs each model with its own folder as the working
directory, so data files resolve; models that save files (hot starts, rainfall interface
files) write them there, and EPA leaves its `swmmXXXXXX` scratch files there when it crashes.
Run it on a copy of the corpus. `tools/census.py`, `hazards.py` and `localtypes.py` are
the libclang census that came before the lift; `features.py` is the corpus feature census
used to choose the coverage set.

## The app

`app/` builds `swmm5-cplus.html`, a single-file browser app that runs EPA 5.2.4 and
swmm5-cplus side by side in WebAssembly, opens many models in one instance, runs the
coupling, and carries the essay *Same Numbers, Different Bones* in its Doc tab.
`app/build.py` reads the gate and timing results from the paths at its top;
`app/doc/essay.py` is the single source of the essay, and `doc/prep_docx.py` plus
`doc/build_docx.js` render the same text and figures to Word.

## What this does not show

- It is not a new engine and not more accurate. Every number is EPA 5.2.4's, EPA's own
  numerical defects included.
- It fixes none of EPA's crashes: the crash decks in the corpus crash it the same way.
- The full gate has not run 781 of the 853 models over 125 KB of input.
- It is not faster. With link-time optimization it costs nothing measurable; at plain
  `-O2` it is about 6 % slower on the eight models timed. One machine, two cores.
- Parity is per platform. EPA's own C source gives different bits natively and in
  WebAssembly on 24 of 221 models; both WebAssembly engines agree on all of them.
- Thread safety was tested (six models on six threads, identical), not audited. OpenMP
  in dynamic wave is compiled out in every build tested.
- The coupling is one-way and a demonstration, not a validated method.
- `Project` is still one large class and the modules still share the big arrays.

## Licence and credit

EPA SWMM is a work of the U.S. Environmental Protection Agency and is in the public domain;
its engine is Lewis Rossman's design, and swmm5-cplus keeps every algorithm as he wrote it.
No licence has been chosen yet for the restructuring itself.

## AI use

swmm5-cplus, its tools, its app and this README were made with Claude, Anthropic's AI
model (Claude Opus 5.5), in one session on 27 and 28 October 2026, at the request of
Robert (Bob) Dickinson, who asked the question and supplied the model corpus.
Responsibility for publishing the work is his. Claude ran the libclang census, wrote the
two generators and every hand-written change after them, found and fixed the resource
defects, wrote the C and C++ APIs, the coupling example, the WebAssembly builds, the gate
and timing harnesses, the app and its essay. The checks are mechanical: byte comparison
against EPA 5.2.4 built from the tag, AddressSanitizer, interleaved timing with a noise
floor. No person has yet reviewed the generated C++ or the hand edits line by line, or
rerun the gate on another machine.
