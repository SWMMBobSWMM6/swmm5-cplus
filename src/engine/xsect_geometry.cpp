//-----------------------------------------------------------------------------
//   xsect.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     10/17/22   (Build 5.2.2)
//   Author:   L. Rossman
//             M. Tryby (EPA)
//
//   Cross section geometry functions.
//
//   The primary functions are:
//      getAofY   -- returns area given depth
//      getWofY   -- returns top width given depth
//      getRofY   -- returns hyd. radius given depth
//      getYofA   -- returns flow depth given area
//      getRofA   -- returns hyd. radius given area
//      getSofA   -- returns section factor given area
//      getAofS   -- returns area given section factor
//      getdSdA   -- returns derivative of section factor w.r.t. area
//   where
//      Y = flow depth
//      A = flow area
//      R = hyd. radius
//      S = section factor = A*R^(2/3)
//
//   Update History
//   ==============
//   Build 5.1.012:
//   - Height at max. width for Modified Baskethandle shape corrected.
//   Build 5.1.013:
//   - Width at full height set to 0 for closed rectangular shape.
//   Build 5.2.0:
//   - Support added for Street cross sections.
//   Build 5.2.2:
//   - Feasibility check added to Mod. Baskethandle & Rect.-Round shapes.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <math.h>
#include "xsect_geometry.hpp"
#include "findroot.hpp"

#define  RECT_ALFMAX        0.97
#define  RECT_TRIANG_ALFMAX 0.98
#define  RECT_ROUND_ALFMAX  0.98

#include "xsect.dat"    // File containing geometry tables for rounded shapes

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
// Ratio of area at max. flow to full area
// (= 1.0 for open shapes, < 1.0 for closed shapes)
double  Amax[] = {
                     1.0,     //  DUMMY
                     0.9756,  //  CIRCULAR
                     0.9756,  //  FILLED_CIRCULAR
                     0.97,    //  RECT_CLOSED
                     1.0,     //  RECT_OPEN
                     1.0,     //  TRAPEZOIDAL
                     1.0,     //  TRIANGULAR
                     1.0,     //  PARABOLIC
                     1.0,     //  POWERFUNC
                     0.98,    //  RECT_TRIANG
                     0.98,    //  RECT_ROUND
                     0.96,    //  MOD_BASKET
                     0.96,    //  HORIZ_ELLIPSE
                     0.96,    //  VERT_ELLIPSE
                     0.92,    //  ARCH
                     0.96,    //  EGGSHAPED
                     0.96,    //  HORSESHOE
                     0.96,    //  GOTHIC
                     0.98,    //  CATENARY
                     0.98,    //  SEMIELLIPTICAL
                     0.96,    //  BASKETHANDLE
                     0.96,    //  SEMICIRCULAR
                     1.0,     //  IRREGULAR
                     0.96,    //  CUSTOM
                     0.9756,  //  FORCE_MAIN
                     1.0};    //  STREET_XSECT

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
typedef struct
{
    double  s;                // section factor
    double  qc;               // critical flow
    TXsect* xsect;            // pointer to a cross section object
} TXsectStar;

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  xsect_isOpen
//  xsect_setParams
//  xsect_setIrregXsectParams
//  xsect_setStreetXsectParams
//  xsect_setCustomXsectParams
//  xsect_getAmax
//  xsect_getSofA
//  xsect_getYofA
//  xsect_getRofA
//  xsect_getAofS
//  xsect_getdSdA
//  xsect_getAofY
//  xsect_getRofY
//  xsect_getWofY
//  xsect_getYcrit

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------























































































//=============================================================================


namespace swmm { namespace xs {


// ---- the base class: EPA's default branches
double XsectShape::getSofA(TXsect *xsect, double a) const
{
    double alpha = a / xsect->aFull;
    double r;
    
        if (a == 0.0) return 0.0;
        r = xsect_getRofA(xsect, a);
        if ( r < TINY ) return 0.0;
        return a * pow(r, 2./3.);
    }

double XsectShape::getYofA(TXsect *xsect, double a) const
{
    double alpha = a / xsect->aFull;
              return 0.0;
    }

double XsectShape::getAofY(TXsect *xsect, double y) const
{
    double yNorm = y / xsect->yFull;
    if ( y <= 0.0 ) return 0.0;
              return 0.0;
    }

double XsectShape::getWofY(TXsect *xsect, double y) const
{
    double yNorm = y / xsect->yFull;
              return 0.0;
    }

double XsectShape::getRofY(TXsect *xsect, double y) const
{
    double yNorm = y / xsect->yFull;
               return xsect_getRofA( xsect, xsect_getAofY(xsect, y) );
    }

double XsectShape::getRofA(TXsect *xsect, double a) const
{
    double cathy;
    if ( a <= 0.0 ) return 0.0;
    
        cathy = xsect_getSofA(xsect, a);
        if ( cathy < TINY || a < TINY ) return 0.0;
        return pow(cathy/a, 3./2.);
    }

double XsectShape::getAofS(TXsect* xsect, double s) const
{
    double psi = s / xsect->sFull;
    if ( s <= 0.0 ) return 0.0;
    if ( s > xsect->sMax ) s = xsect->sMax;
     return generic_getAofS(xsect, s);
    }

double XsectShape::getdSdA(TXsect* xsect, double a) const
{
     return generic_getdSdA(xsect, a);
    }

// ---- DUMMY
class Shape_DUMMY final : public XsectShape
{
public:
    const char* name() const override { return "DUMMY"; }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
             return 0.0;
    }
};

// ---- CIRCULAR
class Shape_CIRCULAR final : public XsectShape
{
public:
    const char* name() const override { return "CIRCULAR"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return circ_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
         return circ_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Circ, N_A_Circ);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Circ, N_W_Circ);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Circ, N_R_Circ);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
          return circ_getAofS(xsect, s);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return circ_getdSdA(xsect, a);
    }
};

// ---- FILLED_CIRCULAR
class Shape_FILLED_CIRCULAR final : public XsectShape
{
public:
    const char* name() const override { return "FILLED_CIRCULAR"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return filled_circ_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return filled_circ_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            yNorm = (y + xsect->yBot) / (xsect->yFull + xsect->yBot);
            return xsect->wMax * lookup(yNorm, W_Circ, N_W_Circ);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            if ( xsect->yBot == 0.0 )
                return xsect->rFull * lookup(yNorm, R_Circ, N_R_Circ);
            return filled_circ_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- RECT_CLOSED
class Shape_RECT_CLOSED final : public XsectShape
{
public:
    const char* name() const override { return "RECT_CLOSED"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return rect_closed_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
         return a / xsect->wMax;
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
          return y * xsect->wMax;
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
     
              if (yNorm == 1.0) return 0.0;
              return xsect->wMax;
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
          return rect_closed_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return rect_closed_getdSdA(xsect, a);
    }
};

// ---- RECT_OPEN
class Shape_RECT_OPEN final : public XsectShape
{
public:
    const char* name() const override { return "RECT_OPEN"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return rect_open_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
           return a / xsect->wMax;
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
           return y * xsect->wMax;
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
           return xsect->wMax;
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
            return a / (xsect->wMax +
                                 (2. - xsect->sBot) * a / xsect->wMax);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return rect_open_getdSdA(xsect, a);
    }
};

// ---- TRAPEZOIDAL
class Shape_TRAPEZOIDAL final : public XsectShape
{
public:
    const char* name() const override { return "TRAPEZOIDAL"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
         return trapez_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
         return trapez_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
         return trapez_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
          return trapez_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
          return trapez_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return trapez_getdSdA(xsect, a);
    }
};

// ---- TRIANGULAR
class Shape_TRIANGULAR final : public XsectShape
{
public:
    const char* name() const override { return "TRIANGULAR"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
          return triang_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
          return triang_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
          return triang_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
           return triang_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
           return triang_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return triang_getdSdA(xsect, a);
    }
};

// ---- PARABOLIC
class Shape_PARABOLIC final : public XsectShape
{
public:
    const char* name() const override { return "PARABOLIC"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
           return parab_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
           return parab_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
           return parab_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
            return parab_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
            return parab_getRofA(xsect, a);
    }
};

// ---- POWERFUNC
class Shape_POWERFUNC final : public XsectShape
{
public:
    const char* name() const override { return "POWERFUNC"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
           return powerfunc_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
           return powerfunc_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
           return powerfunc_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
            return powerfunc_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
            return powerfunc_getRofA(xsect, a);
    }
};

// ---- RECT_TRIANG
class Shape_RECT_TRIANG final : public XsectShape
{
public:
    const char* name() const override { return "RECT_TRIANG"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return rect_triang_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
         return rect_triang_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
         return rect_triang_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
         return rect_triang_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
          return rect_triang_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
          return rect_triang_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return rect_triang_getdSdA(xsect, a);
    }
};

// ---- RECT_ROUND
class Shape_RECT_ROUND final : public XsectShape
{
public:
    const char* name() const override { return "RECT_ROUND"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return rect_round_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
          return rect_round_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
          return rect_round_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
          return rect_round_getWofY(xsect, y);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
           return rect_round_getRofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
           return rect_round_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return rect_round_getdSdA(xsect, a);
    }
};

// ---- MOD_BASKET
class Shape_MOD_BASKET final : public XsectShape
{
public:
    const char* name() const override { return "MOD_BASKET"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
          return mod_basket_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
          return mod_basket_getAofY(xsect, y);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
          return mod_basket_getWofY(xsect, y);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
           return mod_basket_getRofA(xsect, a);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return mod_basket_getdSdA(xsect, a);
    }
};

// ---- HORIZ_ELLIPSE
class Shape_HORIZ_ELLIPSE final : public XsectShape
{
public:
    const char* name() const override { return "HORIZ_ELLIPSE"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha, A_HorizEllipse, N_A_HorizEllipse);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_HorizEllipse, N_A_HorizEllipse);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_HorizEllipse, N_W_HorizEllipse);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_HorizEllipse, N_R_HorizEllipse);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- VERT_ELLIPSE
