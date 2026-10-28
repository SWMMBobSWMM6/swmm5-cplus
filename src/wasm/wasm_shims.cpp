//-----------------------------------------------------------------------------
//   wasm_shims.cpp  --  swmm5-cplus WebAssembly (wasm32-wasi reactor) support
//
//   1. operator new/delete: the Debian wasm32 libc++abi is built without
//      exception support, yet its operator new still references __cxa_throw.
//      swmm5-cplus never throws; an allocation failure aborts the instance, which
//      the host sees as a trap and recovers from by discarding the instance.
//   2. thread_local cleanup hook: a single-threaded reactor never runs it.
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <new>
void* operator new(size_t n) { void* p = malloc(n ? n : 1); if (!p) abort(); return p; }
void* operator new[](size_t n) { return operator new(n); }
void* operator new(size_t n, const std::nothrow_t&) noexcept { return malloc(n ? n : 1); }
void* operator new[](size_t n, const std::nothrow_t&) noexcept { return malloc(n ? n : 1); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }
extern "C" int __cxa_thread_atexit(void (*)(void*), void*, void*) { return 0; }

//-----------------------------------------------------------------------------
//   3. mkstemp / realpath for the in-memory WASI file system.
//      mkstemp creates the file (so the name is reserved) and returns -1 for the
//      descriptor: SWMM only ever uses the name. realpath copies the path through;
//      the host hands the engine absolute paths.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
extern "C" int mkstemp(char* t)
{
    static unsigned long counter = 0;
    size_t n = strlen(t);
    if (n < 6) return -1;
    for (int tries = 0; tries < 1000000; tries++)
    {
        unsigned long k = ++counter;
        for (int i = 1; i <= 6; i++) { t[n - i] = (char)('0' + k % 10); k /= 10; }
        FILE* f = fopen(t, "r");
        if (f) { fclose(f); continue; }          // taken: try the next number
        f = fopen(t, "w");
        if (f) fclose(f);
        return -1;
    }
    return -1;
}
extern "C" char* realpath(const char* path, char* resolved)
{
    if (!path) return 0;
    if (!resolved) resolved = (char*)malloc(strlen(path) + 1);
    strcpy(resolved, path);
    return resolved;
}
