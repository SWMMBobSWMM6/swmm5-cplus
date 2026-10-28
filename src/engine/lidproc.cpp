//-----------------------------------------------------------------------------
//   lidproc.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     07/13/23   (Build 5.2.4)
//   Author:   L. Rossman
//
//   This module computes the hydrologic performance of an LID (Low Impact
//   Development) unit at a given point in time.
//
//   Update History
//   ==============
//   Build 5.1.007:
//   - Euler integration now applied to all LID types except Vegetative
//     Swale which continues to use successive approximation.
//   - LID layer flux routines were re-written to more accurately model
//     flooded conditions.
//   Build 5.1.008:
//   - MAX_STATE_VARS replaced with MAX_LAYERS.
//   - Optional soil layer added to Porous Pavement LID.
//   - Rooftop Disconnection added to types of LIDs.
//   - Separate accounting of drain flows added.
//   - Indicator for currently wet LIDs added.
//   - Detailed reporting procedure fixed.
//   - Possibile negative head on Bioretention Cell drain avoided.
//   - Bug in computing flow through Green Roof drainage mat fixed.
//   Build 5.1.009:
//   - Fixed typo in net flux rate for vegetative swale LID.
//   Build 5.1.010:
//   - New modified version of Green-Ampt used for surface layer infiltration.
//   Build 5.1.011:
//   - Re-named STOR_INFIL to STOR_EXFIL and StorageInfil to StorageExfil to
//     better reflect their meaning.
//   - Evaporation rates from sub-surface layers reduced by fraction of 
//     surface that is pervious (applies to block paver systems)
//   - Flux rate routines for LIDs with underdrains modified to produce more
//     physically meaningful results.
//   - Reporting of detailed results re-written.
//   Build 5.1.012:
//   - Modified upper limit for soil layer percolation.
//   - Modified upper limit on surface infiltration into rain gardens.
//   - Modified upper limit on drain flow for LIDs with storage layers.
//   - Used re-defined wasDry variable for LID reports to fix duplicate lines.
//   Build 5.1.013:
//   - Support added for open/closed head levels and multiplier v. head curve
//     to control underdrain flow.
//   - Support added for regenerating pavement permeability at fixed intervals.
//   Build 5.1.014:
//   - Fixed failure to initialize all LID layer moisture volumes to 0 before
//     computing LID unit performance in lidproc_getOutflow.
//   Build 5.2.0:
//   - Fixed failure to account for effect of Impervious Surface Fraction on
//     pavement permeability for Permeable Pavement LID
//   - Fixed units conversion for pavement depth in detailed report file.
//   Build 5.2.4:
//   - Modified flux limits in biocellFluxRates, pavementFluxRates and
//     trenchFluxRates.
//   - Corrected head calculation in getStorageDrainRate when unit has both
//     a soil and pavement layer.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lid.h"
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
#define STOPTOL  0.00328     // integration error tolerance in ft (= 1 mm)
#define MINFLOW  2.3e-8      // flow cutoff for dry conditions (= 0.001 in/hr)

//-----------------------------------------------------------------------------
//  Enumerations
//-----------------------------------------------------------------------------
enum LidLayerTypes {
    SURF,                    // surface layer
    SOIL,                    // soil layer
    STOR,                    // storage layer
    PAVE,                    // pavement layer
    DRAIN};                  // underdrain system

enum LidRptVars {
    SURF_INFLOW,             // inflow to surface layer
    TOTAL_EVAP,              // evaporation rate from all layers
    SURF_INFIL,              // infiltration into surface layer
    PAVE_PERC,               // percolation through pavement layer
    SOIL_PERC,               // percolation through soil layer
    STOR_EXFIL,              // exfiltration out of storage layer
    SURF_OUTFLOW,            // outflow from surface layer
    STOR_DRAIN,              // outflow from storage layer
    SURF_DEPTH,              // ponded depth on surface layer
    PAVE_DEPTH,              // water level in pavement layer
    SOIL_MOIST,              // moisture content of soil layer
    STOR_DEPTH,              // water level in storage layer
    MAX_RPT_VARS};

//-----------------------------------------------------------------------------
//  Imported variables 
//-----------------------------------------------------------------------------
      // TRUE if any LIDs are wet (declared in runoff.c)

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
     // ptr. to a subcatchment's LID unit
     // ptr. to a LID process

          // current time step (sec)
       // evaporation rate (ft/s)
 // native soil infil. rate limit (ft/s)

  // precip. + runon to LID unit (ft/s)
   // infil. rate from surface layer (ft/s)
    // evap. rate from surface layer (ft/s)
 // outflow from surface layer (ft/s)
  // volume in surface storage (ft)

       // evap. from pavement layer (ft/s)
       // percolation from pavement layer (ft/s)
     // volume stored in pavement layer  (ft)

       // evap. from soil layer (ft/s)
       // percolation from soil layer (ft/s)
     // volume in soil/pavement storage (ft)

  // inflow rate to storage layer (ft/s)
   // exfil. rate from storage layer (ft/s)
    // evap.rate from storage layer (ft/s)
   // underdrain flow rate layer (ft/s)
  // volume in storage layer (ft)


// ---- lidproc module state: formerly file-scope variables of lidproc.c.
// ---- One instance per Project, so two models never share it.
struct LidprocState
{
    TLidUnit * theLidUnit{};   // was static
    TLidProc * theLidProc{};   // was static
    double Tstep{};   // was static
    double EvapRate{};   // was static
    double MaxNativeInfil{};   // was static
    double SurfaceInflow{};   // was static
    double SurfaceInfil{};   // was static
    double SurfaceEvap{};   // was static
    double SurfaceOutflow{};   // was static
    double SurfaceVolume{};   // was static
    double PaveEvap{};   // was static
    double PavePerc{};   // was static
    double PaveVolume{};   // was static
    double SoilEvap{};   // was static
    double SoilPerc{};   // was static
    double SoilVolume{};   // was static
    double StorageInflow{};   // was static
    double StorageExfil{};   // was static
    double StorageEvap{};   // was static
    double StorageDrain{};   // was static
    double StorageVolume{};   // was static
    double Xold[4]{};   // was static
};
void Project::lidproc_createState() { lidproc_ = new LidprocState(); }
void Project::lidproc_destroyState() { delete lidproc_; lidproc_ = nullptr; }
  // previous moisture level in LID layers

//-----------------------------------------------------------------------------
//  External Functions (declared in lid.h)
//-----------------------------------------------------------------------------
// lidproc_initWaterBalance  (called by lid_initState)
// lidproc_getOutflow        (called by evalLidUnit in lid.c)
// lidproc_saveResults       (called by evalLidUnit in lid.c)

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------





















//=============================================================================

void Project::lidproc_initWaterBalance(TLidUnit *lidUnit, double initVol)
//
//  Purpose: initializes the water balance components of a LID unit.
//  Input:   lidUnit = a particular LID unit
//           initVol = initial water volume stored in the unit (ft)
//  Output:  none
//
{
    lidUnit->waterBalance.inflow = 0.0;
    lidUnit->waterBalance.evap = 0.0;
    lidUnit->waterBalance.infil = 0.0;
    lidUnit->waterBalance.surfFlow = 0.0;
    lidUnit->waterBalance.drainFlow = 0.0;
    lidUnit->waterBalance.initVol = initVol;
    lidUnit->waterBalance.finalVol = initVol;
}

//=============================================================================

double Project::lidproc_getOutflow(TLidUnit* lidUnit, TLidProc* lidProc, double inflow,
                          double evap, double infil, double maxInfil,
                          double tStep, double* lidEvap,
                          double* lidInfil, double* lidDrain)