class Shape_VERT_ELLIPSE final : public XsectShape
{
public:
    const char* name() const override { return "VERT_ELLIPSE"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha, A_VertEllipse, N_A_VertEllipse);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_VertEllipse, N_A_VertEllipse);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_VertEllipse, N_W_VertEllipse);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_VertEllipse, N_R_VertEllipse);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- ARCH
class Shape_ARCH final : public XsectShape
{
public:
    const char* name() const override { return "ARCH"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha, A_Arch, N_A_Arch);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Arch, N_A_Arch);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Arch, N_W_Arch);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Arch, N_R_Arch);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- EGGSHAPED
class Shape_EGGSHAPED final : public XsectShape
{
public:
    const char* name() const override { return "EGGSHAPED"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_Egg, N_S_Egg);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_Egg, N_Y_Egg);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Egg, N_A_Egg);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Egg, N_W_Egg);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Egg, N_R_Egg);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_Egg, N_S_Egg);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return tabular_getdSdA(xsect, a, S_Egg, N_S_Egg);
    }
};

// ---- HORSESHOE
class Shape_HORSESHOE final : public XsectShape
{
public:
    const char* name() const override { return "HORSESHOE"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_Horseshoe, N_S_Horseshoe);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_Horseshoe, N_Y_Horseshoe);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Horseshoe, N_A_Horseshoe);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Horseshoe, N_W_Horseshoe);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Horseshoe, N_R_Horseshoe);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_Horseshoe, N_S_Horseshoe);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return tabular_getdSdA(xsect, a, S_Horseshoe, N_S_Horseshoe);
    }
};

// ---- GOTHIC
class Shape_GOTHIC final : public XsectShape
{
public:
    const char* name() const override { return "GOTHIC"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_Gothic, N_S_Gothic);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_Gothic, N_Y_Gothic);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * invLookup(yNorm, Y_Gothic, N_Y_Gothic);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Gothic, N_W_Gothic);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_Gothic, N_S_Gothic);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return tabular_getdSdA(xsect, a, S_Gothic, N_S_Gothic);
    }
};

// ---- CATENARY
class Shape_CATENARY final : public XsectShape
{
public:
    const char* name() const override { return "CATENARY"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_Catenary, N_S_Catenary);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_Catenary, N_Y_Catenary);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * invLookup(yNorm, Y_Catenary, N_Y_Catenary);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Catenary, N_W_Catenary);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_Catenary, N_S_Catenary);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return tabular_getdSdA(xsect, a, S_Catenary, N_S_Catenary);
    }
};

// ---- SEMIELLIPTICAL
class Shape_SEMIELLIPTICAL final : public XsectShape
{
public:
    const char* name() const override { return "SEMIELLIPTICAL"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_SemiEllip, N_S_SemiEllip);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_SemiEllip, N_Y_SemiEllip);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * invLookup(yNorm, Y_SemiEllip, N_Y_SemiEllip);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_SemiEllip, N_W_SemiEllip);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_SemiEllip, N_S_SemiEllip);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return  tabular_getdSdA(xsect, a, S_SemiEllip, N_S_SemiEllip);
    }
};

// ---- BASKETHANDLE
class Shape_BASKETHANDLE final : public XsectShape
{
public:
    const char* name() const override { return "BASKETHANDLE"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_BasketHandle, N_S_BasketHandle);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_BasketHandle, N_Y_BasketHandle);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Baskethandle, N_A_Baskethandle);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_BasketHandle, N_W_BasketHandle);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Baskethandle, N_R_Baskethandle);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_BasketHandle, N_S_BasketHandle);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return  tabular_getdSdA(xsect, a, S_BasketHandle, N_S_BasketHandle);
    }
};

// ---- SEMICIRCULAR
class Shape_SEMICIRCULAR final : public XsectShape
{
public:
    const char* name() const override { return "SEMICIRCULAR"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return xsect->sFull * lookup(alpha, S_SemiCirc, N_S_SemiCirc);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * lookup(alpha, Y_SemiCirc, N_Y_SemiCirc);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * invLookup(yNorm, Y_SemiCirc, N_Y_SemiCirc);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_SemiCirc, N_W_SemiCirc);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
    
            return xsect->aFull * invLookup(psi, S_SemiCirc, N_S_SemiCirc);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return  tabular_getdSdA(xsect, a, S_SemiCirc, N_S_SemiCirc);
    }
};

// ---- IRREGULAR
class Shape_IRREGULAR final : public XsectShape
{
public:
    const char* name() const override { return "IRREGULAR"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha,
                xsect->aTbl, xsect->nTbl);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm,
                xsect->aTbl, xsect->nTbl);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm,
                xsect->wTbl, xsect->nTbl);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm,
                xsect->rTbl, xsect->nTbl);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- CUSTOM
class Shape_CUSTOM final : public XsectShape
{
public:
    const char* name() const override { return "CUSTOM"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha,
                xsect->aTbl, xsect->nTbl);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm,
                xsect->aTbl, xsect->nTbl);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm,
                xsect->wTbl, xsect->nTbl);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm,
                xsect->rTbl, xsect->nTbl);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

// ---- FORCE_MAIN
class Shape_FORCE_MAIN final : public XsectShape
{
public:
    const char* name() const override { return "FORCE_MAIN"; }
    double getSofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
        double r;
    
            return circ_getSofA(xsect, a);
    }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
         return circ_getYofA(xsect, a);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
            return xsect->aFull * lookup(yNorm, A_Circ, N_A_Circ);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->wMax * lookup(yNorm, W_Circ, N_W_Circ);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
            return xsect->rFull * lookup(yNorm, R_Circ, N_R_Circ);
    }
    double getAofS(TXsect* xsect, double s) const override
    {
        double psi = s / xsect->sFull;
        if ( s <= 0.0 ) return 0.0;
        if ( s > xsect->sMax ) s = xsect->sMax;
          return circ_getAofS(xsect, s);
    }
    double getdSdA(TXsect* xsect, double a) const override
    {
    
            return circ_getdSdA(xsect, a);
    }
};

