//-----------------------------------------------------------------------------
//   api.cpp  --  swmm5-cplus public API
//
//   1. swmm5cp_*  : handle-based API, one Project per handle (swmm5cp.h)
//   2. swmm_*     : the unchanged EPA SWMM 5.2.4 API (swmm5.h), routed to one
//                   default Project per thread, so existing programs (runswmm,
//                   GUIs, bindings) link and behave exactly as before.
//-----------------------------------------------------------------------------
#include <atomic>
#include <memory>
#include "headers.h"
#include "swmm5.h"
#include "swmm5cp.h"

namespace {
    std::atomic<int> g_live{0};

    inline Project* P(swmm5cp_handle h) { return reinterpret_cast<Project*>(h); }

    // default project for the legacy, handle-less API: one per thread
    Project& legacy()
    {
        thread_local std::unique_ptr<Project> p(new Project());
        return *p;
    }
}

//=============================================================================
//  Handle-based API
//=============================================================================
extern "C" {

swmm5cp_handle swmm5cp_create(void)
{
    Project* p = new Project();
    ++g_live;
    return reinterpret_cast<swmm5cp_handle>(p);
}

void swmm5cp_destroy(swmm5cp_handle h)
{
    if ( !h ) return;
    delete P(h);             // ~Project closes an open project first
    --g_live;
}

int swmm5cp_count(void) { return g_live.load(); }

int swmm5cp_run(swmm5cp_handle h, const char* a, const char* b, const char* c)
    { return P(h)->swmm_run(a, b, c); }
int swmm5cp_open(swmm5cp_handle h, const char* a, const char* b, const char* c)
    { return P(h)->swmm_open(a, b, c); }
int swmm5cp_start(swmm5cp_handle h, int saveFlag)    { return P(h)->swmm_start(saveFlag); }
int swmm5cp_step(swmm5cp_handle h, double* t)         { return P(h)->swmm_step(t); }
int swmm5cp_stride(swmm5cp_handle h, int s, double* t){ return P(h)->swmm_stride(s, t); }
int swmm5cp_end(swmm5cp_handle h)                     { return P(h)->swmm_end(); }
int swmm5cp_report(swmm5cp_handle h)                  { return P(h)->swmm_report(); }
int swmm5cp_close(swmm5cp_handle h)                   { return P(h)->swmm_close(); }
int swmm5cp_getMassBalErr(swmm5cp_handle h, float* r, float* f, float* q)
    { return P(h)->swmm_getMassBalErr(r, f, q); }
int swmm5cp_getVersion(void)                          { return legacy().swmm_getVersion(); }
int swmm5cp_getError(swmm5cp_handle h, char* m, int n){ return P(h)->swmm_getError(m, n); }
int swmm5cp_getWarnings(swmm5cp_handle h)             { return P(h)->swmm_getWarnings(); }
int swmm5cp_getCount(swmm5cp_handle h, int t)         { return P(h)->swmm_getCount(t); }
void swmm5cp_getName(swmm5cp_handle h, int t, int i, char* n, int s)
    { P(h)->swmm_getName(t, i, n, s); }
int swmm5cp_getIndex(swmm5cp_handle h, int t, const char* n)
    { return P(h)->swmm_getIndex(t, n); }
double swmm5cp_getValue(swmm5cp_handle h, int p, int i)
    { return P(h)->swmm_getValue(p, i); }
void swmm5cp_setValue(swmm5cp_handle h, int p, int i, double v)
    { P(h)->swmm_setValue(p, i, v); }
double swmm5cp_getSavedValue(swmm5cp_handle h, int p, int i, int k)
    { return P(h)->swmm_getSavedValue(p, i, k); }

//=============================================================================
//  Legacy EPA SWMM 5.2.4 API (swmm5.h), unchanged signatures
//=============================================================================
int swmm_run(const char* a, const char* b, const char* c) { return legacy().swmm_run(a, b, c); }
int swmm_open(const char* a, const char* b, const char* c){ return legacy().swmm_open(a, b, c); }
int swmm_start(int s)                    { return legacy().swmm_start(s); }
int swmm_step(double* t)                 { return legacy().swmm_step(t); }
int swmm_stride(int s, double* t)        { return legacy().swmm_stride(s, t); }
int swmm_end(void)                       { return legacy().swmm_end(); }
int swmm_report(void)                    { return legacy().swmm_report(); }
int swmm_close(void)                     { return legacy().swmm_close(); }
int swmm_getMassBalErr(float* r, float* f, float* q) { return legacy().swmm_getMassBalErr(r, f, q); }
int swmm_getVersion(void)                { return legacy().swmm_getVersion(); }
int swmm_getError(char* m, int n)        { return legacy().swmm_getError(m, n); }
int swmm_getWarnings(void)               { return legacy().swmm_getWarnings(); }
int swmm_getCount(int t)                 { return legacy().swmm_getCount(t); }
void swmm_getName(int t, int i, char* n, int s) { legacy().swmm_getName(t, i, n, s); }
int swmm_getIndex(int t, const char* n)  { return legacy().swmm_getIndex(t, n); }
double swmm_getValue(int p, int i)       { return legacy().swmm_getValue(p, i); }
void swmm_setValue(int p, int i, double v) { legacy().swmm_setValue(p, i, v); }
double swmm_getSavedValue(int p, int i, int k) { return legacy().swmm_getSavedValue(p, i, k); }
void swmm_writeLine(const char* line)    { legacy().swmm_writeLine(line); }
void swmm_decodeDate(double d, int* y, int* m, int* dd, int* h, int* mi, int* s, int* w)
    { legacy().swmm_decodeDate(d, y, m, dd, h, mi, s, w); }

} // extern "C"