//
//  Purpose: computes runoff outflow from a single LID unit.
//  Input:   lidUnit  = ptr. to specific LID unit being analyzed
//           lidProc  = ptr. to generic LID process of the LID unit
//           inflow   = runoff rate captured by LID unit (ft/s)
//           evap     = potential evaporation rate (ft/s)
//           infil    = infiltration rate to native soil (ft/s)
//           maxInfil = max. infiltration rate to native soil (ft/s)
//           tStep    = time step (sec)
//  Output:  lidEvap  = evaporation rate for LID unit (ft/s)
//           lidInfil = infiltration rate for LID unit (ft/s)
//           lidDrain = drain flow for LID unit (ft/s)
//           returns surface runoff rate from the LID unit (ft/s)
//
{
    int    i;
    double x[MAX_LAYERS];        // layer moisture levels
    double xOld[MAX_LAYERS];     // work vector
    double xPrev[MAX_LAYERS];    // work vector
    double xMin[MAX_LAYERS];     // lower limit on moisture levels
    double xMax[MAX_LAYERS];     // upper limit on moisture levels
    double fOld[MAX_LAYERS];     // previously computed flux rates
    double f[MAX_LAYERS];        // newly computed flux rates

    // convergence tolerance on moisture levels (ft, moisture fraction , ft)
    double xTol[MAX_LAYERS] = {STOPTOL, STOPTOL, STOPTOL, STOPTOL};

    double omega = 0.0;          // integration time weighting

    //... define a pointer to function that computes flux rates through the LID
    void (Project::*fluxRates) (double *, double *) = NULL;

    //... save references to the LID process and LID unit
    lidproc_->theLidProc = lidProc;
    lidproc_->theLidUnit = lidUnit;

    //... save evap, max. infil. & time step to shared variables
    lidproc_->EvapRate = evap;
    lidproc_->MaxNativeInfil = maxInfil;
    lidproc_->Tstep = tStep;

    //... store current moisture levels in vector x
    x[SURF] = lidproc_->theLidUnit->surfaceDepth;
    x[SOIL] = lidproc_->theLidUnit->soilMoisture;
    x[STOR] = lidproc_->theLidUnit->storageDepth;
    x[PAVE] = lidproc_->theLidUnit->paveDepth;

    //... initialize layer moisture volumes, flux rates and moisture limits
    lidproc_->SurfaceVolume  = 0.0;
    lidproc_->PaveVolume     = 0.0;
    lidproc_->SoilVolume     = 0.0;
    lidproc_->StorageVolume  = 0.0;
    lidproc_->SurfaceInflow  = inflow;
    lidproc_->SurfaceInfil   = 0.0;
    lidproc_->SurfaceEvap    = 0.0;
    lidproc_->SurfaceOutflow = 0.0;
    lidproc_->PaveEvap       = 0.0;
    lidproc_->PavePerc       = 0.0;
    lidproc_->SoilEvap       = 0.0;
    lidproc_->SoilPerc       = 0.0;
    lidproc_->StorageInflow  = 0.0;
    lidproc_->StorageExfil   = 0.0;
    lidproc_->StorageEvap    = 0.0;
    lidproc_->StorageDrain   = 0.0;
    for (i = 0; i < MAX_LAYERS; i++)
    {
        f[i] = 0.0;
        fOld[i] = lidproc_->theLidUnit->oldFluxRates[i];
        xMin[i] = 0.0;
        xMax[i] = BIG;
        lidproc_->Xold[i] = x[i];
    }

    //... find Green-Ampt infiltration from surface layer
    if ( lidproc_->theLidProc->lidType == POROUS_PAVEMENT ) lidproc_->SurfaceInfil = 0.0;
    else if ( lidproc_->theLidUnit->soilInfil.Ks > 0.0 )
    {
        lidproc_->SurfaceInfil =
            grnampt_getInfil(&lidproc_->theLidUnit->soilInfil, lidproc_->Tstep,
                             lidproc_->SurfaceInflow, lidproc_->theLidUnit->surfaceDepth,
                             MOD_GREEN_AMPT);
    }
    else lidproc_->SurfaceInfil = infil;

    //... set moisture limits for soil & storage layers
    if ( lidproc_->theLidProc->soil.thickness > 0.0 )
    {
        xMin[SOIL] = lidproc_->theLidProc->soil.wiltPoint;
        xMax[SOIL] = lidproc_->theLidProc->soil.porosity;
    }
    if ( lidproc_->theLidProc->pavement.thickness > 0.0 )
    {
        xMax[PAVE] = lidproc_->theLidProc->pavement.thickness;
    }
    if ( lidproc_->theLidProc->storage.thickness > 0.0 )
    {
        xMax[STOR] = lidproc_->theLidProc->storage.thickness;
    }
    if ( lidproc_->theLidProc->lidType == GREEN_ROOF )
    {
        xMax[STOR] = lidproc_->theLidProc->drainMat.thickness;
    }

    //... determine which flux rate function to use
    switch (lidproc_->theLidProc->lidType)
    {
    case BIO_CELL:
    case RAIN_GARDEN:     fluxRates = &Project::biocellFluxRates;   break;
    case GREEN_ROOF:      fluxRates = &Project::greenRoofFluxRates; break;
    case INFIL_TRENCH:    fluxRates = &Project::trenchFluxRates;    break;
    case POROUS_PAVEMENT: fluxRates = &Project::pavementFluxRates;  break;
    case RAIN_BARREL:     fluxRates = &Project::barrelFluxRates;    break;
    case ROOF_DISCON:     fluxRates = &Project::roofFluxRates;      break;
    case VEG_SWALE:       fluxRates = &Project::swaleFluxRates;
                          omega = 0.5;
                          break;
    default:              return 0.0;
    }

    //... update moisture levels and flux rates over the time step
    i = modpuls_solve(MAX_LAYERS, x, xOld, xPrev, xMin, xMax, xTol,
                     fOld, f, tStep, omega, fluxRates);

/** For debugging only ********************************************
    if  (i == 0)
    {
        fprintf(Frpt.file,
        "\n  WARNING 09: integration failed to converge at %s %s",
            theDate, theTime);
        fprintf(Frpt.file,
        "\n              for LID %s placed in subcatchment %s.",
            theLidProc->ID, theSubcatch->ID);
    }
*******************************************************************/

    //... add any surface overflow to surface outflow
    if ( lidproc_->theLidProc->surface.canOverflow || lidproc_->theLidUnit->fullWidth == 0.0 )
    {
        lidproc_->SurfaceOutflow += getSurfaceOverflowRate(&x[SURF]);
    }

    //... save updated results
    lidproc_->theLidUnit->surfaceDepth = x[SURF];
    lidproc_->theLidUnit->paveDepth    = x[PAVE];
    lidproc_->theLidUnit->soilMoisture = x[SOIL];
    lidproc_->theLidUnit->storageDepth = x[STOR];
    for (i = 0; i < MAX_LAYERS; i++) lidproc_->theLidUnit->oldFluxRates[i] = f[i];

    //... assign values to LID unit evaporation, infiltration & drain flow
    *lidEvap = lidproc_->SurfaceEvap + lidproc_->PaveEvap + lidproc_->SoilEvap + lidproc_->StorageEvap;
    *lidInfil = lidproc_->StorageExfil;
    *lidDrain = lidproc_->StorageDrain;

    //... return surface outflow (per unit area) from unit
    return lidproc_->SurfaceOutflow;
}

//=============================================================================