// ---- STREET_XSECT
class Shape_STREET_XSECT final : public XsectShape
{
public:
    const char* name() const override { return "STREET_XSECT"; }
    double getYofA(TXsect *xsect, double a) const override
    {
        double alpha = a / xsect->aFull;
    
            return xsect->yFull * invLookup(alpha,
                xsect->aTbl, xsect->nTbl);
    }
    double getAofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
        if ( y <= 0.0 ) return 0.0;
    
              return xsect->aFull * lookup(yNorm,
                  xsect->aTbl, xsect->nTbl);
    }
    double getWofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
              return xsect->wMax * lookup(yNorm,
                  xsect->wTbl, xsect->nTbl);
    }
    double getRofY(TXsect *xsect, double y) const override
    {
        double yNorm = y / xsect->yFull;
    
              return xsect->rFull * lookup(yNorm,
                  xsect->rTbl, xsect->nTbl);
    }
    double getRofA(TXsect *xsect, double a) const override
    {
        double cathy;
        if ( a <= 0.0 ) return 0.0;
    
            return xsect_getRofY( xsect, xsect_getYofA(xsect, a) );
    }
};

const XsectShape& shapeOf(int type)
{
    static const XsectShape base;
    static const Shape_DUMMY s_DUMMY;
    static const Shape_CIRCULAR s_CIRCULAR;
    static const Shape_FILLED_CIRCULAR s_FILLED_CIRCULAR;
    static const Shape_RECT_CLOSED s_RECT_CLOSED;
    static const Shape_RECT_OPEN s_RECT_OPEN;
    static const Shape_TRAPEZOIDAL s_TRAPEZOIDAL;
    static const Shape_TRIANGULAR s_TRIANGULAR;
    static const Shape_PARABOLIC s_PARABOLIC;
    static const Shape_POWERFUNC s_POWERFUNC;
    static const Shape_RECT_TRIANG s_RECT_TRIANG;
    static const Shape_RECT_ROUND s_RECT_ROUND;
    static const Shape_MOD_BASKET s_MOD_BASKET;
    static const Shape_HORIZ_ELLIPSE s_HORIZ_ELLIPSE;
    static const Shape_VERT_ELLIPSE s_VERT_ELLIPSE;
    static const Shape_ARCH s_ARCH;
    static const Shape_EGGSHAPED s_EGGSHAPED;
    static const Shape_HORSESHOE s_HORSESHOE;
    static const Shape_GOTHIC s_GOTHIC;
    static const Shape_CATENARY s_CATENARY;
    static const Shape_SEMIELLIPTICAL s_SEMIELLIPTICAL;
    static const Shape_BASKETHANDLE s_BASKETHANDLE;
    static const Shape_SEMICIRCULAR s_SEMICIRCULAR;
    static const Shape_IRREGULAR s_IRREGULAR;
    static const Shape_CUSTOM s_CUSTOM;
    static const Shape_FORCE_MAIN s_FORCE_MAIN;
    static const Shape_STREET_XSECT s_STREET_XSECT;
    static const XsectShape* const table[] = {
        &s_DUMMY,
        &s_CIRCULAR,
        &s_FILLED_CIRCULAR,
        &s_RECT_CLOSED,
        &s_RECT_OPEN,
        &s_TRAPEZOIDAL,
        &s_TRIANGULAR,
        &s_PARABOLIC,
        &s_POWERFUNC,
        &s_RECT_TRIANG,
        &s_RECT_ROUND,
        &s_MOD_BASKET,
        &s_HORIZ_ELLIPSE,
        &s_VERT_ELLIPSE,
        &s_ARCH,
        &s_EGGSHAPED,
        &s_HORSESHOE,
        &s_GOTHIC,
        &s_CATENARY,
        &s_SEMIELLIPTICAL,
        &s_BASKETHANDLE,
        &s_SEMICIRCULAR,
        &s_IRREGULAR,
        &s_CUSTOM,
        &s_FORCE_MAIN,
        &s_STREET_XSECT
    };
    if ( type < 0 || type >= 26 ) return base;
    return *table[type];
}

int shapeOverrides(int type)
{
    static const int bits[] = {64, 223, 62, 175, 175, 190, 190, 62, 62, 191, 191, 174, 62, 62, 62, 223, 223, 207, 207, 207, 223, 207, 62, 62, 223, 62};
    return ( type < 0 || type >= 26 ) ? 0 : bits[type];
}

// ---- public entry points: one virtual call replaces each switch
double xsect_getSofA(TXsect *xsect, double a)
{
    return shapeOf(xsect->type).getSofA(xsect, a);
}

double xsect_getYofA(TXsect *xsect, double a)
{
    return shapeOf(xsect->type).getYofA(xsect, a);
}

double xsect_getAofY(TXsect *xsect, double y)
{
    return shapeOf(xsect->type).getAofY(xsect, y);
}

double xsect_getWofY(TXsect *xsect, double y)
{
    return shapeOf(xsect->type).getWofY(xsect, y);
}

double xsect_getRofY(TXsect *xsect, double y)
{
    return shapeOf(xsect->type).getRofY(xsect, y);
}

double xsect_getRofA(TXsect *xsect, double a)
{
    return shapeOf(xsect->type).getRofA(xsect, a);
}

double xsect_getAofS(TXsect* xsect, double s)
{
    return shapeOf(xsect->type).getAofS(xsect, s);
}

double xsect_getdSdA(TXsect* xsect, double a)
{
    return shapeOf(xsect->type).getdSdA(xsect, a);
}

// ---- remaining geometry, verbatim from xsect.c
double xsect_getAmax(TXsect* xsect)
//
//  Input:   xsect = ptr. to a cross section data structure
//  Output:  returns area (ft2)
//  Purpose: finds xsection area at maximum flow depth.
//
{
    if ( xsect->type == IRREGULAR ) return xsect->aBot;
    else if ( xsect->type == CUSTOM ) return xsect->aBot;
    else return Amax[xsect->type] * xsect->aFull;
}

double xsect_getYcrit(TXsect* xsect, double q)
//
//  Input:   xsect = ptr. to a cross section data structure
//           q = flow rate (cfs)
//  Output:  returns critical depth (ft)
//  Purpose: computes critical depth at a specific flow rate.
//
{
    double q2g = SQR(q) / GRAVITY;
    double y, r;

    if ( q2g == 0.0 ) return 0.0;
    switch ( xsect->type )
    {
      case DUMMY:
        return 0.0;

      case RECT_OPEN:
      case RECT_CLOSED:
        // --- analytical expression for yCritical is
        //     y = (q2g / w^2)^(1/3) where w = width
        y = pow(q2g / SQR(xsect->wMax), 1./3.);
        break;

      case TRIANGULAR:
        // --- analytical expression for yCritical is
        //     y = (2 * q2g / s^2)^(1/5) where s = side slope
        y = pow(2.0 * q2g / SQR(xsect->sBot), 1./5.);
        break;

      case PARABOLIC:
        // --- analytical expression for yCritical is
        //     y = (27/32 * q2g * c)^(1/4) where y = c*x^2
        //     is eqn. for parabola and 1/sqrt(c) = rBot
        y = pow(27./32. * q2g / SQR(xsect->rBot), 1./4.);
        break;

      case POWERFUNC:
        y = 1. / (2.0 * xsect->sBot + 3.0);
        y = pow( q2g * (xsect->sBot + 1.0) / SQR(xsect->rBot), y);
        break;

      default:
        // --- first estimate yCritical for an equivalent circular conduit
        //     using 1.01 * (q2g / yFull)^(1/4)
        y = 1.01 * pow(q2g / xsect->yFull, 1./4.);
        if (y >= xsect->yFull) y = 0.97 * xsect->yFull;

        // --- then find ratio of conduit area to equiv. circular area
        r = xsect->aFull / (PI / 4.0 * SQR(xsect->yFull));

        // --- use interval enumeration method to find yCritical if
        //     area ratio not too far from 1.0
        if ( r >= 0.5 && r <= 2.0 )
            y = getYcritEnum(xsect, q, y);

        // --- otherwise use Ridder's root finding method
        else y = getYcritRidder(xsect, q, y);
    }

    // --- do not allow yCritical to be > yFull
    return MIN(y, xsect->yFull);
}

