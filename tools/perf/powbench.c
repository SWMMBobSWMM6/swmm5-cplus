// powbench.c: cost of the power functions SWMM5 calls in its inner loops, on this machine.
// Prints ns per call and how often each alternative differs from glibc pow in the last bits.
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#define N 2000000
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+1e-9*t.tv_nsec; }
int main(void){
    double *x = malloc(N*sizeof(double)), s = 0; volatile double sink;
    srand(1); for (int i=0;i<N;i++) x[i] = 0.01 + 10.0*rand()/(double)RAND_MAX;   // hydraulic radius, ft
    struct { const char* name; int k; } F[] = {{"pow(r, 1.33333)  [dwflow friction]",0},{"r*cbrt(r)       [exponent 4/3]",1},
        {"pow(r, 2./3.)    [section factor]",2},{"cbrt(r*r)",3},{"exp(1.33333*log(r))",4},{"sqrt(r) (reference cost)",5}};
    for (int f=0; f<6; f++){
        double t0 = now(); s = 0;
        for (int rep=0; rep<5; rep++) for (int i=0;i<N;i++){
            double r = x[i], y;
            switch (F[f].k){ case 0: y = pow(r,1.33333); break; case 1: y = r*cbrt(r); break; case 2: y = pow(r,2./3.); break;
                             case 3: y = cbrt(r*r); break; case 4: y = exp(1.33333*log(r)); break; default: y = sqrt(r); }
            s += y; }
        double t = (now()-t0)/(5.0*N)*1e9; sink = s;
        // agreement with the glibc pow it would replace
        long diff = 0; double maxrel = 0;
        for (int i=0;i<N;i++){ double r=x[i], a, b;
            if (F[f].k==1){ a=pow(r,1.33333); b=r*cbrt(r);} else if (F[f].k==3){ a=pow(r,2./3.); b=cbrt(r*r);} else if (F[f].k==4){ a=pow(r,1.33333); b=exp(1.33333*log(r)); } else { a=b=0; }
            if (a!=b){ diff++; double rel=fabs(a-b)/fabs(a); if (rel>maxrel) maxrel=rel; } }
        printf("%-36s %6.1f ns/call   differs from pow in %5.1f%% of cases, max rel %.2e\n", F[f].name, t, 100.0*diff/N, maxrel);
    }
    return 0;
}
