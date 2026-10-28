//-----------------------------------------------------------------------------
//   findroot.hpp  --  swmm5-cplus
//
//   Root finders used by SWMM (Numerical Recipes rtsafe and Ridder's method).
//
//   EPA SWMM 5.2.4 took the function as a C function pointer plus a void* for
//   its data. Here they are function templates over any callable, so callers
//   pass a lambda that captures whatever it needs (usually the Project) and the
//   compiler can inline the function evaluations. The void* parameter is kept
//   so that existing evaluation routines keep their signatures.
//
//   The arithmetic is EPA's, line for line.
//-----------------------------------------------------------------------------
#ifndef SWMM5CPLUS_FINDROOT_HPP
#define SWMM5CPLUS_FINDROOT_HPP

#include <math.h>

namespace swmm {

namespace findroot_detail {
    constexpr int kMaxIt = 60;          // EPA: #define MAXIT 60
    inline double signOf(double a, double b) { return (b) >= 0.0 ? fabs(a) : -fabs(a); }   // EPA: SIGN(a,b)
}

// Newton-Raphson safeguarded by bisection. func(x, &f, &df, p) returns the
// function value and its derivative. On entry *rts is the initial guess.
// Returns the number of function evaluations, or 0 if MAXIT was exceeded.
template <class F>
int findroot_Newton(double x1, double x2, double* rts, double xacc, F&& func, void* p)
{
    constexpr int MAXIT_ = findroot_detail::kMaxIt;
    int j, n = 0;
    double df, dx, dxold, f, x;
    double temp, xhi, xlo;

    // Initialize the "stepsize before last" and the last step.
    x = *rts;
    xlo = x1;
    xhi = x2;
    dxold = fabs(x2-x1);
    dx = dxold;
    func(x, &f, &df, p);
    n++;

    // Loop over allowed iterations.
    for (j=1; j<=MAXIT_; j++)
    {
        // Bisect if Newton out of range or not decreasing fast enough.
        if ( ( ( (x-xhi)*df-f)*((x-xlo)*df-f) >= 0.0
        || (fabs(2.0*f) > fabs(dxold*df) ) ) )
        {
            dxold = dx;
            dx = 0.5*(xhi-xlo);
            x = xlo + dx;
            if ( xlo == x ) break;
        }

        // Newton step acceptable. Take it.
        else
        {
            dxold = dx;
            dx = f/df;
            temp = x;
            x -= dx;
            if ( temp == x ) break;
        }

        // Convergence criterion.
        if ( fabs(dx) < xacc ) break;

        // Evaluate function. Maintain bracket on the root.
        func(x, &f, &df, p);
        n++;
        if ( f < 0.0 ) xlo = x;
        else           xhi = x;
    }
    *rts = x;
    if ( n <= MAXIT_) return n;
    else return 0;
}

// Ridder's method. func(x, p) returns the function value. Returns the root,
// or -1.e20 if [x1, x2] does not bracket one.
template <class F>
double findroot_Ridder(double x1, double x2, double xacc, F&& func, void* p)
{
    constexpr int MAXIT_ = findroot_detail::kMaxIt;
    int j;
    double ans, fhi, flo, fm, fnew, s, xhi, xlo, xm, xnew;

    flo = func(x1, p);
    fhi = func(x2, p);
    if ( flo == 0.0 ) return x1;
    if ( fhi == 0.0 ) return x2;
    ans = 0.5*(x1+x2);
    if ( (flo > 0.0 && fhi < 0.0) || (flo < 0.0 && fhi > 0.0) )
    {
        xlo = x1;
        xhi = x2;
        for (j=1; j<=MAXIT_; j++) {
            xm = 0.5*(xlo + xhi);
            fm = func(xm, p);
            s = sqrt( fm*fm - flo*fhi );
            if (s == 0.0) return ans;
            xnew = xm + (xm-xlo)*( (flo >= fhi ? 1.0 : -1.0)*fm/s );
            if ( fabs(xnew - ans) <= xacc ) break;
            ans = xnew;
            fnew = func(ans, p);
            if ( findroot_detail::signOf(fm, fnew) != fm)
            {
                xlo = xm;
                flo = fm;
                xhi = ans;
                fhi = fnew;
            }
            else if ( findroot_detail::signOf(flo, fnew) != flo )
            {
                xhi = ans;
                fhi = fnew;
            }
            else if ( findroot_detail::signOf(fhi, fnew) != fhi)
            {
                xlo = ans;
                flo = fnew;
            }
            else return ans;
            if ( fabs(xhi - xlo) <= xacc ) return ans;
        }
        return ans;
    }
    return -1.e20;
}

} // namespace swmm
#endif