void getTransectParams(TXsect *xsect, TTransect *transect)
//
//  Input:   xsect = ptr. to a cross section data structure
//           transect = ptr. to a transect data structure
//  Output:  none
//  Purpose: gets a cross section's properties from its transect's properties.
//
{
    int     i, iMax;
    double  wMax;
    double* wTbl = transect->widthTbl;

    xsect->yFull = transect->yFull;
    xsect->wMax = transect->wMax;
    xsect->aFull = transect->aFull;
    xsect->rFull = transect->rFull;
    xsect->sFull = xsect->aFull * pow(xsect->rFull, 2. / 3.);
    xsect->sMax = transect->sMax;
    xsect->aBot = transect->aMax;

    // Search transect's width table up to point where width decreases
    iMax = 0;
    wMax = wTbl[0];
    for (i = 1; i < transect->nTbl; i++)
    {
        if (wTbl[i] < wMax) break;
        wMax = wTbl[i];
        iMax = i;
    }

    // Determine height at lowest widest point
    xsect->ywMax = xsect->yFull * (double)iMax / ((double)(transect->nTbl) - 1);
}

double generic_getAofS(TXsect* xsect, double s)
//
//  Input:   xsect = ptr. to a cross section data structure
//           s = section factor (ft^8/3)
//  Output:  returns area (ft2)
//  Purpose: finds area given section factor by
//           solving S = A*(A/P(A))^(2/3) using Newton-Raphson iterations.
//
{
    double a, a1, a2, tol;
    TXsectStar xsectStar;

    if (s <= 0.0) return 0.0;

    // --- if S is between sMax and sFull then
    //     bracket A between aFull and aMax
    if ( (s <= xsect->sMax && s >= xsect->sFull)
    &&   xsect->sMax != xsect->sFull )
    {
        a1 = xsect->aFull;          // do this because sFull < sMax
        a2 = xsect_getAmax(xsect);
    }

    // --- otherwise bracket A between 0 and aMax
    else
    {
        a1 = 0.0;
        a2 = xsect_getAmax(xsect);
    }

    // --- place S & xsect in xsectStar for access by evalSofA function
    xsectStar.xsect = xsect;
    xsectStar.s = s;

    // --- compute starting guess for A
    a = 0.5 * (a1 + a2);

    // use the Newton-Raphson root finder function to find A
    tol = 0.0001 * xsect->aFull;
    swmm::findroot_Newton(a1, a2, &a, tol,
        evalSofA, &xsectStar);
    return a;
}

void evalSofA(double a, double* f, double* df, void* p)
//
//  Input:   a = area
//  Output:  f = root finding function
//           df = derivative of root finding function
//  Purpose: function used in conjunction with getAofS() that evaluates
//           f = S(a) - s and df = dS(a)/dA.
//
{
    TXsectStar* xsectStar;
    double s;

    xsectStar = (TXsectStar *)p;
    s = xsect_getSofA(xsectStar->xsect, a);
    *f = s - xsectStar->s;
    *df = xsect_getdSdA(xsectStar->xsect, a);
}

double tabular_getdSdA(TXsect* xsect, double a, const double *table, int nItems)
//
//  Input:   xsect = ptr. to cross section data structure
//           a = area (ft2)
//           table = ptr. to table of section factor v. normalized area
//           nItems = number of equally spaced items in table
//  Output:  returns derivative of section factor w.r.t. area (ft^2/3)
//  Purpose: computes derivative of section factor w.r.t area
//           using geometry tables.
//
{
    int    i;
    double alpha = a / xsect->aFull;
    double delta = 1.0 / ((double)nItems-1);
    double dSdA;

    // --- find which segment of table contains alpha
    i = (int)(alpha / delta);
    if ( i >= nItems - 1 ) i = nItems - 2;

    // --- compute slope from this interval of table
    dSdA = (table[i+1] - table[i]) / delta;

    // --- convert slope to un-normalized value
    return dSdA * xsect->sFull / xsect->aFull;
}

double generic_getdSdA(TXsect* xsect, double a)
//
//  Input:   xsect = ptr. to cross section data structure
//           a = area (ft2)
//  Output:  returns derivative of section factor w.r.t. area (ft^2/3)
//  Purpose: computes derivative of section factor w.r.t area
//           using central difference approximation.
//
{
    double a1, a2;
    double alpha = a / xsect->aFull;
    double alpha1 = alpha - 0.001;
    double alpha2 = alpha + 0.001;
    if ( alpha1 < 0.0 ) alpha1 = 0.0;
    a1 = alpha1 * xsect->aFull;
    a2 = alpha2 * xsect->aFull;
    return (xsect_getSofA(xsect, a2) - xsect_getSofA(xsect, a1)) / (a2 - a1);
}

double lookup(double x, const double *table, int nItems)
//
//  Input:   x = value of independent variable in a geometry table
//           table = ptr. to geometry table
//           nItems = number of equally spaced items in table
//  Output:  returns value of dependent table variable
//  Purpose: looks up a value in a geometry table (i.e., finds y given x).
//
{
    double  delta, x0, x1, y, y2;
    int     i;

    // --- find which segment of table contains x
    delta = 1.0 / ((double)nItems-1);
    i = (int)(x / delta);
    if ( i >= nItems - 1 ) return table[nItems-1];

    // --- compute x at start and end of segment
    x0 = i * delta;
    x1 = ((double)i+1) * delta;

    // --- linearly interpolate a y-value
    y = table[i] + (x - x0) * (table[i+1] - table[i]) / delta;

    // --- use quadratic interpolation for low x value
    if ( i < 2 )
    {
        y2 = y + (x - x0) * (x - x1) / (delta*delta) *
             (table[i]/2.0 - table[i+1] + table[i+2]/2.0) ;
        if ( y2 > 0.0 ) y = y2;
    }
    if ( y < 0.0 ) y = 0.0;
    return y;
}

double invLookup(double y, const double *table, int nItems)
//
//  Input:   y = value of dependent variable in a geometry table
//           table = ptr. to geometry table
//           nItems = number of equally spaced items in table
//  Output:  returns value of independent table variable
//  Purpose: performs inverse lookup in a geometry table (i.e., finds
//           x given y).
//
//  Notes:   This function assumes that the geometry table has either strictly
//           increasing entries or that the maximum entry is always third
//           from the last (which is true for all section factor tables). In
//           the latter case, the location of a large y can be ambiguous
//           -- it can be both below and above the location of the maximum.
//           In such cases this routine searches only the interval above
//           the maximum (i.e., the last 2 segments of the table).
//
//           nItems-1 is the highest subscript for the table's data.
//
//           The x value's in a geometry table lie between 0 and 1.
//
{
    double dx;               // x-increment of table
    double x, x0, dy;        // interpolation variables
    int    n;                // # items in increasing portion of table
    int    i;                // lower table index that brackets y

    // --- compute table's uniform x-increment
    dx = 1.0 / (double)((double)nItems-1);

    // --- truncate item count if last 2 table entries are decreasing
    n = nItems;
    if ( table[n-3] > table[n-1] ) n = n - 2;

    // --- check if y falls in decreasing portion of table
    if ( n < nItems && y > table[nItems-1])
    {
        if ( y >= table[nItems-3] ) return ((double)n-1) * dx;
	    if ( y <= table[nItems-2] ) i = nItems - 2;
	    else i = nItems - 3;
    }

    // --- otherwise locate the interval where y falls in the table
    else i = locate(y, table, n-1);
    if ( i >= n - 1 ) return ((double)n-1) * dx;

    // --- compute x at start and end of segment
    x0 = i * dx;

    // --- linearly interpolate an x value
    dy = table[i+1] - table[i];
    if ( dy == 0.0 ) x = x0;
    else x = x0 + (y - table[i]) * dx / dy;
    if ( x < 0.0 ) x = 0.0;
    if ( x > 1.0 ) x = 1.0;
    return x;
}

