# SWMM6 comparison: results

SWMM6 = OpenSWMM `swmm6_rel` at commit `c40580c` (28 October 2026). Engines: E = EPA 5.2.4
(gcc -O2, Ubuntu defaults), C = swmm5-cplus R4 (`-O2 -flto`), L = SWMM6 legacy engine
(5.3.0-beta.3), N = SWMM6 new engine (6.0.0-alpha.4). Scripts in `tools/swmm6/`.

| File | What it holds |
|---|---|
| `static.json` | line counts, `PARITY` notes, environment variables, `throw`s, OpenMP pragmas, API size, record sizes, token distance of every legacy file from 5.2.4 |
| `gate_small.jsonl.gz` | one row per model of `list_small.txt` (3,522): six runs, every pair compared, movement for differing pairs |
| `gate_small_rerun_first310.jsonl.gz` | the first 310 models again, after the movement floor was changed to the larger of the two peaks; rows where a run timed out under load are left out and the first pass stands |
| `gate_large_sample.jsonl.gz` | the 40 models of `list_large_sample.txt` |
| `summary.json` | `summarize.py` over the three files above (later rows override earlier ones) |
| `legacy_vs_epa_differing.txt`, `attribution_*.jsonl.gz`, `attribution.json` | EPA 5.2.4 patched with the legacy outfall rule, then also its storage and conveyance changes, against the legacy engine |
| `trace/`, `trace_ex05*.json` | the first difference between SWMM6's two engines in `Training/Exercise0_5_H&H_Elements.inp`, from SWMM6's own trace switches, and its growth over the run |
| `bench_swmm6.json` | CPU timing, eight speed-study models, three interleaved repetitions |
| `bench_large.json` | the same on five networks of 345 to 8,683 links |
| `profile_10subs.json` | callgrind instruction counts on `Hydrology/10_Subs_Continuous.inp`, grouped by function name |
| `bench_buffer.json`, `buffer_identity.json` | SWMM6 with one `fwrite` per reporting period: timing, and byte identity on 48 models |

Pair statuses: `identical values` (every reporting period's date and every float equal, periods
matched by time stamp), `differs`, `missing` (one or both wrote no readable `.out`), `no periods`,
`not comparable` (different element sets). The three 1,000-year models with outputs of 0.7–1.6 GB
were too large for the value comparison; their hashes are in the rows.
