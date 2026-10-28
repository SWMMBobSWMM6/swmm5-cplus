//-----------------------------------------------------------------------------
//   transect.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     07/13/23   (Build 5.2.4)
//   Author:   L. Rossman
//
//   Geometry processing for irregular cross-section transects.
//
//   Update History
//   ==============
//   Build 5.2.0:
//   - Function added to create a transect for a Street cross-section.
//   Build 5.2.4:
//   - Corrected street transect points in transect_createStreetTransect.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
#define MAXSTATION 1500                // max. number of stations in a transect

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
              // total number of transects
               // number of stations in current transect
  // x-coordinate of each station
     // elevation of each station
                  // Manning's n for left overbank
                 // Manning's n for right overbank
               // Manning's n for main channel
              // station where left overbank ends
             // station where right overbank begins
                // multiplier for station spacing
                // factor added to station elevations

// ---- transect module state: formerly file-scope variables of transect.c.
// ---- One instance per Project, so two models never share it.
struct TransectState
{
    int Ntransects{};   // was static
    int Nstations{};   // was static
    double Station[1501]{};   // was static
    double Elev[1501]{};   // was static
    double Nleft{};   // was static
    double Nright{};   // was static
    double Nchannel{};   // was static
    double Xleftbank{};   // was static
    double Xrightbank{};   // was static
    double Xfactor{};   // was static
    double Yfactor{};   // was static
    double Lfactor{};   // was static
};
void Project::transect_createState() { transect_ = new TransectState(); }
void Project::transect_destroyState() { delete transect_; transect_ = nullptr; }
                // main channel/flood plain length

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)   
//-----------------------------------------------------------------------------
//  transect_create      (called by createObjects in project.c)
//  transect_delete      (called by deleteObjects in project.c)
//  transect_readParams  (called by parseLine in input.c)
//  transect_validate    (called by input_readData)
//  transect_createStreetTransect  (called by street_readparams)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------









//=============================================================================

int Project::transect_create(int n)
//
//  Input:   n = number of transect objects to create
//  Output:  returns an error code
//  Purpose: creates an array of cross-section transects.
//
{
    transect_->Ntransects = n;
    if ( n == 0 ) return 0;
    Transect = (TTransect *) calloc(transect_->Ntransects, sizeof(TTransect));
    if ( Transect == NULL ) return ERR_MEMORY;
    transect_->Nchannel = 0.0;
    transect_->Nleft = 0.0;
    transect_->Nright = 0.0;
    transect_->Nstations = 0;
    return 0;
}

//=============================================================================

void Project::transect_delete(void)
//
//  Input:   none
//  Output:  none
//  Purpose: deletes memory allocated for all transects.
//
{
    if ( transect_->Ntransects == 0 ) return;
    FREE(Transect);
    transect_->Ntransects = 0;
}

//=============================================================================