void Project::lidproc_saveResults(TLidUnit* lidUnit, double ucfRainfall, double ucfRainDepth)
//
//  Purpose: updates the mass balance for an LID unit and saves
//           current flux rates to the LID report file.
//  Input:   lidUnit = ptr. to LID unit
//           ucfRainfall = units conversion factor for rainfall rate
//           ucfDepth = units conversion factor for rainfall depth
//  Output:  none
//
{
    double ucf;                        // units conversion factor
    double totalEvap;                  // total evaporation rate (ft/s)
    double totalVolume;                // total volume stored in LID (ft)
    double rptVars[MAX_RPT_VARS];      // array of reporting variables
    int    isDry = FALSE;              // true if current state of LID is dry
    char   timeStamp[TIME_STAMP_SIZE + 1]; // date/time stamp
    double elapsedHrs;                 // elapsed hours

    //... find total evap. rate and stored volume
    totalEvap = lidproc_->SurfaceEvap + lidproc_->PaveEvap + lidproc_->SoilEvap + lidproc_->StorageEvap; 
    totalVolume = lidproc_->SurfaceVolume + lidproc_->PaveVolume + lidproc_->SoilVolume + lidproc_->StorageVolume;

    //... update mass balance totals
    updateWaterBalance(lidproc_->theLidUnit, lidproc_->SurfaceInflow, totalEvap, lidproc_->StorageExfil,
                       lidproc_->SurfaceOutflow, lidproc_->StorageDrain, totalVolume);

    //... check if dry-weather conditions hold
    if ( lidproc_->SurfaceInflow  < MINFLOW &&
         lidproc_->SurfaceOutflow < MINFLOW &&
         lidproc_->StorageDrain   < MINFLOW &&
         lidproc_->StorageExfil   < MINFLOW &&
         totalEvap      < MINFLOW
       ) isDry = TRUE;

    //... update status of HasWetLids
    if ( !isDry ) HasWetLids = TRUE;

    //... write results to LID report file
    if ( lidUnit->rptFile )
    {
        //... convert rate results to original units (in/hr or mm/hr)
        ucf = ucfRainfall;
        rptVars[SURF_INFLOW]  = lidproc_->SurfaceInflow*ucf;
        rptVars[TOTAL_EVAP]   = totalEvap*ucf;
        rptVars[SURF_INFIL]   = lidproc_->SurfaceInfil*ucf;
        rptVars[PAVE_PERC]    = lidproc_->PavePerc*ucf;
        rptVars[SOIL_PERC]    = lidproc_->SoilPerc*ucf;
        rptVars[STOR_EXFIL]   = lidproc_->StorageExfil*ucf;
        rptVars[SURF_OUTFLOW] = lidproc_->SurfaceOutflow*ucf;
        rptVars[STOR_DRAIN]   = lidproc_->StorageDrain*ucf;

        //... convert storage results to original units (in or mm)
        ucf = ucfRainDepth;
        rptVars[SURF_DEPTH] = lidproc_->theLidUnit->surfaceDepth*ucf;
        rptVars[PAVE_DEPTH] = lidproc_->theLidUnit->paveDepth*ucf;
        rptVars[SOIL_MOIST] = lidproc_->theLidUnit->soilMoisture;
        rptVars[STOR_DEPTH] = lidproc_->theLidUnit->storageDepth*ucf;

        //... if the current LID state is wet but the previous state was dry
        //    for more than one period then write the saved previous results
        //    to the report file thus marking the end of a dry period
        if ( !isDry && lidproc_->theLidUnit->rptFile->wasDry > 1)
        {
            fprintf(lidproc_->theLidUnit->rptFile->file, "%s",
                lidproc_->theLidUnit->rptFile->results);
        }

        //... write the current results to a string which is saved between
        //    reporting periods
        elapsedHrs = NewRunoffTime / 1000.0 / 3600.0;
        datetime_getTimeStamp(
            M_D_Y, getDateTime(NewRunoffTime), TIME_STAMP_SIZE, timeStamp);
        snprintf(lidproc_->theLidUnit->rptFile->results, sizeof(lidproc_->theLidUnit->rptFile->results),
             "\n%20s\t %8.3f\t %8.3f\t %8.4f\t %8.3f\t %8.3f\t %8.3f\t %8.3f\t"
             "%8.3f\t %8.3f\t %8.3f\t %8.3f\t %8.3f\t %8.3f",
             timeStamp, elapsedHrs, rptVars[0], rptVars[1], rptVars[2],
             rptVars[3], rptVars[4], rptVars[5], rptVars[6], rptVars[7],
             rptVars[8], rptVars[9], rptVars[10], rptVars[11]);

        //... if the current LID state is dry
        if ( isDry )
        {
            //... if the previous state was wet then write the current
            //    results to file marking the start of a dry period
            if ( lidproc_->theLidUnit->rptFile->wasDry == 0 )
            {
                fprintf(lidproc_->theLidUnit->rptFile->file, "%s",
                    lidproc_->theLidUnit->rptFile->results);
            }

            //... increment the number of successive dry periods
            lidproc_->theLidUnit->rptFile->wasDry++;
        }

        //... if the current LID state is wet
        else
        {
            //... write the current results to the report file
            fprintf(lidproc_->theLidUnit->rptFile->file, "%s",
                lidproc_->theLidUnit->rptFile->results);

            //... re-set the number of successive dry periods to 0
            lidproc_->theLidUnit->rptFile->wasDry = 0; 
        }
    }
}

//=============================================================================

void Project::roofFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates for roof disconnection.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double surfaceDepth = x[SURF];

    lidproc_getEvapRates(surfaceDepth, 0.0, 0.0, 0.0, 1.0);
    lidproc_->SurfaceVolume = surfaceDepth;
    lidproc_->SurfaceInfil = 0.0;
    if ( lidproc_->theLidProc->surface.alpha > 0.0 )
      lidproc_->SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);
    else getSurfaceOverflowRate(&surfaceDepth);
    lidproc_->StorageDrain = MIN(lidproc_->theLidProc->drain.coeff/UCF(RAINFALL), lidproc_->SurfaceOutflow);
    lidproc_->SurfaceOutflow -= lidproc_->StorageDrain;
    f[SURF] = (lidproc_->SurfaceInflow - lidproc_->SurfaceEvap - lidproc_->StorageDrain - lidproc_->SurfaceOutflow);
}

//=============================================================================

void Project::greenRoofFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates from the layers of a green roof.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double soilTheta;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // Green roof properties
    double soilThickness    = lidproc_->theLidProc->soil.thickness;
    double storageThickness = lidproc_->theLidProc->storage.thickness;
    double soilPorosity     = lidproc_->theLidProc->soil.porosity;
    double storageVoidFrac  = lidproc_->theLidProc->storage.voidFrac;
    double soilFieldCap     = lidproc_->theLidProc->soil.fieldCap;
    double soilWiltPoint    = lidproc_->theLidProc->soil.wiltPoint;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    lidproc_->SurfaceVolume = surfaceDepth * lidproc_->theLidProc->surface.voidFrac;
    lidproc_->SoilVolume = soilTheta * soilThickness;
    lidproc_->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = lidproc_->SoilVolume - soilWiltPoint * soilThickness;
    lidproc_getEvapRates(lidproc_->SurfaceVolume, 0.0, availVolume, lidproc_->StorageVolume, 1.0);
    if ( soilTheta >= soilPorosity ) lidproc_->StorageEvap = 0.0;

    //... soil layer perc rate
    lidproc_->SoilPerc = getSoilPercRate(soilTheta);

    //... limit perc rate by available water
    availVolume = (soilTheta - soilFieldCap) * soilThickness;
    maxRate = MAX(availVolume, 0.0) / lidproc_->Tstep - lidproc_->SoilEvap;
    lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);
    lidproc_->SoilPerc = MAX(lidproc_->SoilPerc, 0.0);

    //... storage (drain mat) outflow rate
    lidproc_->StorageExfil = 0.0;
    lidproc_->StorageDrain = getDrainMatOutflow(storageDepth);

    //... unit is full
    if ( soilTheta >= soilPorosity && storageDepth >= storageThickness )
    {
        //... outflow from both layers equals limiting rate
        maxRate = MIN(lidproc_->SoilPerc, lidproc_->StorageDrain);
        lidproc_->SoilPerc = maxRate;
        lidproc_->StorageDrain = maxRate;

        //... adjust inflow rate to soil layer
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
    }

    //... unit not full
    else
    {
        //... limit drainmat outflow by available storage volume
        maxRate = storageDepth * storageVoidFrac / lidproc_->Tstep - lidproc_->StorageEvap;
        if ( storageDepth >= storageThickness ) maxRate += lidproc_->SoilPerc;
        maxRate = MAX(maxRate, 0.0);
        lidproc_->StorageDrain = MIN(lidproc_->StorageDrain, maxRate);

        //... limit soil perc inflow by unused storage volume
        maxRate = (storageThickness - storageDepth) * storageVoidFrac / lidproc_->Tstep +
                  lidproc_->StorageDrain + lidproc_->StorageEvap;
        lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);
                
        //... adjust surface infil. so soil porosity not exceeded
        maxRate = (soilPorosity - soilTheta) * soilThickness / lidproc_->Tstep +
                  lidproc_->SoilPerc + lidproc_->SoilEvap;
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
    }

    // ... find surface outflow rate
    lidproc_->SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);

    // ... compute overall layer flux rates
    f[SURF] = (lidproc_->SurfaceInflow - lidproc_->SurfaceEvap - lidproc_->SurfaceInfil - lidproc_->SurfaceOutflow) /
              lidproc_->theLidProc->surface.voidFrac;
    f[SOIL] = (lidproc_->SurfaceInfil - lidproc_->SoilEvap - lidproc_->SoilPerc) /
              lidproc_->theLidProc->soil.thickness;
    f[STOR] = (lidproc_->SoilPerc - lidproc_->StorageEvap - lidproc_->StorageDrain) /
              lidproc_->theLidProc->storage.voidFrac;
}