int locate(double y, const double *table, int jLast)
//
//  Input:   y      = value being located in table
//           table  = ptr. to table with monotonically increasing entries
//           jLast  = highest table entry index to search over
//  Output:  returns index j of table such that table[j] <= y <= table[j+1]
//  Purpose: uses bisection method to locate the highest table index whose
//           table entry does not exceed a given value.
//
//  Notes:   This function is only used in conjunction with invLookup().
//
{
    int j;
    int j1 = 0;
    int j2 = jLast;

    // Check if value <= first table entry
    if ( y <= table[0] ) return 0;

    // Check if value >= the last entry
    if ( y >= table[jLast] ) return jLast;

    // While a portion of the table still remains
    while ( j2 - j1 > 1)
    {
        // Find midpoint of remaining portion of table
        j = (j1 + j2) >> 1;

        // Value is greater or equal to midpoint: search from midpoint to j2
        if ( y >= table[j] ) j1 = j;

        // Value is less than midpoint: search from j1 to midpoint
        else j2 = j;
    }

    // Return the lower index of the remaining interval,
    return j1;
}

double getQcritical(double yc, void* p)
//
//  Input:   yc = critical depth (ft)
//           p = pointer to a TXsectStar object
//  Output:  returns flow difference value (cfs)
//  Purpose: finds difference between critical flow at depth yc and
//           some target value.
//
{
    double a, w, qc;
    TXsectStar* xsectStar;

    xsectStar = (TXsectStar *)p;
    a = xsect_getAofY(xsectStar->xsect, yc);
    w = xsect_getWofY(xsectStar->xsect, yc);
    qc = -xsectStar->qc;
    if ( w > 0.0 )  qc = a * sqrt(GRAVITY * a / w) - xsectStar->qc;
    return qc;
}

double getYcritEnum(TXsect* xsect, double q, double y0)
//
//  Input:   xsect = ptr. to cross section data structure
//           q = critical flow rate (cfs)
//           y0 = estimate of critical depth (ft)
//  Output:  returns true critical depth (ft)
//  Purpose: solves a * sqrt(a(y)*g / w(y)) - q for y using interval
//           enumeration with starting guess of y0.
//
{
    double     q0, dy, qc, yc;
    int        i1, i;
    TXsectStar xsectStar;

    // --- divide cross section depth into 25 increments and
    //     locate increment corresponding to initial guess y0
    dy = xsect->yFull / 25.;
    i1 = (int)(y0 / dy);

    // --- evaluate critical flow at this increment
    xsectStar.xsect = xsect;
    xsectStar.qc = 0.0;
    q0 = getQcritical(i1*dy, &xsectStar);

    // --- initial flow lies below target flow
    if ( q0 < q )
    {
        // --- search each successive higher depth increment
        yc = xsect->yFull;
        for ( i = i1+1; i <= 25; i++)
        {
            // --- if critical flow at current depth is above target
            //     then use linear interpolation to compute critical depth
            qc = getQcritical(i*dy, &xsectStar);
            if ( qc >= q )
            {
                yc = ( (q-q0) / (qc - q0) + ((double)i-1) ) * dy;
                break;
            }
            q0 = qc;
        }
    }

    // --- initial flow lies above target flow
    else
    {
        // --- search each successively lower depth increment
        yc = 0.0;
        for ( i = i1-1; i >= 0; i--)
        {
            // --- if critical flow at current depth is below target
            //     then use linear interpolation to compute critical depth
            qc = getQcritical(i*dy, &xsectStar);
            if ( qc < q )
            {
                yc = ( (q-qc) / (q0-qc) + (double)i ) * dy;
                break;
            }
            q0 = qc;
        }
    }
    return yc;
}

double getYcritRidder(TXsect* xsect, double q, double y0)
//
//  Input:   xsect = ptr. to cross section data structure
//           q = critical flow rate (cfs)
//           y0 = estimate of critical depth (ft)
//  Output:  returns true critical depth (ft)
//  Purpose: solves a * sqrt(a(y)*g / w(y)) - q for y using Ridder's
//           root finding method with starting guess of y0.
//
{
    double  y1 = 0.0;
    double  y2 = 0.99 * xsect->yFull;
    double  yc;
    double q0, q1, q2;
    TXsectStar xsectStar;

    // --- store reference to cross section in global pointer
    xsectStar.xsect = xsect;
    xsectStar.qc = 0.0;

    // --- check if critical flow at (nearly) full depth < target flow
    q2 = getQcritical(y2, &xsectStar);
    if (q2 < q ) return xsect->yFull;

    // --- evaluate critical flow at initial depth guess y0
    //     and at 1/2 of full depth
    q0 = getQcritical(y0, &xsectStar);
    q1 = getQcritical(0.5*xsect->yFull, &xsectStar);

    // --- adjust search interval on depth so it contains flow q
    if ( q0 > q )
    {
        y2 = y0;
        if ( q1 < q ) y1 = 0.5*xsect->yFull;
    }
    else
    {
        y1 = y0;
        if ( q1 > q ) y2 = 0.5*xsect->yFull;
    }

    // --- save value of target critical flow in global variable
    xsectStar.qc = q;

    // --- call Ridder root finding procedure with error tolerance
    //     of 0.001 ft. to find critical depth yc
    yc = swmm::findroot_Ridder(y1, y2, 0.001,
        getQcritical, &xsectStar);
    return yc;
}

double rect_closed_getSofA(TXsect* xsect, double a)
{
    // --- if a > area corresponding to Smax then
    //     interpolate between sMax and Sfull
    double alfMax = RECT_ALFMAX;
    if ( a / xsect->aFull > alfMax )
    {
        return xsect->sMax + (xsect->sFull - xsect->sMax) *
               (a/xsect->aFull - alfMax) / (1.0 - alfMax);
    }

    // --- otherwise use regular formula
    return a * pow(xsect_getRofA(xsect, a), 2./3.);
}

double rect_closed_getdSdA(TXsect* xsect, double a)
{
    double alpha, alfMax, r;

    // --- if above level corresponding to sMax, then
    //     use slope between sFull & sMax
    alfMax = RECT_ALFMAX;
    alpha = a / xsect->aFull;
    if ( alpha > alfMax )
    {
        return (xsect->sFull - xsect->sMax) /
               ((1.0 - alfMax) * xsect->aFull);
    }

    // --- for small a/aFull use generic central difference formula
    if ( alpha <= 1.0e-30 ) return generic_getdSdA(xsect, a);

    // --- otherwise evaluate dSdA = [5/3 - (2/3)(dP/dA)R]R^(2/3)
    //     (where P = wetted perimeter & dPdA = 2/width)
    r = xsect_getRofA(xsect, a);
    return  (5./3. - (2./3.) * (2.0/xsect->wMax) * r) * pow(r, 2./3.);
}

double rect_closed_getRofA(TXsect* xsect, double a)
{
    double p;
    if ( a <= 0.0 )   return 0.0;
    p = xsect->wMax + 2.*a/xsect->wMax; // Wetted Perim = width + 2*area/width
    if ( a/xsect->aFull > RECT_ALFMAX )
    {
        p += (a/xsect->aFull - RECT_ALFMAX) / (1.0 - RECT_ALFMAX) * xsect->wMax;
    }
    return a / p;
}

double rect_open_getSofA(TXsect* xsect, double a)
{
    double y = a / xsect->wMax;
    double r = a / ((2.0-xsect->sBot)*y + xsect->wMax);
    return a * pow(r, 2./3.);
}

double rect_open_getdSdA(TXsect* xsect, double a)
{
    double r, dPdA;

    // --- for small a/aFull use generic central difference formula
    if ( a / xsect->aFull <= 1.0e-30 ) return generic_getdSdA(xsect, a);

    // --- otherwise evaluate dSdA = [5/3 - (2/3)(dP/dA)R]R^(2/3)
    //     (where P = wetted perimeter)
    r = xsect_getRofA(xsect, a);
    dPdA = (2.0 - xsect->sBot) / xsect->wMax; // since P = geom2 + 2a/geom2
    return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
}

