//-----------------------------------------------------------------------------
//   kinwave.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     06/12/23  (Build 5.2.4)
//   Author:   L. Rossman
//             M. Tryby (EPA)
//
//   Kinematic wave flow routing functions.
//
//   Update History
//   ==============
//   Build 5.1.008:
//   - Conduit inflow passed to function that computes conduit losses.
//   Build 5.1.014:
//   - Arguments to function link_getLossRate changed.
//   Build 5.2.4:
//   - Arguments to function link_getLossRate changed again.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <math.h>
#include "headers.h"
#include "findroot.h"

//-----------------------------------------------------------------------------
//  Constants 
//-----------------------------------------------------------------------------
static const double WX      = 0.6;     // distance weighting
static const double WT      = 0.6;     // time weighting
static const double EPSIL   = 0.001;   // convergence criterion

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------






// ---- kinwave module state: formerly file-scope variables of kinwave.c.
// ---- One instance per Project, so two models never share it.
struct KinwaveState
{
    double Beta1{};   // was static
    double C1{};   // was static
    double C2{};   // was static
    double Afull{};   // was static
    double Qfull{};   // was static
    TXsect * pXsect{};   // was static
};
void Project::kinwave_createState() { kinwave_ = new KinwaveState(); }
void Project::kinwave_destroyState() { delete kinwave_; kinwave_ = nullptr; }


//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  kinwave_execute  (called by flowrout_execute)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------



//=============================================================================

int Project::kinwave_execute(int j, double* qinflow, double* qoutflow, double tStep)
//
//  Input:   j = link index
//           qinflow = inflow at current time (cfs)
//           tStep = time step (sec)
//  Output:  qoutflow = outflow at current time (cfs),
//           returns number of iterations used
//  Purpose: finds outflow over time step tStep given flow entering a
//           conduit using Kinematic Wave flow routing.
//
//
//                               ^ q3 
//  t                            |   
//  |          qin, ain |-------------------| qout, aout
//  |                   |  Flow --->        |
//  |----> x     q1, a1 |-------------------| q2, a2
//
//
{
    int    k;
    int    result = 1;
    double dxdt, dq;
    double ain, aout;
    double qin, qout;
    double a1, a2, q1, q2, q3;

    // --- no routing for non-conduit link
    (*qoutflow) = (*qinflow); 
    if ( Link[j].type != CONDUIT ) return result;

    // --- no routing for dummy xsection
    if ( Link[j].xsect.type == DUMMY ) return result;

    // --- assign module-level variables
    kinwave_->pXsect = &Link[j].xsect;
    kinwave_->Qfull = Link[j].qFull;
    kinwave_->Afull = Link[j].xsect.aFull;
    k = Link[j].subIndex;
    kinwave_->Beta1 = Conduit[k].beta / kinwave_->Qfull;
 
    // --- normalize previous flows
    q1 = Conduit[k].q1 / kinwave_->Qfull;
    q2 = Conduit[k].q2 / kinwave_->Qfull;

    // --- normalize inflow
    qin = (*qinflow) / Conduit[k].barrels / kinwave_->Qfull;

    // --- compute evaporation and infiltration loss rate
    q3 = link_getLossRate(j, KW, qin*kinwave_->Qfull, tStep) / kinwave_->Qfull;

    // --- normalize previous areas
    a1 = Conduit[k].a1 / kinwave_->Afull;
    a2 = Conduit[k].a2 / kinwave_->Afull;

    // --- use full area when inlet flow >= full flow
    if ( qin >= 1.0 ) ain = 1.0;

    // --- get normalized inlet area corresponding to inlet flow
    else ain = xsect_getAofS(kinwave_->pXsect, qin/kinwave_->Beta1) / kinwave_->Afull;

    // --- check for no flow
    if ( qin <= TINY && q2 <= TINY )
    {
        qout = 0.0;
        aout = 0.0;
    }

    // --- otherwise solve finite difference form of continuity eqn.
    else
    {
        // --- compute constant factors
        dxdt = link_getLength(j) / tStep * kinwave_->Afull / kinwave_->Qfull;
        dq   = q2 - q1;
        kinwave_->C1   = dxdt * WT / WX;
        kinwave_->C2   = (1.0 - WT) * (ain - a1);
        kinwave_->C2   = kinwave_->C2 - WT * a2;
        kinwave_->C2   = kinwave_->C2 * dxdt / WX;
        kinwave_->C2   = kinwave_->C2 + (1.0 - WX) / WX * dq - qin;
        kinwave_->C2   = kinwave_->C2 + q3 / WX;

        // --- starting guess for aout is value from previous time step
        aout = a2;

        // --- solve continuity equation for aout
        result = solveContinuity(qin, ain, &aout);

        // --- report error if continuity eqn. not solved
        if ( result == -1 )
        {
            report_writeErrorMsg(ERR_KINWAVE, Link[j].ID);
            return 1;
        }
        if ( result <= 0 ) result = 1;

        // --- compute normalized outlet flow from outlet area
        qout = kinwave_->Beta1 * xsect_getSofA(kinwave_->pXsect, aout*kinwave_->Afull);
        if ( qin > 1.0 ) qin = 1.0;
    }

    // --- save new flows and areas
    Conduit[k].q1 = qin * kinwave_->Qfull;
    Conduit[k].a1 = ain * kinwave_->Afull;
    Conduit[k].q2 = qout * kinwave_->Qfull;
    Conduit[k].a2 = aout * kinwave_->Afull;
    Conduit[k].fullState =
        link_getFullState(Conduit[k].a1, Conduit[k].a2, kinwave_->Afull);
    (*qinflow)  = Conduit[k].q1 * Conduit[k].barrels;
    (*qoutflow) = Conduit[k].q2 * Conduit[k].barrels;
    return result;
}