//=============================================================================

void Project::biocellFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates from the layers of a bio-retention cell LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double soilTheta;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // LID layer properties
    double soilThickness    = lidproc_->theLidProc->soil.thickness;
    double soilPorosity     = lidproc_->theLidProc->soil.porosity;
    double soilFieldCap     = lidproc_->theLidProc->soil.fieldCap;
    double soilWiltPoint    = lidproc_->theLidProc->soil.wiltPoint;
    double storageThickness = lidproc_->theLidProc->storage.thickness;
    double storageVoidFrac  = lidproc_->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    lidproc_->SurfaceVolume = surfaceDepth * lidproc_->theLidProc->surface.voidFrac;
    lidproc_->SoilVolume    = soilTheta * soilThickness;
    lidproc_->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = lidproc_->SoilVolume - soilWiltPoint * soilThickness;
    lidproc_getEvapRates(lidproc_->SurfaceVolume, 0.0, availVolume, lidproc_->StorageVolume, 1.0);
    if ( soilTheta >= soilPorosity ) lidproc_->StorageEvap = 0.0;

    //... soil layer perc rate
    lidproc_->SoilPerc = getSoilPercRate(soilTheta);

    //... limit perc rate by available water
    availVolume =  (soilTheta - soilFieldCap) * soilThickness;
    maxRate = MAX(availVolume, 0.0) / lidproc_->Tstep - lidproc_->SoilEvap;
    lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);
    lidproc_->SoilPerc = MAX(lidproc_->SoilPerc, 0.0);

    //... exfiltration rate out of storage layer
    lidproc_->StorageExfil = getStorageExfilRate();

    //... underdrain flow rate
    lidproc_->StorageDrain = 0.0;
    if ( lidproc_->theLidProc->drain.coeff > 0.0 )
    {
        lidproc_->StorageDrain = getStorageDrainRate(storageDepth, soilTheta, 0.0,
                                           surfaceDepth);
    }

    //... special case of no storage layer present
    if ( storageThickness == 0.0 )
    {
        lidproc_->StorageEvap = 0.0;
        maxRate = MIN(lidproc_->SoilPerc, lidproc_->StorageExfil);
        lidproc_->SoilPerc = maxRate;
        lidproc_->StorageExfil = maxRate;

        //... limit surface infil. by unused soil volume
        maxRate = (soilPorosity - soilTheta) * soilThickness / lidproc_->Tstep +
                  lidproc_->SoilPerc + lidproc_->SoilEvap;
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
    }

    else
    {
        //... storage & soil layers are full
        if ( soilTheta >= soilPorosity && storageDepth >= storageThickness )
        {
            //... limiting rate is smaller of soil perc and storage outflow
            maxRate = lidproc_->StorageExfil + lidproc_->StorageDrain;
            if ( lidproc_->SoilPerc < maxRate )
            {
                maxRate = lidproc_->SoilPerc;
                if ( maxRate > lidproc_->StorageExfil ) lidproc_->StorageDrain = maxRate - lidproc_->StorageExfil;
                else
                {
                    lidproc_->StorageExfil = maxRate;
                    lidproc_->StorageDrain = 0.0;
                }
            }
            else lidproc_->SoilPerc = maxRate;

            //... apply limiting rate to surface infil.
            lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
        }

        //... either layer not full
        else
        {
            //... limit storage exfiltration by available storage volume
            maxRate = lidproc_->SoilPerc - lidproc_->StorageEvap + storageDepth*storageVoidFrac/lidproc_->Tstep;
            lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, maxRate);
            lidproc_->StorageExfil = MAX(lidproc_->StorageExfil, 0.0);

            //... limit underdrain flow by volume above drain offset
            if ( lidproc_->StorageDrain > 0.0 )
            {
                maxRate = -lidproc_->StorageExfil - lidproc_->StorageEvap;
                if ( storageDepth >= storageThickness) maxRate += lidproc_->SoilPerc;
                if ( lidproc_->theLidProc->drain.offset <= storageDepth )
                {
                    maxRate += (storageDepth - lidproc_->theLidProc->drain.offset) *
                               storageVoidFrac/lidproc_->Tstep;
                }
                maxRate = MAX(maxRate, 0.0);
                lidproc_->StorageDrain = MIN(lidproc_->StorageDrain, maxRate);
            }
        
            //... limit soil perc by unused storage volume
            maxRate = lidproc_->StorageExfil + lidproc_->StorageDrain + lidproc_->StorageEvap +
                      (storageThickness - storageDepth) *
                      storageVoidFrac/lidproc_->Tstep;
            lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);

            //... limit surface infil. by unused soil volume
            maxRate = (soilPorosity - soilTheta) * soilThickness / lidproc_->Tstep +
                      lidproc_->SoilPerc + lidproc_->SoilEvap;
            lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
        }
    }
    
    //... find surface layer outflow rate
    lidproc_->SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);

    //... compute overall layer flux rates
    f[SURF] = (lidproc_->SurfaceInflow - lidproc_->SurfaceEvap - lidproc_->SurfaceInfil - lidproc_->SurfaceOutflow) /
              lidproc_->theLidProc->surface.voidFrac;
    f[SOIL] = (lidproc_->SurfaceInfil - lidproc_->SoilEvap - lidproc_->SoilPerc) / 
              lidproc_->theLidProc->soil.thickness;
    if ( storageThickness == 0.0 ) f[STOR] = 0.0;
    else f[STOR] = (lidproc_->SoilPerc - lidproc_->StorageEvap - lidproc_->StorageExfil - lidproc_->StorageDrain) /
                   lidproc_->theLidProc->storage.voidFrac;
}

//=============================================================================

void Project::trenchFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates from the layers of an infiltration trench LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    // Moisture level variables
    double surfaceDepth;
    double storageDepth;

    // Intermediate variables
    double availVolume;
    double maxRate;

    // Storage layer properties
    double storageThickness = lidproc_->theLidProc->storage.thickness;
    double storageVoidFrac = lidproc_->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    lidproc_->SurfaceVolume = surfaceDepth * lidproc_->theLidProc->surface.voidFrac;
    lidproc_->SoilVolume = 0.0;
    lidproc_->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = (storageThickness - storageDepth) * storageVoidFrac;
    lidproc_getEvapRates(lidproc_->SurfaceVolume, 0.0, 0.0, lidproc_->StorageVolume, 1.0);

    //... no storage evap if surface ponded
    if ( surfaceDepth > 0.0 ) lidproc_->StorageEvap = 0.0;

    //... nominal storage inflow
    lidproc_->StorageInflow = lidproc_->SurfaceInflow + lidproc_->SurfaceVolume / lidproc_->Tstep;

    //... exfiltration rate out of storage layer
   lidproc_->StorageExfil = getStorageExfilRate();

    //... underdrain flow rate
    lidproc_->StorageDrain = 0.0;
    if ( lidproc_->theLidProc->drain.coeff > 0.0 )
    {
        lidproc_->StorageDrain = getStorageDrainRate(storageDepth, 0.0, 0.0, surfaceDepth);
    }

    //... limit storage exfiltration by available storage volume
    maxRate = lidproc_->StorageInflow - lidproc_->StorageEvap + storageDepth*storageVoidFrac/lidproc_->Tstep;
    lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, maxRate);
    lidproc_->StorageExfil = MAX(lidproc_->StorageExfil, 0.0);

    //... limit underdrain flow by volume above drain offset
    if ( lidproc_->StorageDrain > 0.0 )
    {
        maxRate = -lidproc_->StorageExfil - lidproc_->StorageEvap;
        if (storageDepth >= storageThickness ) maxRate += lidproc_->StorageInflow;
        if ( lidproc_->theLidProc->drain.offset <= storageDepth )
        {
            maxRate += (storageDepth - lidproc_->theLidProc->drain.offset) *
                       storageVoidFrac/lidproc_->Tstep;
        }
        maxRate = MAX(maxRate, 0.0);
        lidproc_->StorageDrain = MIN(lidproc_->StorageDrain, maxRate);
    }

    //... limit storage inflow to not exceed storage layer capacity
    maxRate = (storageThickness - storageDepth)*storageVoidFrac/lidproc_->Tstep +
              lidproc_->StorageExfil + lidproc_->StorageEvap + lidproc_->StorageDrain;
    lidproc_->StorageInflow = MIN(lidproc_->StorageInflow, maxRate);

    //... equate surface infil to storage inflow
    lidproc_->SurfaceInfil = lidproc_->StorageInflow;

    //... find surface outflow rate
    lidproc_->SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);

    // ... find net fluxes for each layer
    f[SURF] = (lidproc_->SurfaceInflow - lidproc_->SurfaceEvap - lidproc_->StorageInflow - lidproc_->SurfaceOutflow) /
              lidproc_->theLidProc->surface.voidFrac;;
    f[STOR] = (lidproc_->StorageInflow - lidproc_->StorageEvap - lidproc_->StorageExfil - lidproc_->StorageDrain) /
              lidproc_->theLidProc->storage.voidFrac;
    f[SOIL] = 0.0;
}

