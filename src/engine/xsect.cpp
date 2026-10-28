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
#include "headers.h"
#include "xsect_geometry.hpp"
using namespace swmm::xs;

#define  RECT_ALFMAX        0.97
#define  RECT_TRIANG_ALFMAX 0.98
#define  RECT_ROUND_ALFMAX  0.98

// xsect.dat tables: defined in xsect_geometry.cpp; the few that xsect_setParams needs are declared in xsect_geometry.hpp

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
// Ratio of area at max. flow to full area
// (= 1.0 for open shapes, < 1.0 for closed shapes)
// Amax[] now lives in xsect_geometry.cpp (declared in xsect_geometry.hpp)

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
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


int Project::xsect_isOpen(int type)
//
//  Input:   type = type of xsection shape
//  Output:  returns 1 if xsection is open, 0 if not
//  Purpose: determines if a xsection type is open or closed.
//
{
    return ((Amax[type] >= 1.0) ? 1 : 0);
}

int Project::xsect_setParams(TXsect *xsect, int type, double p[], double ucf)
//
//  Input:   xsect = ptr. to a cross section data structure
//           type = xsection shape type
//           p[] = vector of xsection parameters
//           ucf = units correction factor
//  Output:  returns TRUE if successful, FALSE if not
//  Purpose: assigns parameters to a cross section's data structure.
//
{
    int    index;
    double aMax, theta;

    if ( type != DUMMY && p[0] <= 0.0 ) return FALSE;
    xsect->type  = type;
    switch ( xsect->type )
    {
    case DUMMY:
        xsect->yFull = TINY;
        xsect->wMax  = TINY;
        xsect->aFull = TINY;
        xsect->rFull = TINY;
        xsect->sFull = TINY;
        xsect->sMax  = TINY;
        break;

    case CIRCULAR:
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = xsect->yFull;
        xsect->aFull = PI / 4.0 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2500 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.08 * xsect->sFull;
        xsect->ywMax = 0.5 * xsect->yFull;
        break;

    case FORCE_MAIN:
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = xsect->yFull;
        xsect->aFull = PI / 4.0 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2500 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 0.63);
        xsect->sMax  = 1.06949 * xsect->sFull;
        xsect->ywMax = 0.5 * xsect->yFull;

        // --- save C-factor or roughness in rBot position
        xsect->rBot  = p[1];
        break;

    case FILLED_CIRCULAR:
        if ( p[1] >= p[0] ) return FALSE;

        // --- initially compute full values for unfilled pipe
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = xsect->yFull;
        xsect->aFull = PI / 4.0 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2500 * xsect->yFull;

        // --- find:
        //     yBot = depth of filled bottom
        //     aBot = area of filled bottom
        //     sBot = width of filled bottom
        //     rBot = wetted perimeter of filled bottom
        xsect->yBot  = p[1]/ucf;
        xsect->aBot  = circ_getAofY(xsect, xsect->yBot);
        xsect->sBot  = xsect_getWofY(xsect, xsect->yBot);
        xsect->rBot  = xsect->aBot / (xsect->rFull *
                       lookup(xsect->yBot/xsect->yFull, R_Circ, N_R_Circ));

        // --- revise full values for filled bottom
        xsect->aFull -= xsect->aBot;
        xsect->rFull = xsect->aFull /
                       (PI*xsect->yFull - xsect->rBot + xsect->sBot);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.08 * xsect->sFull;
        xsect->yFull -= xsect->yBot;
        xsect->ywMax = 0.5 * xsect->yFull;
        break;

    case EGGSHAPED:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.5105 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.1931 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.065 * xsect->sFull;
        xsect->wMax  = 2./3. * xsect->yFull;
        xsect->ywMax = 0.64 * xsect->yFull;
        break;

    case HORSESHOE:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.8293 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2538 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.077 * xsect->sFull;
        xsect->wMax  = 1.0 * xsect->yFull;
        xsect->ywMax = 0.5 * xsect->yFull;
        break;

    case GOTHIC:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.6554 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2269 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.065 * xsect->sFull;
        xsect->wMax  = 0.84 * xsect->yFull;
        xsect->ywMax = 0.45 * xsect->yFull;
        break;

    case CATENARY:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.70277 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.23172 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.05 * xsect->sFull;
        xsect->wMax  = 0.9 * xsect->yFull;
        xsect->ywMax = 0.25 * xsect->yFull;
        break;

    case SEMIELLIPTICAL:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.785 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.242 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.045 * xsect->sFull;
        xsect->wMax  = 1.0 * xsect->yFull;
        xsect->ywMax = 0.15 * xsect->yFull;
        break;

    case BASKETHANDLE:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 0.7862 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2464 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.06078 * xsect->sFull;
        xsect->wMax  = 0.944 * xsect->yFull;
        xsect->ywMax = 0.2 * xsect->yFull;
        break;

    case SEMICIRCULAR:
        xsect->yFull = p[0]/ucf;
        xsect->aFull = 1.2697 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2946 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = 1.06637 * xsect->sFull;
        xsect->wMax  = 1.64 * xsect->yFull;
        xsect->ywMax = 0.15 * xsect->yFull;
        break;

    case RECT_CLOSED:
        if ( p[1] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->aFull = xsect->yFull * xsect->wMax;
        xsect->rFull = xsect->aFull / (2.0 * (xsect->yFull + xsect->wMax));
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        aMax = RECT_ALFMAX * xsect->aFull;
        xsect->sMax = aMax * pow(rect_closed_getRofA(xsect, aMax), 2./3.);
        xsect->ywMax = xsect->yFull;
        break;

    case RECT_OPEN:
        if ( p[1] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        if (p[2] < 0.0 || p[2] > 2.0) return FALSE;   //# sides to ignore
        xsect->sBot = p[2];
        xsect->aFull = xsect->yFull * xsect->wMax;
        xsect->rFull = xsect->aFull / ((2.0 - xsect->sBot) *
                       xsect->yFull + xsect->wMax);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        xsect->ywMax = xsect->yFull;
        break;

    case RECT_TRIANG:
        if ( p[1] <= 0.0 || p[2] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->yBot  = p[2]/ucf;
        xsect->ywMax = xsect->yFull;

        // --- area of bottom triangle
        xsect->aBot  = xsect->yBot * xsect->wMax / 2.0;

        // --- slope of bottom side wall
        xsect->sBot  = xsect->wMax / xsect->yBot / 2.0;

        // --- length of side wall per unit of depth
        xsect->rBot  = sqrt( 1. + xsect->sBot * xsect->sBot );

        xsect->aFull = xsect->wMax * (xsect->yFull - xsect->yBot / 2.0);
        xsect->rFull = xsect->aFull / (2.0 * xsect->yBot * xsect->rBot + 2.0 *
                        (xsect->yFull - xsect->yBot) + xsect->wMax);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        aMax = RECT_TRIANG_ALFMAX * xsect->aFull;
        xsect->sMax  = aMax * pow(rect_triang_getRofA(xsect, aMax), 2./3.);
        break;

    case RECT_ROUND:
        if ( p[1] <= 0.0 ) return FALSE;
        if ( p[2] < p[1]/2.0 ) p[2] = p[1]/2.0;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->rBot  = p[2]/ucf;

        // --- angle of circular arc
        theta = 2.0 * asin(xsect->wMax / 2.0 / xsect->rBot);

        // --- area of circular bottom
        xsect->aBot  = xsect->rBot * xsect->rBot /
                       2.0 * (theta - sin(theta));

        // --- section factor for circular bottom
        xsect->sBot  = PI * xsect->rBot * xsect->rBot *
                       pow(xsect->rBot/2.0, 2./3.);

        // --- depth of circular bottom
        xsect->yBot  = xsect->rBot * (1.0 - cos(theta/2.0));
        if (xsect->yBot > xsect->yFull) return FALSE;
        xsect->ywMax = xsect->yFull;

        xsect->aFull = xsect->wMax * (xsect->yFull - xsect->yBot) + xsect->aBot;
        xsect->rFull = xsect->aFull / (xsect->rBot * theta + 2.0 *
                        (xsect->yFull - xsect->yBot) + xsect->wMax);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        aMax = RECT_ROUND_ALFMAX * xsect->aFull;
        xsect->sMax = aMax * pow(rect_round_getRofA(xsect, aMax), 2./3.);
        break;

    case MOD_BASKET:
        if ( p[1] <= 0.0 ) return FALSE;
        if ( p[2] < p[1]/2.0 ) p[2] = p[1]/2.0;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;

        // --- radius of circular arc
        xsect->rBot = p[2]/ucf;

        // --- angle of circular arc
        theta = 2.0 * asin(xsect->wMax / 2.0 / xsect->rBot);
        xsect->sBot = theta;

        // --- height of circular arc
        xsect->yBot = xsect->rBot * (1.0 - cos(theta/2.0));
        if (xsect->yBot > xsect->yFull) return FALSE;
        xsect->ywMax = xsect->yFull - xsect->yBot;

        // --- area of circular arc
        xsect->aBot = xsect->rBot * xsect->rBot /
                      2.0 * (theta - sin(theta));

        // --- full area
        xsect->aFull = (xsect->yFull - xsect->yBot) * xsect->wMax +
                       xsect->aBot;

        // --- full hydraulic radius & section factor
        xsect->rFull = xsect->aFull / (xsect->rBot * theta + 2.0 *
                        (xsect->yFull - xsect->yBot) + xsect->wMax);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);

        // --- area corresponding to max. section factor
        xsect->sMax = xsect_getSofA(xsect, Amax[MOD_BASKET]*xsect->aFull);
        break;

    case TRAPEZOIDAL:
        if ( p[1] < 0.0 || p[2] < 0.0 || p[3] < 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->ywMax = xsect->yFull;

        // --- bottom width
        xsect->yBot = p[1]/ucf;

        // --- avg. slope of side walls
        xsect->sBot  = ( p[2] + p[3] )/2.0;
        if ( xsect->yBot == 0.0 && xsect->sBot == 0.0 ) return FALSE;

        // --- length of side walls per unit of depth
        xsect->rBot  = sqrt( 1.0 + p[2]*p[2] ) + sqrt( 1.0 + p[3]*p[3] );

        // --- top width
        xsect->wMax = xsect->yBot + xsect->yFull * (p[2] + p[3]);

        xsect->aFull = ( xsect->yBot + xsect->sBot * xsect->yFull ) * xsect->yFull;
        xsect->rFull = xsect->aFull / (xsect->yBot + xsect->yFull * xsect->rBot);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        break;

    case TRIANGULAR:
        if ( p[1] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->ywMax = xsect->yFull;

        // --- slope of side walls
        xsect->sBot  = xsect->wMax / xsect->yFull / 2.;

        // --- length of side wall per unit of depth
        xsect->rBot  = sqrt( 1. + xsect->sBot * xsect->sBot );

        xsect->aFull = xsect->yFull * xsect->yFull * xsect->sBot;
        xsect->rFull = xsect->aFull / (2.0 * xsect->yFull * xsect->rBot);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        break;

    case PARABOLIC:
        if ( p[1] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->ywMax = xsect->yFull;

        // --- rBot :: 1/c^.5, where y = c*x^2 is eqn. of parabolic shape
        xsect->rBot  = xsect->wMax / 2.0 / sqrt(xsect->yFull);

        xsect->aFull = (2./3.) * xsect->yFull * xsect->wMax;
        xsect->rFull = xsect_getRofY(xsect, xsect->yFull);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        break;

    case POWERFUNC:
        if ( p[1] <= 0.0 || p[2] <= 0.0 ) return FALSE;
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = p[1]/ucf;
        xsect->ywMax = xsect->yFull;
        xsect->sBot  = 1.0 / p[2];
        xsect->rBot  = xsect->wMax / (xsect->sBot + 1) /
                       pow(xsect->yFull, xsect->sBot);
        xsect->aFull = xsect->yFull * xsect->wMax / (xsect->sBot+1);
        xsect->rFull = xsect_getRofY(xsect, xsect->yFull);
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        break;

    case HORIZ_ELLIPSE:
        if ( p[1] == 0.0 ) p[2] = p[0];
        if ( p[2] > 0.0 )                        // std. ellipse pipe
        {
            index = (int)floor(p[2]) - 1;        // size code
            if ( index < 0 ||
                 index >= NumCodesEllipse ) return FALSE;
            xsect->yFull = MinorAxis_Ellipse[index]/12.;
            xsect->wMax  = MajorAxis_Ellipse[index]/12.;
            xsect->aFull = Afull_Ellipse[index];
            xsect->rFull = Rfull_Ellipse[index];
        }
        else
        {
            // --- length of minor axis
            xsect->yFull = p[0]/ucf;

            // --- length of major axis
            if ( p[1] < 0.0 ) return FALSE;
            xsect->wMax = p[1]/ucf;
            xsect->aFull = 1.2692 * xsect->yFull * xsect->yFull;
            xsect->rFull = 0.3061 * xsect->yFull;
        }
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        xsect->ywMax = 0.48 * xsect->yFull;
        break;

    case VERT_ELLIPSE:
        if ( p[1] == 0.0 ) p[2] = p[0];
        if ( p[2] > 0.0 )                        // std. ellipse pipe
        {
            index = (int)floor(p[2]) - 1;        // size code
            if ( index < 0 ||
                 index >= NumCodesEllipse ) return FALSE;
            xsect->yFull = MajorAxis_Ellipse[index]/12.;
            xsect->wMax  = MinorAxis_Ellipse[index]/12.;
            xsect->aFull = Afull_Ellipse[index];
            xsect->rFull = Rfull_Ellipse[index];
        }
        else
        {
            // --- length of major axis
            if ( p[1] < 0.0 ) return FALSE;

            // --- length of minor axis
            xsect->yFull = p[0]/ucf;
            xsect->wMax = p[1]/ucf;
            xsect->aFull = 1.2692 * xsect->wMax * xsect->wMax;
            xsect->rFull = 0.3061 * xsect->wMax;
        }
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        xsect->ywMax = 0.48 * xsect->yFull;
        break;

    case ARCH:
        if ( p[1] == 0.0 ) p[2] = p[0];
        if ( p[2] > 0.0 )                        // std. arch pipe
        {
            index = (int)floor(p[2]) - 1;        // size code
            if ( index < 0 ||
                 index >= NumCodesArch ) return FALSE;
            xsect->yFull = Yfull_Arch[index]/12.;     // Yfull units are inches
            xsect->wMax  = Wmax_Arch[index]/12.;      // Wmax units are inches
            xsect->aFull = Afull_Arch[index];
            xsect->rFull = Rfull_Arch[index];
        }
        else                                     // non-std. arch pipe
        {
            if ( p[1] < 0.0 ) return FALSE;
            xsect->yFull = p[0]/ucf;
            xsect->wMax  = p[1]/ucf;
            xsect->aFull = 0.7879 * xsect->yFull * xsect->wMax;
            xsect->rFull = 0.2991 * xsect->yFull;
        }
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
        xsect->sMax  = xsect->sFull;
        xsect->ywMax = 0.28 * xsect->yFull;
        break;
    }
    return TRUE;
}

void Project::xsect_setIrregXsectParams(TXsect *xsect)
//
//  Input:   xsect = ptr. to a cross section data structure
//  Output:  none
//  Purpose: assigns transect parameters to an irregular shaped cross section.
//
{
    int index = xsect->transect;
    getTransectParams(xsect, &Transect[index]);

    // swmm5-cplus: bind the transect's geometry tables to the cross section,
    // so geometry never has to look up the global Transect array again
    xsect->aTbl = Transect[index].areaTbl;
    xsect->rTbl = Transect[index].hradTbl;
    xsect->wTbl = Transect[index].widthTbl;
    xsect->nTbl = N_TRANSECT_TBL;
}

void Project::xsect_setStreetXsectParams(TXsect *xsect)
//
//  Input:   xsect = ptr. to a cross section data structure
//  Output:  none
//  Purpose: assigns transect parameters to a street cross section.
//
{
    int index = xsect->transect;
    getTransectParams(xsect, &Street[index].transect);

    // swmm5-cplus: bind the street transect's geometry tables
    xsect->aTbl = Street[index].transect.areaTbl;
    xsect->rTbl = Street[index].transect.hradTbl;
    xsect->wTbl = Street[index].transect.widthTbl;
    xsect->nTbl = Street[index].transect.nTbl;
}

void Project::xsect_setCustomXsectParams(TXsect *xsect)
//
//  Input:   xsect = ptr. to a cross section data structure
//  Output:  none
//  Purpose: assigns parameters to a custom-shaped cross section.
//
{
    int     index = Curve[xsect->transect].refersTo;
    double  yFull = xsect->yFull;
    int     i, iMax;
    double  wMax;
    double* wTbl = Shape[index].widthTbl;

    xsect->wMax  = Shape[index].wMax * yFull;
    xsect->aFull = Shape[index].aFull * yFull * yFull;
    xsect->rFull = Shape[index].rFull * yFull;
    xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
    xsect->sMax  = Shape[index].sMax * yFull * yFull * pow(yFull, 2./3.);
    xsect->aBot  = Shape[index].aMax * yFull * yFull;

    // Search shape's width table up to point where width decreases
    iMax = 0;
    wMax = wTbl[0];
    for (i = 1; i < N_SHAPE_TBL; i++)
    {
        if ( wTbl[i] < wMax ) break;
        wMax = wTbl[i];
        iMax = i;
    }

    // Determine height at lowest widest point
    xsect->ywMax = yFull * (double)iMax / (double)(N_SHAPE_TBL-1);

    // swmm5-cplus: bind the custom shape's geometry tables
    xsect->aTbl = Shape[index].areaTbl;
    xsect->rTbl = Shape[index].hradTbl;
    xsect->wTbl = Shape[index].widthTbl;
    xsect->nTbl = N_SHAPE_TBL;
}

double Project::xsect_getAmax(TXsect* xsect)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getAmax(xsect);
}

double Project::xsect_getYcrit(TXsect* xsect, double q)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getYcrit(xsect, q);
}

double Project::xsect_getSofA(TXsect *xsect, double a)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getSofA(xsect, a);
}

double Project::xsect_getYofA(TXsect *xsect, double a)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getYofA(xsect, a);
}

double Project::xsect_getAofY(TXsect *xsect, double y)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getAofY(xsect, y);
}

double Project::xsect_getWofY(TXsect *xsect, double y)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getWofY(xsect, y);
}

double Project::xsect_getRofY(TXsect *xsect, double y)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getRofY(xsect, y);
}

double Project::xsect_getRofA(TXsect *xsect, double a)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getRofA(xsect, a);
}

double Project::xsect_getAofS(TXsect* xsect, double s)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getAofS(xsect, s);
}

double Project::xsect_getdSdA(TXsect* xsect, double a)
//  forwards to the pure geometry in xsect_geometry.cpp
{
    return swmm::xs::xsect_getdSdA(xsect, a);
}