double rect_triang_getYofA(TXsect* xsect, double a)
{
    // below upper section
    if ( a <= xsect->aBot ) return sqrt(a / xsect->sBot);

    // above bottom section
    else return xsect->yBot + (a - xsect->aBot) / xsect->wMax;
}

double rect_triang_getRofA(TXsect* xsect, double a)
{
    double y;
    double p, alf;

    if ( a <= 0.0 )   return 0.0;
    y = rect_triang_getYofA(xsect, a);

    // below upper section
    if ( y <= xsect->yBot ) return a / (2. * y * xsect->rBot);

    // wetted perimeter without contribution of top surface
    p = 2. * xsect->yBot * xsect->rBot + 2. * (y - xsect->yBot);

    // top-surface contribution
    alf = (a / xsect->aFull) - RECT_TRIANG_ALFMAX;
    if ( alf > 0.0 ) p += alf / (1.0 - RECT_TRIANG_ALFMAX) * xsect->wMax;
    return a / p;
}

double rect_triang_getSofA(TXsect* xsect, double a)
{
    // --- if a > area corresponding to sMax, then
    //     interpolate between sMax and Sfull
    double alfMax = RECT_TRIANG_ALFMAX;
    if ( a / xsect->aFull > alfMax )
        return xsect->sMax + (xsect->sFull - xsect->sMax) *
               (a/xsect->aFull - alfMax) / (1.0 - alfMax);

    // --- otherwise use regular formula
    else return a * pow(rect_triang_getRofA(xsect, a), 2./3.);
}

double rect_triang_getdSdA(TXsect* xsect, double a)
{
    double alpha, alfMax, dPdA, r;

    // --- if a > area corresponding to sMax, then
    //     use slope between sFull & sMax
    alfMax = RECT_TRIANG_ALFMAX;
    alpha = a / xsect->aFull;
    if ( alpha > alfMax )
        return (xsect->sFull - xsect->sMax) / ((1.0 - alfMax) * xsect->aFull);

    // --- use generic central difference method for very small a
    if ( alpha <= 1.0e-30 ) return generic_getdSdA(xsect, a);

    // --- find deriv. of wetted perimeter
    if ( a > xsect->aBot ) dPdA = 2.0 / xsect->wMax;  // for upper rectangle
    else dPdA = xsect->rBot / sqrt(a * xsect->sBot);  // for triang. bottom

    // --- get hyd. radius & evaluate section factor derivative formula
    r = rect_triang_getRofA(xsect, a);
    return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
}

double rect_triang_getAofY(TXsect* xsect, double y)
{
    if ( y <= xsect->yBot ) return y * y * xsect->sBot;         // below upper section
    else return xsect->aBot + (y - xsect->yBot) * xsect->wMax;  // above bottom section
}

double rect_triang_getRofY(TXsect* xsect, double y)
{
    double p, a, alf;

    // y is below upper rectangular section
    if ( y <= xsect->yBot ) return y * xsect->sBot / (2. * xsect->rBot);

    // area
    a = xsect->aBot + (y - xsect->yBot) * xsect->wMax;

    // wetted perimeter without contribution of top surface
    p = 2. * xsect->yBot * xsect->rBot + 2. * (y - xsect->yBot);

    // top-surface contribution
    alf = (a / xsect->aFull) - RECT_TRIANG_ALFMAX;
    if ( alf > 0.0 ) p += alf / (1.0 - RECT_TRIANG_ALFMAX) * xsect->wMax;
    return a / p;
}

double rect_triang_getWofY(TXsect* xsect, double y)
{
    if ( y <= xsect->yBot ) return 2.0 * xsect->sBot * y;  // below upper section
    else return xsect->wMax;                               // above bottom section
}

double rect_round_getYofA(TXsect* xsect, double a)
{
    double alpha;

    // --- if above circular bottom:
    if ( a > xsect->aBot )
        return xsect->yBot + (a - xsect->aBot) / xsect->wMax;

    // --- otherwise use circular xsection method to find height
    alpha = a / (PI * xsect->rBot * xsect->rBot);
    if ( alpha < 0.04 ) return (2.0 * xsect->rBot) * getYcircular(alpha);
    return (2.0 * xsect->rBot) * lookup(alpha, Y_Circ, N_Y_Circ);
}

double rect_round_getRofA(TXsect* xsect, double a)
{
    double y1, theta1, p, arg;

    // --- if above circular invert ...
    if ( a <= 0.0 ) return 0.0;
    if ( a > xsect->aBot )
    {
        // wetted perimeter without contribution of top surface
        y1 = (a - xsect->aBot) / xsect->wMax;
        theta1 = 2.0 * asin(xsect->wMax/2.0/xsect->rBot);
        p = xsect->rBot*theta1 + 2.0*y1;

        // top-surface contribution
        arg = (a / xsect->aFull) - RECT_ROUND_ALFMAX;
        if ( arg > 0.0 ) p += arg / (1.0 - RECT_ROUND_ALFMAX) * xsect->wMax;
        return a / p;
    }

    // --- if within circular invert ...
    y1 = rect_round_getYofA(xsect, a);
    theta1 = 2.0*acos(1.0 - y1/xsect->rBot);
    p = xsect->rBot * theta1;
    return a / p;
}

double rect_round_getSofA(TXsect* xsect, double a)
{
    double alpha, aFull, sFull;

    // --- if a > area corresponding to sMax,
    //     interpolate between sMax and sFull
    double alfMax = RECT_ROUND_ALFMAX;
    if ( a / xsect->aFull > alfMax )
    {
        return xsect->sMax + (xsect->sFull - xsect->sMax) *
               (a / xsect->aFull - alfMax) / (1.0 - alfMax);
    }

    // --- if above circular invert, use generic function
    else if ( a > xsect->aBot )
    {
        return a * pow(xsect_getRofA(xsect, a), 2./3.);
    }

    // --- otherwise use circular xsection function applied
    //     to full circular shape of bottom section
    else
    {
        aFull = PI * xsect->rBot * xsect->rBot;
        alpha = a / aFull;
        sFull = xsect->sBot;

        // --- use special function for small a/aFull
        if ( alpha < 0.04 ) return sFull * getScircular(alpha);

        // --- otherwise use table
        else return sFull * lookup(alpha, S_Circ, N_S_Circ);
    }
}

double rect_round_getdSdA(TXsect* xsect, double a)
{
    double alfMax, r, dPdA;

    // --- if a > area corresponding to sMax, then
    //     use slope between sFull & sMax
    alfMax = RECT_ROUND_ALFMAX;
    if ( a / xsect->aFull > alfMax )
    {
        return (xsect->sFull - xsect->sMax) /
               ((1.0 - alfMax) * xsect->aFull);
    }

    // --- if above circular invert, use analytical function for dS/dA
    else if ( a > xsect->aBot )
    {
        r = rect_round_getRofA(xsect, a);
        dPdA = 2.0 / xsect->wMax;       // d(wet perim)/dA for rect.
        return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
    }

    // --- otherwise use generic finite difference function
    else return generic_getdSdA(xsect, a);
}

double rect_round_getAofY(TXsect* xsect, double y)
{
    double theta1;

    // --- if above circular invert...
    if ( y > xsect->yBot )
        return xsect->aBot + (y - xsect->yBot) * xsect->wMax;

    // --- find area of circular section
    theta1 = 2.0*acos(1.0 - y/xsect->rBot);
    return 0.5 * xsect->rBot * xsect->rBot * (theta1 - sin(theta1));
}

double rect_round_getRofY(TXsect* xsect, double y)
{
    double theta1;

    // --- if above top of circular bottom, use RofA formula
    if ( y <= 0.0 ) return 0.0;
    if ( y > xsect->yBot )
        return rect_round_getRofA( xsect, rect_round_getAofY(xsect, y) );

    // --- find hyd. radius of circular section
    theta1 = 2.0*acos(1.0 - y/xsect->rBot);
    return 0.5 * xsect->rBot * (1.0 - sin(theta1)) / theta1;
}