//=============================================================================

void Project::pavementFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates for the layers of a porous pavement LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    //... Moisture level variables
    double surfaceDepth;
    double paveDepth;
    double soilTheta;
    double storageDepth;

    //... Intermediate variables
    double pervFrac = (1.0 - lidproc_->theLidProc->pavement.impervFrac);
    double storageInflow;    // inflow rate to storage layer (ft/s)
    double availVolume;
    double maxRate;

    //... LID layer properties
    double paveVoidFrac     = lidproc_->theLidProc->pavement.voidFrac * pervFrac;
    double paveThickness    = lidproc_->theLidProc->pavement.thickness;
    double soilThickness    = lidproc_->theLidProc->soil.thickness;
    double soilPorosity     = lidproc_->theLidProc->soil.porosity;
    double soilFieldCap     = lidproc_->theLidProc->soil.fieldCap;
    double soilWiltPoint    = lidproc_->theLidProc->soil.wiltPoint;
    double storageThickness = lidproc_->theLidProc->storage.thickness;
    double storageVoidFrac  = lidproc_->theLidProc->storage.voidFrac;

    //... retrieve moisture levels from input vector
    surfaceDepth = x[SURF];
    paveDepth    = x[PAVE];
    soilTheta    = x[SOIL];
    storageDepth = x[STOR];

    //... convert moisture levels to volumes
    lidproc_->SurfaceVolume = surfaceDepth * lidproc_->theLidProc->surface.voidFrac;
    lidproc_->PaveVolume = paveDepth * paveVoidFrac;
    lidproc_->SoilVolume = soilTheta * soilThickness;
    lidproc_->StorageVolume = storageDepth * storageVoidFrac;

    //... get ET rates
    availVolume = lidproc_->SoilVolume - soilWiltPoint * soilThickness;
    lidproc_getEvapRates(lidproc_->SurfaceVolume, lidproc_->PaveVolume, availVolume, lidproc_->StorageVolume,
                 pervFrac);

    //... no storage evap if soil or pavement layer saturated
    if ( paveDepth >= paveThickness ||
       ( soilThickness > 0.0 && soilTheta >= soilPorosity )
       ) lidproc_->StorageEvap = 0.0;

    //... find nominal rate of surface infiltration into pavement layer
    lidproc_->SurfaceInfil = lidproc_->SurfaceInflow + (lidproc_->SurfaceVolume / lidproc_->Tstep);

    //... find perc rate out of pavement layer
    lidproc_->PavePerc = getPavementPermRate() * pervFrac;

    //... surface infiltration can't exceed pavement permeability
    lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, lidproc_->PavePerc);

    //... limit pavement perc by available water
    maxRate = lidproc_->PaveVolume/lidproc_->Tstep + lidproc_->SurfaceInfil - lidproc_->PaveEvap;
    maxRate = MAX(maxRate, 0.0);
    lidproc_->PavePerc = MIN(lidproc_->PavePerc, maxRate);

    //... find soil layer perc rate
    if ( soilThickness > 0.0 )
    {
        lidproc_->SoilPerc = getSoilPercRate(soilTheta);
        availVolume = (soilTheta - soilFieldCap) * soilThickness;
        maxRate = MAX(availVolume, 0.0) / lidproc_->Tstep - lidproc_->SoilEvap;
        lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);
        lidproc_->SoilPerc = MAX(lidproc_->SoilPerc, 0.0);
    }
    else lidproc_->SoilPerc = lidproc_->PavePerc;

    //... exfiltration rate out of storage layer
    lidproc_->StorageExfil = getStorageExfilRate();

    //... underdrain flow rate
    lidproc_->StorageDrain = 0.0;
    if ( lidproc_->theLidProc->drain.coeff > 0.0 )
    {
        lidproc_->StorageDrain = getStorageDrainRate(storageDepth, soilTheta, paveDepth,
                                           surfaceDepth);
    }

    //... check for adjacent saturated layers

    //... no soil layer, pavement & storage layers are full
    if ( soilThickness == 0.0 &&
         storageDepth >= storageThickness &&
         paveDepth >= paveThickness )
    {
        //... pavement outflow can't exceed storage outflow
        maxRate = lidproc_->StorageEvap + lidproc_->StorageDrain + lidproc_->StorageExfil;
        if ( lidproc_->PavePerc > maxRate ) lidproc_->PavePerc = maxRate;

        //... storage outflow can't exceed pavement outflow
        else
        {
            //... use up available exfiltration capacity first
            lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, lidproc_->PavePerc);
            lidproc_->StorageDrain = lidproc_->PavePerc - lidproc_->StorageExfil;
        }

        //... set soil perc to pavement perc
        lidproc_->SoilPerc = lidproc_->PavePerc;

        //... limit surface infil. by pavement perc
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, lidproc_->PavePerc);
    }

    //... pavement, soil & storage layers are full
    else if ( soilThickness > 0 &&
              storageDepth >= storageThickness &&
              soilTheta >= soilPorosity &&
              paveDepth >= paveThickness )
    {
        //... find which layer has limiting flux rate
        maxRate = lidproc_->StorageExfil + lidproc_->StorageDrain;
        if ( lidproc_->SoilPerc < maxRate) maxRate = lidproc_->SoilPerc;
        else maxRate = MIN(maxRate, lidproc_->PavePerc);

        //... use up available storage exfiltration capacity first
        if ( maxRate > lidproc_->StorageExfil ) lidproc_->StorageDrain = maxRate - lidproc_->StorageExfil;
        else
        {
            lidproc_->StorageExfil = maxRate;
            lidproc_->StorageDrain = 0.0;
        }
        lidproc_->SoilPerc = maxRate;
        lidproc_->PavePerc = maxRate;

        //... limit surface infil. by pavement perc
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, lidproc_->PavePerc);
    }

    //... storage & soil layers are full
    else if ( soilThickness > 0.0 &&
              storageDepth >= storageThickness &&
              soilTheta >= soilPorosity )
    {
        //... soil perc can't exceed storage outflow
        maxRate = lidproc_->StorageDrain + lidproc_->StorageExfil;
        if ( lidproc_->SoilPerc > maxRate ) lidproc_->SoilPerc = maxRate;

        //... storage outflow can't exceed soil perc
        else
        {
            //... use up available exfiltration capacity first
            lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, lidproc_->SoilPerc);
            lidproc_->StorageDrain = lidproc_->SoilPerc - lidproc_->StorageExfil;
        }
        lidproc_->PavePerc = MIN(lidproc_->PavePerc, lidproc_->SoilPerc);        

        //... limit surface infil. by available pavement volume
        availVolume = (paveThickness - paveDepth) * paveVoidFrac;
        maxRate = availVolume / lidproc_->Tstep + lidproc_->PavePerc + lidproc_->PaveEvap;
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
    }

    //... soil and pavement layers are full
    else if ( soilThickness > 0.0 &&
              paveDepth >= paveThickness &&
              soilTheta >= soilPorosity )
    {
        lidproc_->PavePerc = MIN(lidproc_->PavePerc, lidproc_->SoilPerc);
        lidproc_->SoilPerc = lidproc_->PavePerc;
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil,lidproc_->PavePerc); 
        maxRate = MAX(lidproc_->StorageVolume / lidproc_->Tstep + lidproc_->SoilPerc - lidproc_->StorageEvap, 0.0);
	    lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, maxRate); 
    }

    //... no adjoining layers are full
    else
    {
        //... limit storage exfiltration by available storage volume
        //    (if no soil layer, SoilPerc is same as PavePerc)
        maxRate = lidproc_->SoilPerc - lidproc_->StorageEvap + lidproc_->StorageVolume / lidproc_->Tstep;
        maxRate = MAX(0.0, maxRate);
        lidproc_->StorageExfil = MIN(lidproc_->StorageExfil, maxRate);

        //... limit underdrain flow by volume above drain offset
        if ( lidproc_->StorageDrain > 0.0 )
        {
            maxRate = -lidproc_->StorageExfil - lidproc_->StorageEvap;
            if (storageDepth >= storageThickness ) maxRate += lidproc_->SoilPerc;
            if ( lidproc_->theLidProc->drain.offset <= storageDepth ) 
            {
                maxRate += (storageDepth - lidproc_->theLidProc->drain.offset) *
                           storageVoidFrac/lidproc_->Tstep;
            }
            maxRate = MAX(maxRate, 0.0);
            lidproc_->StorageDrain = MIN(lidproc_->StorageDrain, maxRate);
        }

        //... limit soil & pavement outflow by unused storage volume
        availVolume = (storageThickness - storageDepth) * storageVoidFrac;
        maxRate = availVolume/lidproc_->Tstep + lidproc_->StorageEvap + lidproc_->StorageDrain + lidproc_->StorageExfil;
        maxRate = MAX(maxRate, 0.0);
        if ( soilThickness > 0.0 )
        {
            lidproc_->SoilPerc = MIN(lidproc_->SoilPerc, maxRate);
            maxRate = (soilPorosity - soilTheta) * soilThickness / lidproc_->Tstep +
                      lidproc_->SoilPerc;
        }
        lidproc_->PavePerc = MIN(lidproc_->PavePerc, maxRate);

        //... limit surface infil. by available pavement volume
        availVolume = (paveThickness - paveDepth) * paveVoidFrac;
        maxRate = availVolume / lidproc_->Tstep + lidproc_->PavePerc + lidproc_->PaveEvap;
        lidproc_->SurfaceInfil = MIN(lidproc_->SurfaceInfil, maxRate);
    }

    //... surface outflow
    lidproc_->SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);

    //... compute overall layer flux rates
    f[SURF] = lidproc_->SurfaceInflow - lidproc_->SurfaceEvap - lidproc_->SurfaceInfil - lidproc_->SurfaceOutflow;
    f[PAVE] = (lidproc_->SurfaceInfil - lidproc_->PaveEvap - lidproc_->PavePerc) / paveVoidFrac;
    if ( lidproc_->theLidProc->soil.thickness > 0.0)
    {
        f[SOIL] = (lidproc_->PavePerc - lidproc_->SoilEvap - lidproc_->SoilPerc) / soilThickness;
        storageInflow = lidproc_->SoilPerc;
    }
    else
    {
        f[SOIL] = 0.0;
        storageInflow = lidproc_->PavePerc;
        lidproc_->SoilPerc = 0.0;
    }
    f[STOR] = (storageInflow - lidproc_->StorageEvap - lidproc_->StorageExfil - lidproc_->StorageDrain) /
              storageVoidFrac;
}

