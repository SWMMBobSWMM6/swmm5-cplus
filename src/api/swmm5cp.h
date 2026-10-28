/*
 *  swmm5cp.h  --  public C API of swmm5-cplus
 *
 *  Same functions as EPA SWMM 5.2.4's swmm5.h, plus one extra argument: the
 *  project handle. Every handle is a complete, independent simulation, so a host
 *  program (a GUI, a calibration loop, a Python binding, a browser page running the
 *  WebAssembly build) can hold as many open models as it likes and step them in
 *  any order, from any thread, one handle per thread at a time.
 *
 *  The legacy swmm5.h API is still exported (see api.cpp) for drop-in use; it
 *  drives one default project per thread.
 */
#ifndef SWMM5CP_H
#define SWMM5CP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__) || defined(__wasm__)
  #define SWMM5CP_EXPORT __attribute__((visibility("default")))
#else
  #define SWMM5CP_EXPORT
#endif

typedef struct swmm5cp_project* swmm5cp_handle;

SWMM5CP_EXPORT swmm5cp_handle swmm5cp_create(void);
SWMM5CP_EXPORT void   swmm5cp_destroy(swmm5cp_handle h);
SWMM5CP_EXPORT int    swmm5cp_count(void);       /* live projects in this process */

SWMM5CP_EXPORT int    swmm5cp_run(swmm5cp_handle h, const char* inp, const char* rpt, const char* out);
SWMM5CP_EXPORT int    swmm5cp_open(swmm5cp_handle h, const char* inp, const char* rpt, const char* out);
SWMM5CP_EXPORT int    swmm5cp_start(swmm5cp_handle h, int saveFlag);
SWMM5CP_EXPORT int    swmm5cp_step(swmm5cp_handle h, double* elapsedTime);
SWMM5CP_EXPORT int    swmm5cp_stride(swmm5cp_handle h, int strideStep, double* elapsedTime);
SWMM5CP_EXPORT int    swmm5cp_end(swmm5cp_handle h);
SWMM5CP_EXPORT int    swmm5cp_report(swmm5cp_handle h);
SWMM5CP_EXPORT int    swmm5cp_close(swmm5cp_handle h);
SWMM5CP_EXPORT int    swmm5cp_getMassBalErr(swmm5cp_handle h, float* runoffErr, float* flowErr, float* qualErr);
SWMM5CP_EXPORT int    swmm5cp_getVersion(void);
SWMM5CP_EXPORT int    swmm5cp_getError(swmm5cp_handle h, char* errMsg, int msgLen);
SWMM5CP_EXPORT int    swmm5cp_getWarnings(swmm5cp_handle h);
SWMM5CP_EXPORT int    swmm5cp_getCount(swmm5cp_handle h, int objType);
SWMM5CP_EXPORT void   swmm5cp_getName(swmm5cp_handle h, int objType, int index, char* name, int size);
SWMM5CP_EXPORT int    swmm5cp_getIndex(swmm5cp_handle h, int objType, const char* name);
SWMM5CP_EXPORT double swmm5cp_getValue(swmm5cp_handle h, int property, int index);
SWMM5CP_EXPORT void   swmm5cp_setValue(swmm5cp_handle h, int property, int index, double value);
SWMM5CP_EXPORT double swmm5cp_getSavedValue(swmm5cp_handle h, int property, int index, int period);

#ifdef __cplusplus
}
#endif
#endif