//=============================================================================

int Project::solveContinuity(double qin, double ain, double* aout)
//
//  Input:   qin = upstream normalized flow
//           ain = upstream normalized area
//           aout = downstream normalized area
//  Output:  new value for aout; returns an error code
//  Purpose: solves continuity equation f(a) = Beta1*S(a) + C1*a + C2 = 0
//           for 'a' using the Newton-Raphson root finder function.
//           Return code has the following meanings:
//           >= 0 number of function evaluations used
//           -1   Newton function failed
//           -2   flow always above max. flow
//           -3   flow always below zero
//
//     Note: pXsect (pointer to conduit's cross-section), and constants Beta1,
//           C1, and C2 are module-level shared variables assigned values
//           in kinwave_execute().
//
{
    int    n;                          // # evaluations or error code
    double aLo, aHi, aTmp;             // lower/upper bounds on a
    double fLo, fHi;                   // lower/upper bounds on f
    double tol = EPSIL;                // absolute convergence tol.

    // --- first determine bounds on 'a' so that f(a) passes through 0.

    // --- set upper bound to area at full flow
    aHi = 1.0;
    fHi = 1.0 + kinwave_->C1 + kinwave_->C2;

    // --- try setting lower bound to area where section factor is maximum
    aLo = xsect_getAmax(kinwave_->pXsect) / kinwave_->Afull;
    if ( aLo < aHi )
    {
        fLo = ( kinwave_->Beta1 * kinwave_->pXsect->sMax ) + (kinwave_->C1 * aLo) + kinwave_->C2;
    }
    else fLo = fHi;

    // --- if fLo and fHi have same sign then set lower bound to 0
    if ( fHi*fLo > 0.0 )
    {
        aHi = aLo;
        fHi = fLo;
        aLo = 0.0;
        fLo = kinwave_->C2;
    }

    // --- proceed with search for root if fLo and fHi have different signs
    if ( fHi*fLo <= 0.0 )
    {
        // --- start search at midpoint of lower/upper bounds
        //     if initial value outside of these bounds
		if ( *aout < aLo || *aout > aHi ) *aout = 0.5*(aLo + aHi);

        // --- if fLo > fHi then switch aLo and aHi
        if ( fLo > fHi )
        {
            aTmp = aLo;
            aLo  = aHi;
            aHi  = aTmp;
        }

        // --- call the Newton root finder method passing it the 
        //     evalContinuity function to evaluate the function
        //     and its derivatives
        n = findroot_Newton(aLo, aHi, aout, tol, &Project::evalContinuity, NULL);

        // --- check if root finder succeeded
        if ( n <= 0 ) n = -1;
    }

    // --- if lower/upper bound functions both negative then use full flow
    else if ( fLo < 0.0 )
    {
        if ( qin > 1.0 ) *aout = ain;
        else *aout = 1.0;
        n = -2;
    }

    // --- if lower/upper bound functions both positive then use no flow
    else if ( fLo > 0 )
    {
        *aout = 0.0;
        n = -3;
    }
    else n = -1;
    return n;
}

//=============================================================================

void Project::evalContinuity(double a, double* f, double* df, void* p)
//
//  Input:   a = outlet normalized area
//  Output:  f = value of continuity eqn.
//           df = derivative of continuity eqn.
//  Purpose: computes value of continuity equation (f) and its derivative (df)
//           w.r.t. normalized area for link with normalized outlet area 'a'.
//
{
    *f  = (kinwave_->Beta1 * xsect_getSofA(kinwave_->pXsect, a*kinwave_->Afull)) + (kinwave_->C1 * a) + kinwave_->C2;
    *df = (kinwave_->Beta1 * kinwave_->Afull * xsect_getdSdA(kinwave_->pXsect, a*kinwave_->Afull)) + kinwave_->C1;
}

//=============================================================================