//=============================================================================

void Project::swaleFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates from a vegetative swale LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double depth;            // depth of surface water in swale (ft)
    double topWidth;         // top width of full swale (ft)
    double botWidth;         // bottom width of swale (ft)
    double length;           // length of swale (ft)
    double surfInflow;       // inflow rate to swale (cfs)
    double surfWidth;        // top width at current water depth (ft)
    double surfArea;         // surface area of current water depth (ft2)
    double flowArea;         // x-section flow area (ft2)
    double lidArea;          // surface area of full swale (ft2)
    double hydRadius;        // hydraulic radius for current depth (ft)
    double slope;            // slope of swale side wall (run/rise)
    double volume;           // swale volume at current water depth (ft3)
    double dVdT;             // change in volume w.r.t. time (cfs)
    double dStore;           // depression storage depth (ft)
    double xDepth;           // depth above depression storage (ft)

    //... retrieve state variable from work vector
    depth = x[SURF];
    depth = MIN(depth, lidproc_->theLidProc->surface.thickness);

    //... depression storage depth
    dStore = 0.0;

    //... get swale's bottom width
    //    (0.5 ft minimum to avoid numerical problems)
    slope = lidproc_->theLidProc->surface.sideSlope;
    topWidth = lidproc_->theLidUnit->fullWidth;
    topWidth = MAX(topWidth, 0.5);
    botWidth = topWidth - 2.0 * slope * lidproc_->theLidProc->surface.thickness;
    if ( botWidth < 0.5 )
    {
        botWidth = 0.5;
        slope = 0.5 * (topWidth - 0.5) / lidproc_->theLidProc->surface.thickness;
    }

    //... swale's length
    lidArea = lidproc_->theLidUnit->area;
    length = lidArea / topWidth;

    //... top width, surface area and flow area of current ponded depth
    surfWidth = botWidth + 2.0 * slope * depth;
    surfArea = length * surfWidth;
    flowArea = (depth * (botWidth + slope * depth)) *
               lidproc_->theLidProc->surface.voidFrac;

    //... wet volume and effective depth
    volume = length * flowArea;

    //... surface inflow into swale (cfs)
    surfInflow = lidproc_->SurfaceInflow * lidArea;

    //... ET rate in cfs
    lidproc_->SurfaceEvap = lidproc_->EvapRate * surfArea;
    lidproc_->SurfaceEvap = MIN(lidproc_->SurfaceEvap, volume/lidproc_->Tstep);

    //... infiltration rate to native soil in cfs
    lidproc_->StorageExfil = lidproc_->SurfaceInfil * surfArea;

    //... no surface outflow if depth below depression storage
    xDepth = depth - dStore;
    if ( xDepth <= ZERO ) lidproc_->SurfaceOutflow = 0.0;

    //... otherwise compute a surface outflow
    else
    {
        //... modify flow area to remove depression storage,
        flowArea -= (dStore * (botWidth + slope * dStore)) *
                     lidproc_->theLidProc->surface.voidFrac;
        if ( flowArea < ZERO ) lidproc_->SurfaceOutflow = 0.0;
        else
        {
            //... compute hydraulic radius
            botWidth = botWidth + 2.0 * dStore * slope;
            hydRadius = botWidth + 2.0 * xDepth * sqrt(1.0 + slope*slope);
            hydRadius = flowArea / hydRadius;

            //... use Manning Eqn. to find outflow rate in cfs
            lidproc_->SurfaceOutflow = lidproc_->theLidProc->surface.alpha * flowArea *
                             pow(hydRadius, 2./3.);
        }
    }

    //... net flux rate (dV/dt) in cfs
    dVdT = surfInflow - lidproc_->SurfaceEvap - lidproc_->StorageExfil - lidproc_->SurfaceOutflow;

    //... when full, any net positive inflow becomes spillage
    if ( depth == lidproc_->theLidProc->surface.thickness && dVdT > 0.0 )
    {
        lidproc_->SurfaceOutflow += dVdT;
        dVdT = 0.0;
    }

    //... convert flux rates to ft/s
    lidproc_->SurfaceEvap /= lidArea;
    lidproc_->StorageExfil /= lidArea;
    lidproc_->SurfaceOutflow /= lidArea;
    f[SURF] = dVdT / surfArea;
    f[SOIL] = 0.0;
    f[STOR] = 0.0;

    //... assign values to layer volumes
    lidproc_->SurfaceVolume = volume / lidArea;
    lidproc_->SoilVolume = 0.0;
    lidproc_->StorageVolume = 0.0;
}

