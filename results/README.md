# Results

Raw rows behind every number in the app and essay. Paths in the rows are relative to the
corpus (SWMMBobSWMM6/1729-SWMM5-Models-2030 at 9ee63bd).

- `full.jsonl`: native corpus gate, EPA 5.2.4 (`ref/runswmm`, GCC 13.3 -O2) vs swmm5-cplus be57fec
  (GCC 13.3 -O2 -flto; hashes in `binaries.sha256`). Run in `full_by_size.txt` order after a first
  partial pass; stopped as recorded in `full.stopped`.
- `large_sample.jsonl`: seeded (20261028, 20261029) size-stratified sample of 72 of the 853 models
  the in-order gate did not reach; list in `large_sample.txt`.
- `wasm.jsonl`: both WebAssembly engines plus native EPA on the smoke and coverage sets (`wasm_list.txt`).
- `smoke*.jsonl`, `cov_r*.jsonl`, `stage1.jsonl`: the gate at each refactor step.
- `bench_*.json`: interleaved timing, A (EPA), B (C++), A' (EPA again) per rep.
- `coupling_matrix.json`: libclang count of module references to each globals.h variable.
- `evidence/`: AddressSanitizer two-open and one-open runs, and the six-model round-robin and
  threaded runs against solo EPA runs.
- `features.json.gz`: per-model feature census used to choose the coverage set.
