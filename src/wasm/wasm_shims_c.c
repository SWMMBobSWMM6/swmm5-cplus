/* C twin of the mkstemp/realpath shims, for the unmodified EPA reference build */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int mkstemp(char* t)
{
    static unsigned long counter = 0;
    size_t n = strlen(t);
    if (n < 6) return -1;
    for (int tries = 0; tries < 1000000; tries++) {
        unsigned long k = ++counter;
        for (int i = 1; i <= 6; i++) { t[n - i] = (char)('0' + k % 10); k /= 10; }
        FILE* f = fopen(t, "r");
        if (f) { fclose(f); continue; }
        f = fopen(t, "w"); if (f) fclose(f);
        return -1;
    }
    return -1;
}
char* realpath(const char* path, char* resolved)
{
    if (!path) return 0;
    if (!resolved) resolved = (char*)malloc(strlen(path) + 1);
    strcpy(resolved, path);
    return resolved;
}