//=============================================================================

void Project::barrelFluxRates(double x[], double f[])
//
//  Purpose: computes flux rates for a rain barrel LID.
//  Input:   x = vector of storage levels
//  Output:  f = vector of flux rates
//
{
    double storageDepth = x[STOR];
    double head;
    double maxValue;

    //... assign values to layer volumes
    lidproc_->SurfaceVolume = 0.0;
    lidproc_->SoilVolume = 0.0;
    lidproc_->StorageVolume = storageDepth;

    //... initialize flows
    lidproc_->SurfaceInfil = 0.0;
    lidproc_->SurfaceOutflow = 0.0;
    lidproc_->StorageDrain = 0.0;

    //... compute outflow if time since last rain exceeds drain delay
    //    (dryTime is updated in lid.evalLidUnit at each time step)
    if ( lidproc_->theLidProc->drain.delay == 0.0 ||
        lidproc_->theLidUnit->dryTime >= lidproc_->theLidProc->drain.delay )
    {
        head = storageDepth - lidproc_->theLidProc->drain.offset;
        if ( head > 0.0 )
        {
            lidproc_->StorageDrain = getStorageDrainRate(storageDepth, 0.0, 0.0, 0.0);
            maxValue = (head/lidproc_->Tstep);
            lidproc_->StorageDrain = MIN(lidproc_->StorageDrain, maxValue);
        }
    }

    //... limit inflow to available storage
    lidproc_->StorageInflow = lidproc_->SurfaceInflow;
    maxValue = (lidproc_->theLidProc->storage.thickness - storageDepth) / lidproc_->Tstep +
        lidproc_->StorageDrain;
    lidproc_->StorageInflow = MIN(lidproc_->StorageInflow, maxValue);
    lidproc_->SurfaceInfil = lidproc_->StorageInflow;

    //... assign values to layer flux rates
    f[SURF] = lidproc_->SurfaceInflow - lidproc_->StorageInflow;
    f[STOR] = lidproc_->StorageInflow - lidproc_->StorageDrain;
    f[SOIL] = 0.0;
}

//=============================================================================

double Project::getSurfaceOutflowRate(double depth)
//
//  Purpose: computes outflow rate from a LID's surface layer.
//  Input:   depth = depth of ponded water on surface layer (ft)
//  Output:  returns outflow from surface layer (ft/s)
//
//  Note: this function should not be applied to swales or rain barrels.
//
{
    double delta;
    double outflow;

    //... no outflow if ponded depth below storage depth
    delta = depth - lidproc_->theLidProc->surface.thickness;
    if ( delta < 0.0 ) return 0.0;

    //... compute outflow from overland flow Manning equation
    outflow = lidproc_->theLidProc->surface.alpha * pow(delta, 5.0/3.0) *
              lidproc_->theLidUnit->fullWidth / lidproc_->theLidUnit->area;
    outflow = MIN(outflow, delta / lidproc_->Tstep);
    return outflow;
}

//=============================================================================

double Project::getPavementPermRate()
//
//  Purpose: computes reduced permeability of a pavement layer due to
//           clogging.
//  Input:   none
//  Output:  returns the reduced permeability of the pavement layer (ft/s).
//
{
    double permReduction = 0.0;
    double clogFactor= lidproc_->theLidProc->pavement.clogFactor;
    double regenDays = lidproc_->theLidProc->pavement.regenDays;

    // ... find permeability reduction due to clogging     
    if ( clogFactor > 0.0 )
    {
        // ... see if permeability regeneration has occurred
        //     (regeneration is assumed to reduce the total
        //      volumetric loading that the pavement has received)
        if ( regenDays > 0.0 )
        {
            if ( OldRunoffTime / 1000.0 / SECperDAY >= lidproc_->theLidUnit->nextRegenDay )
            {
                // ... reduce total volume treated by degree of regeneration
                lidproc_->theLidUnit->volTreated *= 
                    (1.0 - lidproc_->theLidProc->pavement.regenDegree);

                // ... update next day that regenration occurs
                lidproc_->theLidUnit->nextRegenDay += regenDays;
            }
        }

        // ... find permeabiity reduction factor
        permReduction = lidproc_->theLidUnit->volTreated / clogFactor;
        permReduction = MIN(permReduction, 1.0);
    }

    // ... return the effective pavement permeability
    return lidproc_->theLidProc->pavement.kSat * (1.0 - permReduction);
}

//=============================================================================

double Project::getSoilPercRate(double theta)
//
//  Purpose: computes percolation rate of water through a LID's soil layer.
//  Input:   theta = moisture content (fraction)
//  Output:  returns percolation rate within soil layer (ft/s)
//
{
    double delta;            // moisture deficit

    // ... no percolation if soil moisture <= field capacity
    if ( theta <= lidproc_->theLidProc->soil.fieldCap ) return 0.0;

    // ... perc rate = unsaturated hydraulic conductivity
    delta = lidproc_->theLidProc->soil.porosity - theta;
    return lidproc_->theLidProc->soil.kSat * exp(-delta * lidproc_->theLidProc->soil.kSlope);

}

//=============================================================================

double Project::getStorageExfilRate()
//
//  Purpose: computes exfiltration rate from storage zone into
//           native soil beneath a LID.
//  Input:   depth = depth of water storage zone (ft)
//  Output:  returns infiltration rate (ft/s)
//
{
    double infil = 0.0;
    double clogFactor = 0.0;

    if ( lidproc_->theLidProc->storage.kSat == 0.0 ) return 0.0;
    if ( lidproc_->MaxNativeInfil == 0.0 ) return 0.0;

    //... reduction due to clogging
    clogFactor = lidproc_->theLidProc->storage.clogFactor;
    if ( clogFactor > 0.0 )
    {
        clogFactor = lidproc_->theLidUnit->waterBalance.inflow / clogFactor;
        clogFactor = MIN(clogFactor, 1.0);
    }

    //... infiltration rate = storage Ksat reduced by any clogging
    infil = lidproc_->theLidProc->storage.kSat * (1.0 - clogFactor);

    //... limit infiltration rate by any groundwater-imposed limit
    return MIN(infil, lidproc_->MaxNativeInfil);
}

//=============================================================================

double  Project::getStorageDrainRate(double storageDepth, double soilTheta, 
                            double paveDepth, double surfaceDepth)
