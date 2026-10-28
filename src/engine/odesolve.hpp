//-----------------------------------------------------------------------------
//   odesolve.hpp  --  swmm5-cplus
//
//   Fifth-order Runge-Kutta-Cash-Karp integrator with adaptive step size
//   (Numerical Recipes rkqs/rkck), as used by SWMM for subcatchment ponded depth
//   and groundwater states.
//
//   EPA SWMM 5.2.4 kept the work arrays in seven globals with external linkage
//   (nmax, y, yscal, yerr, ytemp, dydx, ak) and passed the derivative function
//   as a plain C function pointer, so the derivative routine had to read its
//   inputs from other globals. Here the solver is an object that owns its work
//   arrays (released by its destructor), and the derivative is any callable --
//   typically a lambda that captures the Project.
//
//   The arithmetic is EPA's, character for character.
//-----------------------------------------------------------------------------
#ifndef SWMM5CPLUS_ODESOLVE_HPP
#define SWMM5CPLUS_ODESOLVE_HPP

#include <math.h>
#include <stdlib.h>

namespace swmm {

class OdeSolver
{
public:
    OdeSolver() = default;
    ~OdeSolver() { close(); }
    OdeSolver(const OdeSolver&) = delete;
    OdeSolver& operator=(const OdeSolver&) = delete;

    // allocate work space for up to n equations; returns 1 on success
    int open(int n)
    {
        int n5 = n*5;
        nmax  = 0;
        y     = (double *) calloc(n, sizeof(double));
        yscal = (double *) calloc(n, sizeof(double));
        dydx  = (double *) calloc(n, sizeof(double));
        yerr  = (double *) calloc(n, sizeof(double));
        ytemp = (double *) calloc(n, sizeof(double));
        ak    = (double *) calloc(n5, sizeof(double));
        if ( !y || !yscal || !dydx || !yerr || !ytemp || !ak ) return 0;
        nmax = n;
        return 1;
    }

    void close()
    {
        free(y);     y = nullptr;
        free(yscal); yscal = nullptr;
        free(dydx);  dydx = nullptr;
        free(yerr);  yerr = nullptr;
        free(ytemp); ytemp = nullptr;
        free(ak);    ak = nullptr;
        nmax = 0;
    }

    // integrate ystart[0..n-1] from x1 to x2 with tolerance eps and first step h1;
    // derivs(x, y, dydx) computes the derivatives. Returns 0 on success.
    template <class Derivs>
    int integrate(double ystart[], int n, double x1, double x2,
                  double eps, double h1, Derivs&& derivs)
    {
        int    i, errcode, nstp;
        double hdid, hnext;
        double x = x1;
        double h = h1;
        if (nmax < n) return 1;
        for (i=0; i<n; i++) y[i] = ystart[i];

        for (nstp=1; nstp<=kMaxStp; nstp++)
        {
            derivs(x,y,dydx);
            for (i=0; i<n; i++)
                yscal[i] = fabs(y[i]) + fabs(dydx[i]*h) + kTiny;
            if ((x+h-x2)*(x+h-x1) > 0.0) h = x2 - x;
            errcode = rkqs(&x,n,h,eps,&hdid,&hnext,derivs);
            if (errcode) break;
            if ((x-x2)*(x2-x1) >= 0.0)
            {
                for (i=0; i<n; i++) ystart[i] = y[i];
                return 0;
            }
            if (fabs(hnext) <= 0.0) return 2;
            h = hnext;
        }
        return 3;
    }

private:
    static constexpr int    kMaxStp = 10000;
    static constexpr double kTiny   = 1.0e-30;
    static constexpr double kSafety = 0.9;
    static constexpr double kPGrow  = -0.2;
    static constexpr double kPShrnk = -0.25;
    static constexpr double kErrCon = 1.89e-4;    // = (5/kSafety)^(1/kPGrow)

    int     nmax  = 0;          // max. number of equations
    double* y     = nullptr;    // dependent variable
    double* yscal = nullptr;    // scaling factors
    double* yerr  = nullptr;    // integration errors
    double* ytemp = nullptr;    // temporary values of y
    double* dydx  = nullptr;    // derivatives of y
    double* ak    = nullptr;    // derivatives at intermediate points