double rect_round_getWofY(TXsect* xsect, double y)
{
    // --- return width if depth above circular bottom section
    if ( y > xsect->yBot ) return xsect->wMax;

    // --- find width of circular section
    return 2.0 * sqrt( y * (2.0*xsect->rBot - y) );
}

double mod_basket_getYofA(TXsect* xsect, double a)
{
    double alpha, y1;

    // --- water level below top of rectangular bottom
    if ( a <= xsect->aFull - xsect->aBot ) return a / xsect->wMax;

    // --- find unfilled top area / area of full circular top
    alpha = (xsect->aFull - a) / (PI * xsect->rBot * xsect->rBot);

    // --- find unfilled height
    if ( alpha < 0.04 ) y1 = getYcircular(alpha);
    else                y1 = lookup(alpha, Y_Circ, N_Y_Circ);
    y1 = 2.0 * xsect->rBot * y1;

    // --- return difference between full height & unfilled height
    return xsect->yFull - y1;
}

double mod_basket_getRofA(TXsect* xsect, double a)
{
    double y1, p, theta1;

    // --- water level is below top of rectangular bottom;
    //     return hyd. radius of rectangle
    if ( a <= xsect->aFull - xsect->aBot )
        return a / (xsect->wMax + 2.0 * a / xsect->wMax);

    // --- find height of empty area
    y1 = xsect->yFull - mod_basket_getYofA(xsect, a);

    // --- find angle of circular arc corresponding to this height
    theta1 = 2.0 * acos(1.0 - y1 / xsect->rBot);

    // --- find perimeter of wetted portion of circular arc
    //     (angle of full circular opening was stored in sBot)
    p = (xsect->sBot - theta1) * xsect->rBot;

    // --- add on wetted perimeter of bottom rectangular area
    y1 = xsect->yFull - xsect->yBot;
    p =  p + 2.0*y1 + xsect->wMax;

    // --- return area / wetted perimeter
    return a / p;
}

double mod_basket_getdSdA(TXsect* xsect, double a)
{
    double r, dPdA;

    // --- if water level below top of rectangular bottom but not
    //     empty then use same code as for rectangular xsection
    if ( a <= xsect->aFull - xsect->aBot && a/xsect->aFull > 1.0e-30 )
    {
        r = a / (xsect->wMax + 2.0 * a / xsect->wMax);
        dPdA = 2.0 / xsect->wMax;
        return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
    }

    // --- otherwise use generic function
    else return generic_getdSdA(xsect, a);
}

double mod_basket_getAofY(TXsect* xsect, double y)
{
    double a1, theta1, y1;

    // --- if water level is below top of rectangular bottom
    //     return depth * width
    if ( y <= xsect->yFull - xsect->yBot ) return y * xsect->wMax;

    // --- find empty top circular area
    y1 = xsect->yFull - y;
    theta1 = 2.0*acos(1.0 - y1/xsect->rBot);
    a1 = 0.5 * xsect->rBot * xsect->rBot * (theta1 - sin(theta1));

    // --- return difference between full and empty areas
    return xsect->aFull - a1;
}

double mod_basket_getWofY(TXsect* xsect, double y)
{
    double y1;

    // --- if water level below top of rectangular bottom then return width
    if ( y <= 0.0 ) return 0.0;
    if ( y <= xsect->yFull - xsect->yBot ) return xsect->wMax;

    // --- find width of empty top circular section
    y1 = xsect->yFull - y;
    return 2.0 * sqrt( y1 * (2.0*xsect->rBot - y1) );
}

double trapez_getYofA(TXsect* xsect, double a)
{
    if ( xsect->sBot == 0.0 ) return a / xsect->yBot;
    return ( sqrt( xsect->yBot*xsect->yBot + 4.*xsect->sBot*a )
             - xsect->yBot )/(2. * xsect->sBot);
}

double trapez_getRofA(TXsect* xsect, double a)
{
    return a / (xsect->yBot + trapez_getYofA(xsect, a) * xsect->rBot);
}

double trapez_getdSdA(TXsect* xsect, double a)
{
    double r, dPdA;
    // --- use generic central difference method for very small a
    if ( a/xsect->aFull <= 1.0e-30 ) return generic_getdSdA(xsect, a);

    // --- otherwise use analytical formula:
    //     dSdA = [5/3 - (2/3)(dP/dA)R]R^(2/3)
    r = trapez_getRofA(xsect, a);
    dPdA = xsect->rBot /
           sqrt( xsect->yBot * xsect->yBot + 4. * xsect->sBot * a );
    return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
}

double trapez_getAofY(TXsect* xsect, double y)
{
    return ( xsect->yBot + xsect->sBot * y ) * y;
}

double trapez_getRofY(TXsect* xsect, double y)
{
    if ( y == 0.0 ) return 0.0;
    return trapez_getAofY(xsect, y) / (xsect->yBot + y * xsect->rBot);
}

double trapez_getWofY(TXsect* xsect, double y)
{
    return xsect->yBot + 2.0 * y * xsect->sBot;
}

double triang_getYofA(TXsect* xsect, double a)
{
    return sqrt(a / xsect->sBot);
}

double triang_getRofA(TXsect* xsect, double a)
{
    return a / (2. * triang_getYofA(xsect, a) * xsect->rBot);
}

double triang_getdSdA(TXsect* xsect, double a)
{
    double r, dPdA;
    // --- use generic finite difference method for very small 'a'
    if ( a/xsect->aFull <= 1.0e-30 ) return generic_getdSdA(xsect, a);

    // --- evaluate dSdA = [5/3 - (2/3)(dP/dA)R]R^(2/3)
    r = triang_getRofA(xsect, a);
    dPdA = xsect->rBot / sqrt(a * xsect->sBot);
    return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
}

double triang_getAofY(TXsect* xsect, double y)
{
    return y * y * xsect->sBot;
}

double triang_getRofY(TXsect* xsect, double y)
{
    return (y * xsect->sBot) / (2. * xsect->rBot);
}

double triang_getWofY(TXsect* xsect, double y)
{
    return 2.0 * xsect->sBot * y;
}

double parab_getYofA(TXsect* xsect, double a)
{
    return pow( (3./4.) * a / xsect->rBot, 2./3. );
}

double parab_getRofA(TXsect* xsect, double a)
{
    if ( a <= 0.0 ) return 0.0;
    return a / parab_getPofY( xsect, parab_getYofA(xsect, a) );
}

double parab_getPofY(TXsect* xsect, double y)
{
    double x = 2. * sqrt(y) / xsect->rBot;
    double t = sqrt(1.0 + x * x);
    return 0.5 * xsect->rBot * xsect->rBot * ( x * t + log(x + t) );
}

double parab_getAofY(TXsect* xsect, double y)
{
    return (4./3. * xsect->rBot * y * sqrt(y));
}

double parab_getRofY(TXsect* xsect, double y)
{
    if ( y <= 0.0 ) return 0.0;
    return parab_getAofY(xsect, y) / parab_getPofY(xsect, y);
}

double parab_getWofY(TXsect* xsect, double y)
{
    return 2.0 * xsect->rBot * sqrt(y);
}

double powerfunc_getYofA(TXsect* xsect, double a)
{
    return pow(a / xsect->rBot, 1.0 / (xsect->sBot + 1.0));
}

double powerfunc_getRofA(TXsect* xsect, double a)
{
    if ( a <= 0.0 ) return 0.0;
    return a / powerfunc_getPofY(xsect, powerfunc_getYofA(xsect, a));
}

double powerfunc_getPofY(TXsect* xsect, double y)
{
    double dy1 = 0.02 * xsect->yFull;
    double h = (xsect->sBot + 1.0) * xsect->rBot / 2.0;
    double m = xsect->sBot;
    double p = 0.0;
    double y1 = 0.0;
    double x1 = 0.0;
    double x2, y2, dx, dy;
    do
    {
        y2 = y1 + dy1;
        if ( y2 > y ) y2 = y;
        x2 = h * pow(y2, m);
        dx = x2 - x1;
        dy = y2 - y1;
        p += sqrt(dx*dx + dy*dy);
        x1 = x2;
        y1 = y2;
    } while ( y2 < y );
    return 2.0 * p;
}