//
//  Purpose: computes underdrain flow rate in a LID's storage layer.
//  Input:   storageDepth = depth of water in storage layer (ft)
//           soilTheta    = moisture content of soil layer
//           paveDepth    = effective depth of water in pavement layer (ft)
//           surfaceDepth = depth of ponded water on surface layer (ft)
//  Output:  returns flow in underdrain (ft/s)
//
//  Note:    drain eqn. is evaluated in user's units.
//  Note:    head on drain is water depth in storage layer plus the
//           layers above it (soil, pavement, and surface in that order)
//           minus the drain outlet offset.
{
    int    curve = lidproc_->theLidProc->drain.qCurve;
    double head = storageDepth;
    double outflow = 0.0;
    double paveThickness    = lidproc_->theLidProc->pavement.thickness;
    double soilThickness    = lidproc_->theLidProc->soil.thickness;
    double soilPorosity     = lidproc_->theLidProc->soil.porosity;
    double soilFieldCap     = lidproc_->theLidProc->soil.fieldCap;
    double storageThickness = lidproc_->theLidProc->storage.thickness;

    // --- storage layer is full
    if ( storageDepth >= storageThickness )
    {
        // --- a soil layer exists
        if ( soilThickness > 0.0 )
        {
            // --- increase head by fraction of soil layer saturated
            if ( soilTheta > soilFieldCap )
            {
                head += (soilTheta - soilFieldCap) /
                        (soilPorosity - soilFieldCap) * soilThickness;

                // --- soil layer is saturated, increase head by water
                //     depth in layer above it
                if ( soilTheta >= soilPorosity )
                {
                    if ( paveThickness > 0.0 )
                    {
                        head += paveDepth;
                        if ( paveDepth >= paveThickness ) head += surfaceDepth;
                    }    
                    else head += surfaceDepth;
                }
            }
        }

        // --- no soil layer so increase head by water level in pavement
        //     layer and possibly surface layer
        else if ( paveThickness > 0.0 )
        {
            head += paveDepth;
            if ( paveDepth >= paveThickness ) head += surfaceDepth;
        }
    }

    // --- no outflow if:
    //     a) no prior outflow and head below open threshold
    //     b) prior outflow and head below closed threshold
    if ( lidproc_->theLidUnit->oldDrainFlow == 0.0 &&
         head <= lidproc_->theLidProc->drain.hOpen ) return 0.0;
    if ( lidproc_->theLidUnit->oldDrainFlow > 0.0 &&
         head <= lidproc_->theLidProc->drain.hClose ) return 0.0;

    // --- make head relative to drain offset
    head -= lidproc_->theLidProc->drain.offset;

    // --- compute drain outflow from underdrain flow equation in user units
    //     (head in inches or mm, flow rate in in/hr or mm/hr)
    if ( head > ZERO )
    {
        // --- convert head to user units
        head *= UCF(RAINDEPTH);

        // --- compute drain outflow in user units
        outflow = lidproc_->theLidProc->drain.coeff *
                  pow(head, lidproc_->theLidProc->drain.expon);

        // --- apply user-supplied control curve to outflow
        if (curve >= 0)  outflow *= table_lookup(&Curve[curve], head);

        // --- convert outflow to ft/s
        outflow /= UCF(RAINFALL);
    }
    return outflow;
}

//=============================================================================

double Project::getDrainMatOutflow(double depth)
//
//  Purpose: computes flow rate through a green roof's drainage mat.
//  Input:   depth = depth of water in drainage mat (ft)
//  Output:  returns flow in drainage mat (ft/s)
//
{
    //... default is to pass all inflow
    double result = lidproc_->SoilPerc;

    //... otherwise use Manning eqn. if its parameters were supplied
    if ( lidproc_->theLidProc->drainMat.alpha > 0.0 )
    {
        result = lidproc_->theLidProc->drainMat.alpha * pow(depth, 5.0/3.0) *
                 lidproc_->theLidUnit->fullWidth / lidproc_->theLidUnit->area *
                 lidproc_->theLidProc->drainMat.voidFrac;
    }
    return result;
}

//=============================================================================

void Project::lidproc_getEvapRates(double surfaceVol, double paveVol, double soilVol,
    double storageVol, double pervFrac)
//
//  Purpose: computes surface, pavement, soil, and storage evaporation rates.
//  Input:   surfaceVol = volume/area of ponded water on surface layer (ft)
//           paveVol    = volume/area of water in pavement pores (ft)
//           soilVol    = volume/area of water in soil (or pavement) pores (ft)
//           storageVol = volume/area of water in storage layer (ft)
//           pervFrac   = fraction of surface layer that is pervious
//  Output:  none
//
{
    double availEvap;

    //... surface evaporation flux
    availEvap = lidproc_->EvapRate;
    lidproc_->SurfaceEvap = MIN(availEvap, surfaceVol/lidproc_->Tstep);
    lidproc_->SurfaceEvap = MAX(0.0, lidproc_->SurfaceEvap);
    availEvap = MAX(0.0, (availEvap - lidproc_->SurfaceEvap));
    availEvap *= pervFrac;

    //... no subsurface evap if water is infiltrating
    if ( lidproc_->SurfaceInfil > 0.0 )
    {
        lidproc_->PaveEvap = 0.0;
        lidproc_->SoilEvap = 0.0;
        lidproc_->StorageEvap = 0.0;
    }
    else
    {
        //... pavement evaporation flux
        lidproc_->PaveEvap = MIN(availEvap, paveVol / lidproc_->Tstep);
        availEvap = MAX(0.0, (availEvap - lidproc_->PaveEvap));

        //... soil evaporation flux
        lidproc_->SoilEvap = MIN(availEvap, soilVol / lidproc_->Tstep);
        availEvap = MAX(0.0, (availEvap - lidproc_->SoilEvap));

        //... storage evaporation flux
        lidproc_->StorageEvap = MIN(availEvap, storageVol / lidproc_->Tstep);
    }
}

//=============================================================================

double Project::getSurfaceOverflowRate(double* surfaceDepth)
//
//  Purpose: finds surface overflow rate from a LID unit.
//  Input:   surfaceDepth = depth of water stored in surface layer (ft)
//  Output:  returns the overflow rate (ft/s)
//
{
    double delta = *surfaceDepth - lidproc_->theLidProc->surface.thickness;
    if (  delta <= 0.0 ) return 0.0;
    *surfaceDepth = lidproc_->theLidProc->surface.thickness;
    return delta * lidproc_->theLidProc->surface.voidFrac / lidproc_->Tstep;
}

//=============================================================================

void Project::updateWaterBalance(TLidUnit *lidUnit, double inflow, double evap,
    double infil, double surfFlow, double drainFlow, double storage)
//
//  Purpose: updates components of the water mass balance for a LID unit
//           over the current time step.
//  Input:   lidUnit   = a particular LID unit
//           inflow    = runon + rainfall to the LID unit (ft/s)
//           evap      = evaporation rate from the unit (ft/s)
//           infil     = infiltration out the bottom of the unit (ft/s)
//           surfFlow  = surface runoff from the unit (ft/s)
//           drainFlow = underdrain flow from the unit
//           storage   = volume of water stored in the unit (ft)
//  Output:  none
//
{
    lidUnit->volTreated += inflow * lidproc_->Tstep;
    lidUnit->waterBalance.inflow += inflow * lidproc_->Tstep;
    lidUnit->waterBalance.evap += evap * lidproc_->Tstep;
    lidUnit->waterBalance.infil += infil * lidproc_->Tstep;
    lidUnit->waterBalance.surfFlow += surfFlow * lidproc_->Tstep;
    lidUnit->waterBalance.drainFlow += drainFlow * lidproc_->Tstep;
    lidUnit->waterBalance.finalVol = storage;
}

//=============================================================================

int Project::modpuls_solve(int n, double* x, double* xOld, double* xPrev,
                  double* xMin, double* xMax, double* xTol,
                  double* qOld, double* q, double dt, double omega,
                  void (Project::*derivs)(double*, double*))
//
//  Purpose: solves system of equations dx/dt = q(x) for x at end of time step
//           dt using a modified Puls method.
//  Input:   n = number of state variables
//           x = vector of state variables
//           xOld = state variable values at start of time step
//           xPrev = state variable values from previous iteration
//           xMin = lower limits on state variables
//           xMax = upper limits on state variables
//           xTol = convergence tolerances on state variables
//           qOld = flux rates at start of time step
//           q = flux rates at end of time step
//           dt = time step (sec)
//           omega = time weighting parameter (use 0 for Euler method
//                   or 0.5 for modified Puls method)
//           derivs = pointer to function that computes flux rates q as a
//                    function of state variables x
//  Output:  returns number of steps required for convergence (or 0 if
//           process doesn't converge)
//
{
    int i;
    int canStop;
    int steps = 1;
    int maxSteps = 20;

    //... initialize state variable values
    for (i=0; i<n; i++)
    {
        xOld[i] = x[i];
        xPrev[i] = x[i];
    }

    //... repeat until convergence achieved
    while (steps < maxSteps)
    {
        //... compute flux rates for current state levels
        canStop = 1;
        (this->*derivs)(x, q);

        //... update state levels based on current flux rates
        for (i=0; i<n; i++)
        {
            x[i] = xOld[i] + (omega*qOld[i] + (1.0 - omega)*q[i]) * dt;
            x[i] = MIN(x[i], xMax[i]);
            x[i] = MAX(x[i], xMin[i]);

            if ( omega > 0.0 &&
                 fabs(x[i] - xPrev[i]) > xTol[i] ) canStop = 0;
            xPrev[i] = x[i];
        }

        //... return if process converges
        if (canStop) return steps;
        steps++;
    }

    //... no convergence so return 0
    return 0;
}