    template <class Derivs>
    int rkqs(double* x, int n, double htry, double eps, double* hdid,
             double* hnext, Derivs& derivs)
    {
        int i;
        double err, errmax, h, htemp, xnew, xold = *x;

        // --- set initial stepsize
        h = htry;
        for (;;)
        {
            // --- take a Runge-Kutta-Cash-Karp step
            rkck(xold, n, h, derivs);

            // --- compute scaled maximum error
            errmax = 0.0;
            for (i=0; i<n; i++)
            {
                err = fabs(yerr[i]/yscal[i]);
                if (err > errmax) errmax = err;
            }
            errmax /= eps;

            // --- error too large; reduce stepsize & repeat
            if (errmax > 1.0)
            {
                htemp = kSafety*h*pow(errmax,kPShrnk);
                if (h >= 0)
                {
                    if (htemp > 0.1*h) h = htemp;
                    else h = 0.1*h;
                }
                else
                {
                    if (htemp < 0.1*h) h = htemp;
                    else h = 0.1*h;
                }
                xnew = xold + h;
                if (xnew == xold) return 2;
                continue;
            }

            // --- step succeeded; compute size of next step
            else
            {
                if (errmax > kErrCon) *hnext = kSafety*h*pow(errmax,kPGrow);
                else *hnext = 5.0*h;
                *x += (*hdid=h);
                for (i=0; i<n; i++) y[i] = ytemp[i];
                return 0;
            }
        }
    }

    template <class Derivs>
    void rkck(double x, int n, double h, Derivs& derivs)
    {
        double a2=0.2, a3=0.3, a4=0.6, a5=1.0, a6=0.875,
               b21=0.2, b31=3.0/40.0, b32=9.0/40.0, b41=0.3, b42= -0.9, b43=1.2,
               b51= -11.0/54.0, b52=2.5, b53= -70.0/27.0, b54=35.0/27.0,
               b61=1631.0/55296.0, b62=175.0/512.0, b63=575.0/13824.0,
               b64=44275.0/110592.0, b65=253.0/4096.0, c1=37.0/378.0,
               c3=250.0/621.0, c4=125.0/594.0, c6=512.0/1771.0,
               dc5= -277.0/14336.0;
        double dc1=c1-2825.0/27648.0, dc3=c3-18575.0/48384.0,
               dc4=c4-13525.0/55296.0, dc6=c6-0.25;
        int i;
        int n2 = n*2;
        int n3 = n*3;
        int n4 = n*4;
        double *ak2 = (ak);
        double *ak3 = ((ak)+(n));
        double *ak4 = ((ak)+(n2));
        double *ak5 = ((ak)+(n3));
        double *ak6 = ((ak)+(n4));

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + b21*h*dydx[i];
        derivs(x+a2*h,ytemp,ak2);

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + h*(b31*dydx[i]+b32*ak2[i]);
        derivs(x+a3*h,ytemp,ak3);

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + h*(b41*dydx[i]+b42*ak2[i] + b43*ak3[i]);
        derivs(x+a4*h,ytemp,ak4);

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + h*(b51*dydx[i]+b52*ak2[i] + b53*ak3[i] + b54*ak4[i]);
        derivs(x+a5*h,ytemp,ak5);

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + h*(b61*dydx[i]+b62*ak2[i] + b63*ak3[i] + b64*ak4[i]
                       + b65*ak5[i]);
        derivs(x+a6*h,ytemp,ak6);

        for (i=0; i<n; i++)
            ytemp[i] = y[i] + h*(c1*dydx[i] + c3*ak3[i] + c4*ak4[i] + c6*ak6[i]);

        for (i=0; i<n; i++)
            yerr[i] = h*(dc1*dydx[i] +dc3*ak3[i] + dc4*ak4[i] + dc5*ak5[i] + dc6*ak6[i]);
    }
};

} // namespace swmm
#endif
