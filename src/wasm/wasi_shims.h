// wasi_shims.h -- force-included in the wasm32-wasi build (see build_wasm.sh).
// wasi-libc declares mkstemp/realpath only under feature macros it does not
// implement; swmm5-cplus supplies minimal versions in wasm_shims.cpp.
#ifndef SWMM5CP_WASI_SHIMS_H
#define SWMM5CP_WASI_SHIMS_H
#ifdef __cplusplus
extern "C" {
#endif
int   mkstemp(char* tmpl);
char* realpath(const char* path, char* resolved);
#ifdef __cplusplus
}
#endif
#endif