double powerfunc_getAofY(TXsect* xsect, double y)
{
    return xsect->rBot * pow(y, xsect->sBot + 1.0);
}

double powerfunc_getRofY(TXsect* xsect, double y)
{
    if ( y <= 0.0 ) return 0.0;
    return powerfunc_getAofY(xsect, y) / powerfunc_getPofY(xsect, y);
}

double powerfunc_getWofY(TXsect* xsect, double y)
{
    return (xsect->sBot + 1.0) * xsect->rBot * pow(y, xsect->sBot);
}

double circ_getYofA(TXsect* xsect, double a)
{
    double alpha = a / xsect->aFull;

    // --- use special function for small a/aFull
    if ( alpha < 0.04 )  return xsect->yFull * getYcircular(alpha);

    // --- otherwise use table
    else return xsect->yFull * lookup(alpha, Y_Circ, N_Y_Circ);
}

double circ_getAofS(TXsect* xsect, double s)
{
    double psi = s / xsect->sFull;
    if (psi == 0.0) return 0.0;
    if (psi >= 1.0) return xsect->aFull;

    // --- use special function for small s/sFull
    if (psi <= 0.015) return xsect->aFull * getAcircular(psi);

    // --- otherwise use table
    else return xsect->aFull * invLookup(psi, S_Circ, N_S_Circ);
}

double circ_getSofA(TXsect* xsect, double a)
{
    double alpha = a / xsect->aFull;

    // --- use special function for small a/aFull
    if ( alpha < 0.04 ) return xsect->sFull * getScircular(alpha);

    // --- otherwise use table
    else
    return xsect->sFull * lookup(alpha, S_Circ, N_S_Circ);
}

double circ_getdSdA(TXsect* xsect, double a)
{
    double alpha, theta, p, r, dPdA;

    // --- for near-zero area, use generic central difference formula
    alpha = a / xsect->aFull;
    if ( alpha <= 1.0e-30 ) return 1.0e-30;  //generic_getdSdA(xsect, a);

	// --- for small a/aFull use analytical derivative
    else if ( alpha < 0.04 )
    {
        theta = getThetaOfAlpha(alpha);
        p = theta * xsect->yFull / 2.0;
        r = a / p;
        dPdA = 4.0 / xsect->yFull / (1. - cos(theta));
        return  (5./3. - (2./3.) * dPdA * r) * pow(r, 2./3.);
    }

    // --- otherwise use generic tabular getdSdA
    else return tabular_getdSdA(xsect, a, S_Circ, N_S_Circ);
}

double circ_getAofY(TXsect* xsect, double y)
{
    double yNorm;
    yNorm = y / xsect->yFull;
    return xsect->aFull * lookup(yNorm, A_Circ, N_A_Circ);
}

double filled_circ_getYofA(TXsect* xsect, double a)
{
    double y;

    // --- temporarily remove filled portion of circle
    xsect->yFull += xsect->yBot;
    xsect->aFull += xsect->aBot;
    a += xsect->aBot;

    // --- find depth in unfilled circle
    y = circ_getYofA(xsect, a);

    // --- restore original values
    y -= xsect->yBot;
    xsect->yFull -= xsect->yBot;
    xsect->aFull -= xsect->aBot;
    return y;
}

double filled_circ_getAofY(TXsect* xsect, double y)
{
    double a;

    // --- temporarily remove filled portion of circle
    xsect->yFull += xsect->yBot;
    xsect->aFull += xsect->aBot;
    y += xsect->yBot;

    // --- find area of unfilled circle
    a = circ_getAofY(xsect, y);

    // --- restore original values
    a -= xsect->aBot;
    xsect->yFull -= xsect->yBot;
    xsect->aFull -= xsect->aBot;
    return a;
}

double filled_circ_getRofY(TXsect* xsect, double y)
{
    double a, r, p;

    // --- temporarily remove filled portion of circle
    xsect->yFull += xsect->yBot;
    xsect->aFull += xsect->aBot;
    y += xsect->yBot;

    // --- get area,  hyd. radius & wetted perimeter of unfilled circle
    a = circ_getAofY(xsect, y);
    r = 0.25 * xsect->yFull * lookup(y/xsect->yFull, R_Circ, N_R_Circ);
    p = (a/r);

    // --- reduce area and wetted perimeter by amount of filled circle
    //     (rBot = filled perimeter, sBot = filled width)
    a = a - xsect->aBot;
    p = p - xsect->rBot + xsect->sBot;

    // --- compute actual hyd. radius & restore xsect parameters
    r = a / p;
    xsect->yFull -= xsect->yBot;
    xsect->aFull -= xsect->aBot;
    return r;
}

double getYcircular(double alpha)
{
    double theta;
    if ( alpha >= 1.0 ) return 1.0;
    if ( alpha <= 0.0 ) return 0.0;
    if ( alpha <= 1.0e-5 )
    {
        theta = pow(37.6911*alpha, 1./3.);
        return theta * theta / 16.0;
    }
    theta = getThetaOfAlpha(alpha);
    return (1.0 - cos(theta/2.)) / 2.0;
}

double getScircular(double alpha)
{
    double theta;
    if ( alpha >= 1.0 ) return 1.0;
    if ( alpha <= 0.0 ) return 0.0;
    if ( alpha <= 1.0e-5 )
    {
        theta = pow(37.6911*alpha, 1./3.);
        return pow(theta, 13./3.) / 124.4797;
    }
    theta = getThetaOfAlpha(alpha);
    return pow((theta - sin(theta)), 5./3.) / (2.0 * PI) / pow(theta, 2./3.);
}

double getAcircular(double psi)
{
    double theta;
    if ( psi >= 1.0 ) return 1.0;
    if ( psi <= 0.0 ) return 0.0;
    if ( psi <= 1.0e-6 )
    {
        theta = pow(124.4797*psi, 3./13.);
        return theta*theta*theta / 37.6911;
    }
    theta = getThetaOfPsi(psi);
    return (theta - sin(theta)) / (2.0 * PI);
}

double getThetaOfAlpha(double alpha)
{
    int    k;
    double theta, theta1, ap, d;

    if ( alpha > 0.04 ) theta = 1.2 + 5.08 * (alpha - 0.04) / 0.96;
    else theta = 0.031715 - 12.79384 * alpha + 8.28479 * sqrt(alpha);
    theta1 = theta;
    ap  = (2.0*PI) * alpha;
    for (k = 1; k <= 40; k++ )
    {
        d = - (ap - theta + sin(theta)) / (1.0 - cos(theta));
        // --- modification to improve convergence for large theta
        if ( d > 1.0 ) d = SIGN( 1.0, d );
        theta = theta - d;
        if ( fabs(d) <= 0.0001 ) return theta;
    }
    return theta1;
}

double getThetaOfPsi(double psi)
{
    int    k;
    double theta, theta1, ap, tt, tt23, t3, d;

    if      (psi > 0.90)  theta = 4.17 + 1.12 * (psi - 0.90) / 0.176;
    else if (psi > 0.5)   theta = 3.14 + 1.03 * (psi - 0.5) / 0.4;
    else if (psi > 0.015) theta = 1.2 + 1.94 * (psi - 0.015) / 0.485;
    else                  theta = 0.12103 - 55.5075 * psi +
                                  15.62254 * sqrt(psi);
    theta1 = theta;
    ap     = (2.0*PI) * psi;

    for (k = 1; k <= 40; k++)
    {
        theta    = fabs(theta);
        tt       = theta - sin(theta);
        tt23     = pow(tt, 2./3.);
        t3       = pow(theta, 1./3.);
        d        = ap * theta / t3 - tt * tt23;
        d        = d / ( ap*(2./3.)/t3 - (5./3.)*tt23*(1.0-cos(theta)) );
        theta    = theta - d;
        if ( fabs(d) <= 0.0001 ) return theta;
    }
    return theta1;
}

}} // namespace swmm::xs