int Project::transect_readParams(int* count, char* tok[], int ntoks)
//
//  Input:   count = transect index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  updated value of count,
//           returns an error code
//  Purpose: read parameters of a transect from a tokenized line of input data.
//
//  Format of transect data follows that used for HEC-2 program:
//    NC  nLeft  nRight  nChannel
//    X1  name  nSta  xLeftBank  xRightBank  0  0  0  xFactor  yFactor
//    GR  Elevation  Station  ... 
//
{
    int    i, k;
    int    index = *count;             // transect index
    int    errcode;                    // error code
    double x[10];                      // parameter values
    char*  id;                         // transect ID name

    // --- match first token to a transect keyword
    k = findmatch(tok[0], TransectKeyWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);

    // --- read parameters associated with keyword
    switch ( k )
    {
      // --- NC line: Manning n values
      case 0:

        // --- finish processing the previous transect
        transect_validate(index - 1);

        // --- read Manning's n values
        if ( ntoks < 4 ) return error_setInpError(ERR_ITEMS, "");
        for (i = 1; i <= 3; i++)
        {
            if ( ! getDouble(tok[i], &x[i]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        return setManning(x);

      // --- X1 line: identifies start of next transect
      case 1:

        // --- check that transect was already added to project
        //     (by input_countObjects)
        if ( ntoks < 10 ) return error_setInpError(ERR_ITEMS, "");
        id = project_findID(TRANSECT, tok[1]);
        if ( id == NULL ) return error_setInpError(ERR_NAME, tok[1]);

        // --- read in rest of numerical values on data line
        for ( i = 2; i < 10; i++ )
        {
            if ( ! getDouble(tok[i], &x[i]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }

        // --- update total transect count
        *count = index + 1;

        // --- transfer parameter values to transect's properties
        return setParams(index, id, x);

      // --- GR line: station elevation & location data
      case 2:

        // --- check that line contains pairs of data values
        if ( (ntoks - 1) % 2 > 0 ) return error_setInpError(ERR_ITEMS, "");

        // --- parse each pair of Elevation-Station values
        i = 1;
        while ( i < ntoks )
        {
            if ( ! getDouble(tok[i], &x[1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
            if ( ! getDouble(tok[i+1], &x[2]) )
                return error_setInpError(ERR_NUMBER, tok[i+1]);
            errcode = addStation(x[1], x[2]);
            if ( errcode ) return errcode;
            i += 2;
        }
        return 0;
    }
    return 0;
}

//=============================================================================

void  Project::transect_validate(int j)
//
//  Input:   j = transect index
//  Output:  none
//  Purpose: validates transect data and creates its geometry tables.
//
{
    int    i;
    double ymin, ymax;
    double oldNchannel = transect_->Nchannel;

    // --- check for valid transect data
    if ( j < 0 || j >= transect_->Ntransects ) return;
    if ( transect_->Nstations < 2 ) 
    {
        report_writeErrorMsg(ERR_TRANSECT_TOO_FEW, Transect[j].ID);
        return;
    }
    if ( transect_->Nstations >= MAXSTATION )
    {
        report_writeErrorMsg(ERR_TRANSECT_TOO_MANY, Transect[j].ID);
        return;
    }
    if ( transect_->Nchannel <= 0.0 )
    {
        report_writeErrorMsg(ERR_TRANSECT_MANNING, Transect[j].ID);
        return;
    }
    if ( transect_->Xleftbank > transect_->Xrightbank )
    {
        report_writeErrorMsg(ERR_TRANSECT_OVERBANK, Transect[j].ID);
        return;
    }

    // --- adjust main channel's Mannings n to make its equivalent
    //     length equal to that of entire flood plain
    transect_->Nchannel = transect_->Nchannel * sqrt(transect_->Lfactor);
    Transect[j].lengthFactor = transect_->Lfactor;

    // --- find max. depth across transect
    ymax = transect_->Elev[1];
    ymin = transect_->Elev[1];
    for (i = 2; i <= transect_->Nstations; i++)
    {
        ymax = MAX(transect_->Elev[i], ymax);
        ymin = MIN(transect_->Elev[i], ymin);
    }
    if ( ymin >= ymax )
    {
        report_writeErrorMsg(ERR_TRANSECT_NO_DEPTH, Transect[j].ID);
        return;
    }

    // --- add vertical sides to transect to reach full ht. on both ends
    transect_->Station[0] = transect_->Station[1];
    transect_->Elev[0] = ymax;
    transect_->Nstations++;
    transect_->Station[transect_->Nstations] = transect_->Station[transect_->Nstations-1];
    transect_->Elev[transect_->Nstations] = transect_->Elev[0];

    // --- create geometry tables
    Transect[j].nTbl = N_TRANSECT_TBL;
    createTables(&Transect[j], ymin, ymax);

    // --- save unadjusted main channel roughness 
    Transect[j].roughness = oldNchannel;
}

//=============================================================================

void Project::createTables(TTransect *transect, double ymin, double ymax)
{
    int    i, nLast;
    double dy, y;

    transect->yFull = ymax - ymin;
    transect->wMax = 0.0;

    // --- set 1st table entries to zero
    transect->areaTbl[0] = 0.0;
    transect->hradTbl[0] = 0.0;
    transect->widthTbl[0] = 0.0;

    // --- compute geometry for each depth increment
    dy = (ymax - ymin) / ((double)(transect->nTbl) - 1);
    y = ymin;
    for (i = 1; i < transect->nTbl; i++)
    {
        y += dy;
        transect->areaTbl[i] = 0.0;
        transect->hradTbl[i] = 0.0;
        transect->widthTbl[i] = 0.0;
        getGeometry(transect, i, y);
    }

    // --- determine max. section factor 
    setMaxSectionFactor(transect);

    // --- normalize geometry table entries
    //     (full cross-section values are last table entries)
    nLast = transect->nTbl - 1;
    transect->aFull = transect->areaTbl[nLast];
    transect->rFull = transect->hradTbl[nLast];
    transect->wMax = transect->widthTbl[nLast];

    for (i = 1; i <= nLast; i++)
    {
        transect->areaTbl[i] /= transect->aFull;
        transect->hradTbl[i] /= transect->rFull;
        transect->widthTbl[i] /= transect->wMax;
    }

    // --- set width at 0 height equal to width at 4% of max. height
    transect->widthTbl[0] = transect->widthTbl[1];
}

//=============================================================================

int  Project::setManning(double n[])
//
//  Input:   n[] = array of Manning's n values
//  Output:  returns an error code
//  Purpose: sets Manning's n for overbanks and main channel of a transect.
//
{
    int i;
    for (i=1; i<=3; i++)
    {
        if ( n[i] < 0.0 ) return ERR_NUMBER;
    }
    if ( n[1] > 0.0 ) transect_->Nleft = n[1];
    if ( n[2] > 0.0 ) transect_->Nright = n[2];
    if ( n[3] > 0.0 ) transect_->Nchannel = n[3];
    if ( transect_->Nleft == 0.0  ) transect_->Nleft = transect_->Nchannel;
    if ( transect_->Nright == 0.0 ) transect_->Nright = transect_->Nchannel;
    return 0;
}

//=============================================================================

int  Project::setParams(int j, char* id, double x[])
//
//  Input:   j = transect index
//           id = transect ID name
//           x[] = array of parameter values
//  Output:  returns an error code
//  Purpose: assigns parameter values to current transect being processed.
//
{
    if ( j < 0 || j >= transect_->Ntransects ) return ERR_NUMBER;
    Transect[j].ID = id;                         // ID name
    transect_->Xleftbank = x[3] / UCF(LENGTH);              // left overbank location
    transect_->Xrightbank = x[4] / UCF(LENGTH);             // right overbank location
    transect_->Lfactor = x[7];                              // channel/bank length
    if ( transect_->Lfactor == 0.0 ) transect_->Lfactor = 1.0;
    transect_->Xfactor = x[8];                              // station location multiplier
    if ( transect_->Xfactor == 0.0 ) transect_->Xfactor = 1.0;
    transect_->Xleftbank *= transect_->Xfactor;                        // adjusted left bank
    transect_->Xrightbank *= transect_->Xfactor;                       // adjusted right bank
    transect_->Yfactor = x[9] / UCF(LENGTH);                // elevation offset
    transect_->Nstations = 0;
    return 0;
}

//=============================================================================

int  Project::addStation(double y, double x)
//
//  Input:   y = station elevation value
//           x = station distance value
//  Output:  returns an error code
//  Purpose: adds a new station to the transect currently being processed.
//
{
    // --- check for valid number of stations
    if ( transect_->Nstations < 0 ) return ERR_TRANSECT_UNKNOWN;
    transect_->Nstations++;
    if ( transect_->Nstations >= MAXSTATION ) return 0;

    // --- add station distance, modified by distance multiplier
    transect_->Station[transect_->Nstations] = x * transect_->Xfactor / UCF(LENGTH);

    // --- add station elevation, modified by offset elevation
    transect_->Elev[transect_->Nstations] = (y + transect_->Yfactor) / UCF(LENGTH);

    // --- check if station distances are non-increasing
    if ( transect_->Nstations > 1
        && transect_->Station[transect_->Nstations] < transect_->Station[transect_->Nstations-1] )
        return ERR_TRANSECT_SEQUENCE;
    return 0;    
}

//=============================================================================

void  Project::getGeometry(TTransect *transect, int i, double y)
//
//  Input:   transect = transect being analyzed
//           i = index of current entry in geometry tables
//           y = depth of current entry in geometry tables
//  Output:  none
//  Purpose: computes entries in a transect's geometry tables at a given depth. 
//
{
    int    k;                // station index
    double ylo,              // lower elev. of transect slice
           yhi,              // higher elev. of transect slice
           w,                // top width of transect slice
           wp,               // wetted perimeter of transect slice
           wpSum,            // total wetted perimeter across transect
           a,                // area of transect slice
           aSum,             // total area across transect
           q,                // flow across transect slices with same roughness
           qSum;             // total flow across transect
    int   findFlow;          // true if flow thru area slice needs updating

    // --- initialize
    wpSum = 0.0;
    aSum = 0.0;
    qSum = 0.0;

    // --- examine each horizontal station from left to right
    for (k = 1; k <= transect_->Nstations; k++)
    {
        // --- determine low & high elevations for transect sub-section
        if ( transect_->Elev[k-1] >= transect_->Elev[k] )
        {
            yhi = transect_->Elev[k-1];
            ylo = transect_->Elev[k];
        }
        else
        {
            yhi = transect_->Elev[k];
            ylo = transect_->Elev[k-1];
        }

        // --- skip station if its totally dry
        if ( ylo >= y ) continue;

        // --- get top width, area & wetted perimeter values for transect
        //     slice between station k and k-1
        getSliceGeom(k, y, ylo, yhi, &w, &a, &wp);

        // --- update total transect values
        wpSum += wp;
        aSum += a;
        transect->areaTbl[i] += a;
        transect->widthTbl[i] += w;

        // --- must update flow if station elevation is above water level
        if ( transect_->Elev[k] >= y ) findFlow = TRUE;
        else findFlow = FALSE;

        // --- update flow across transect if called for
        q = getFlow(k, aSum, wpSum, findFlow);
        if ( q > 0.0 )
        {
            qSum += q;
            aSum = 0.0;
            wpSum = 0.0;
        }

    }   // next station k 

    // --- find hyd. radius table entry solving Manning eq. with
    //     total flow, total area, and main channel n
    aSum = transect->areaTbl[i];
    if ( aSum == 0.0 )
        transect->hradTbl[i] = transect->hradTbl[i-1];
    else
        transect->hradTbl[i] = pow(qSum * transect_->Nchannel / 1.49 / aSum, 1.5);
}

//=============================================================================

void Project::getSliceGeom(int k, double y, double ylo, double yhi, double *w,
                  double *a, double *wp)
//
//  Input:   k = station index
//           y = water elevation
//           ylo = transect elevation on low side of slice
//           yhi = transect elevation on high side of slice
//  Output   w = width of transect slice
//           a = area of transect slice
//           wp = wetted perimeter of transect slice
//  Purpose: finds area, width & wetted perim. for slice of transect that
//           is covered by given water depth.
//
//      yhi  |           
//           |
//        y  |**********
//           |********** --> slice of transect being analyzed
//      ylo  |**********|
//           |**********|
//           |**********|
//         Station    Station
//           k-1        k
//
{
    double width, ratio;

    // --- compute width & wetted perimeter of transect slice
    width = fabs(transect_->Station[k] - transect_->Station[k-1]);
    (*w) = width;
    (*wp) = sqrt(width * width + (yhi - ylo) * (yhi - ylo));
    (*a)  = 0.0;

    // --- find area for completely submerged slice
    if ( y > yhi )
    {
        (*a) = width * ( (y - yhi) + (y - ylo) ) / 2.0;
    }

    // --- otherwise find area and adjust width & wetted perim. for
    //     partly submerged slice
    else if ( yhi > ylo )
    {
         ratio = (y - ylo) / (yhi - ylo);
         (*a) = width * (yhi - ylo) / 2.0 * ratio * ratio;
         (*w) *= ratio;
         (*wp) *= ratio;
     }
}

//=============================================================================

double Project::getFlow(int k, double a, double wp, int findFlow)
//
//  Input:   k = index of station at end of transect sub-section
//           a = flow area of sub-section
//           wp = wetted perimeter of flow area of sub-section
//           findFlow = TRUE if flow needs updating 
//  Output:  returns normal flow (per unit of slope)
//  Purpose: finds flow through a sub-section of a transect.
//
{
    double n;                          // Manning's n

    if ( findFlow == FALSE)
    {
        // --- flow needs updating if we are at last station
        if ( k == transect_->Nstations - 1 ) findFlow = TRUE;

        // --- flow needs updating if we are at end of left overbank and
        //     there is a change in Manning's n and section not vertical
        else if ( transect_->Station[k] == transect_->Xleftbank )
        {
            if ( transect_->Nleft != transect_->Nchannel &&
                transect_->Station[k] != transect_->Station[k-1] ) findFlow = TRUE;
        }

        // --- flow needs updating if we are at start of right overbank and
        //     there is a change in Manning's n and section not vertical
        else if ( transect_->Station[k] == transect_->Xrightbank )
        {
            if ( transect_->Nright != transect_->Nchannel &&
                transect_->Station[k] != transect_->Station[k+1] ) findFlow = TRUE;
        }
    }

    // --- if flow needs updating
    if ( findFlow )
    {
        // --- find value of Manning's n to use
        n = transect_->Nchannel;
        if ( transect_->Station[k-1] < transect_->Xleftbank ) n = transect_->Nleft;
        if ( transect_->Station[k] > transect_->Xrightbank )  n = transect_->Nright;

        // --- compute flow through flow area
        //     (PHI is the Manning Eqn. constant defined in consts.h)
        return PHI / n * a * pow(a/wp, 2./3.);
    }
    return 0.0;
}

//=============================================================================

void Project::setMaxSectionFactor(TTransect *transect)
//
//  Input:   transect = transect being analyzed
//  Output:  none
//  Purpose: determines the maximum section factor for a transect and the
//           area where this maxumum occurs.
//
{
    int    i;
    double sf;

    transect->aMax = 0.0;
    transect->sMax = 0.0;
    for (i = 1; i < transect->nTbl; i++)
    {
        sf = transect->areaTbl[i] * pow(transect->hradTbl[i], 2. / 3.);
        if (sf > transect->sMax)
        {
            transect->sMax = sf;
            transect->aMax = transect->areaTbl[i];
        }
    }
}

//=============================================================================

void  Project::transect_createStreetTransect(TStreet* street)
//
{
    double ymin, ymax, y1, y3, y4;
    double w1, w2, w3, w4;

    // Point 0 = top of backing
    // Point 1 = top of curb
    // Point 2 = bottom of curb
    // Point 3 = bottom of depressed gutter
    // Point 4 = top of depressed gutter
    // Point 5 = street crown 

    // --- assign height (y) and width (w) to road & gutter sections
    ymin = 0.0;
    w1 = street->backWidth;
    w2 = street->gutterWidth;
    w3 = street->width;
    w4 = w3 - w2;
    y3 = street->gutterDepression + street->slope * w2;
    y1 = street->curbHeight + street->gutterDepression;
    ymax = street->backSlope * street->backWidth + y1;
    y4 = y3 + street->slope * w4;
    ymax = MAX(ymax, y4);

    // --- assign Station,Elevation points to the street's sections
    transect_->Station[0] = 0.0;
    transect_->Elev[0] = ymax;
    transect_->Station[1] = w1;
    transect_->Elev[1] = y1;
    transect_->Station[2] = w1;
    transect_->Elev[2] = 0.0;
    transect_->Station[3] = w1 + w2;
    transect_->Elev[3] = y3;
    transect_->Station[4] = w1 + w3;
    transect_->Elev[4] = y4;

    // --- a half street ends here
    if (street->sides == 1)
    {
        transect_->Station[5] = transect_->Station[4];
        transect_->Elev[5] = ymax;
        transect_->Nstations = 5;
        street->transect.nTbl = N_TRANSECT_TBL;
    }

    // --- the right side of a full street mirrors the left side
    else
    {
        transect_->Station[5] = transect_->Station[4] + w4;
        transect_->Elev[5] = y3;
        transect_->Station[6] = transect_->Station[5] + w2;
        transect_->Elev[6] = 0.0;
        transect_->Station[7] = transect_->Station[6];
        transect_->Elev[7] = y1;
        transect_->Station[8] = transect_->Station[7] + w1;
        transect_->Elev[8] = ymax;
        transect_->Nstations = 8;
        street->transect.nTbl = N_TRANSECT_TBL;
    }

    // --- assign Manning's N to street
    transect_->Nchannel = street->roughness;
    if (street->backWidth == 0.0)
    {
        transect_->Nleft = transect_->Nchannel;
        transect_->Nright = transect_->Nchannel;
        transect_->Xleftbank = transect_->Station[0];
        transect_->Xrightbank = transect_->Station[transect_->Nstations];
    }
    else
    {
        transect_->Nleft = street->backRoughness;
        transect_->Nright = transect_->Nleft;
        transect_->Xleftbank = transect_->Station[1];
        if (street->sides == 2)
            transect_->Xrightbank = transect_->Station[transect_->Nstations - 1];
        else
            transect_->Xrightbank = transect_->Station[transect_->Nstations];
    }

    // --- create the street's geometry tables
    createTables(&(street->transect), ymin, ymax);
    street->transect.roughness = street->roughness;
}
