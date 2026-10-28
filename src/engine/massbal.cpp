//-----------------------------------------------------------------------------
//   massbal.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     11/01/21  (Build 5.2.0)
//   Author:   L. Rossman
//             M. Tryby (EPA)
//
//   Mass balance functions
//
//   Update History
//   ==============
//   Build 5.1.007:
//   - Mass balances modified to to correctly handle negative external inflows.
//   - Volume from minimum surface area at nodes included in mass balances.
//   Build 5.1.008:
//   - massbal_updateRunoffTotals() modified.
//   - LID drain flows and returned outfall flows added to components of
//     runoff mass balance.
//   - Seepage pollutant loss added into mass balances.
//   Build 5.1.010:
//   - Remaining pollutant mass in "dry" elements now added to final storage.
//   Build 5.1.011:
//   - Final stored pollutant mass in links ignored for Steady Flow routing.
//   Build 5.1.012:
//   - Terminal storage nodes no longer treated as non-storage terminal
//     nodes are when updating total outflow volume.
//   Build 5.1.013:
//   - Volume from MinSurfArea no longer included in initial & final storage.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants   
//-----------------------------------------------------------------------------
static const double MAX_RUNOFF_BALANCE_ERR = 10.0;
static const double MAX_FLOW_BALANCE_ERR   = 10.0;

//-----------------------------------------------------------------------------
//  Shared variables   
//-----------------------------------------------------------------------------
    // overall surface runoff continuity totals
   // overall WQ washoff continuity totals
    // overall groundwater continuity totals 
      // overall routed flow continuity totals 
      // overall routed WQ continuity totals 
  // routed flow totals over time step

  // routed WQ totals over time step

//-----------------------------------------------------------------------------
//  Exportable variables
//-----------------------------------------------------------------------------
              // total inflow volume to each node (ft3)
             // total outflow volume from each node (ft3)

// ---- massbal module state: formerly file-scope variables of massbal.c.
// ---- One instance per Project, so two models never share it.
struct MassbalState
{
    TRunoffTotals RunoffTotals{};   // was global (external linkage)
    TLoadingTotals * LoadingTotals{};   // was global (external linkage)
    TGwaterTotals GwaterTotals{};   // was global (external linkage)
    TRoutingTotals FlowTotals{};   // was global (external linkage)
    TRoutingTotals * QualTotals{};   // was global (external linkage)
    TRoutingTotals OldStepFlowTotals{};   // was global (external linkage)
    double TotalArea{};   // was global (external linkage)
};
void Project::massbal_createState() { massbal_ = new MassbalState(); }
void Project::massbal_destroyState() { delete massbal_; massbal_ = nullptr; }
               // total drainage area (ft2)

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  massbal_open                (called from swmm_start in swmm5.c)
//  massbal_close               (called from swmm_end in swmm5.c)
//  massbal_report              (called from swmm_end in swmm5.c)
//  massbal_updateRunoffTotals  (called from subcatch_getRunoff)
//  massbal_updateDrainTotals   (called from evalLidUnit in lid.c)
//  massbal_updateLoadingTotals (called from subcatch_getBuildup)
//  massbal_updateGwaterTotals  (called from updateMassBal in gwater.c)
//  massbal_updateRoutingTotals (called from routing_execute)
//  massbal_initTimeStepTotals  (called from routing_execute)
//  massbal_addInflowFlow       (called from routing.c)
//  massbal_addInflowQual       (called from routing.c)
//  massbal_addOutflowFlow      (called from removeOutflows in routing.c)
//  massbal_addOutflowQual      (called from removeOutflows in routing.c)
//  massbal_addNodeLosses       (called from removeStorageLosses in routing.c)
//  massbal_addLinkLosses       (called from removeConduitLosses in routing.c)
//  massbal_addReactedMass      (called from qualrout.c & treatmnt.c)
//  massbal_addSeepageLoss      (called from routing.c)
//  massbal_addToFinalStorage   (called from qualrout.c)
//  massbal_getStepFlowError    (called from routing.c)

//-----------------------------------------------------------------------------
//  Local Functions   
//-----------------------------------------------------------------------------








//=============================================================================

int Project::massbal_open()
//
//  Input:   none
//  Output:  returns error code
//  Purpose: opens and initializes mass balance continuity checking.
//
{
    int j, n;

    // --- initialize global continuity errors
    RunoffError = 0.0;
    GwaterError = 0.0;
    FlowError   = 0.0;
    QualError   = 0.0;

    // --- initialize runoff totals
    massbal_->RunoffTotals.rainfall    = 0.0;
    massbal_->RunoffTotals.evap        = 0.0;
    massbal_->RunoffTotals.infil       = 0.0;
    massbal_->RunoffTotals.runoff      = 0.0;
    massbal_->RunoffTotals.runon       = 0.0;
    massbal_->RunoffTotals.drains      = 0.0;
    massbal_->RunoffTotals.snowRemoved = 0.0;
    massbal_->RunoffTotals.initStorage = 0.0;
    massbal_->RunoffTotals.initSnowCover = 0.0;
    massbal_->TotalArea = 0.0;
    for (j = 0; j < Nobjects[SUBCATCH]; j++)
    {
        massbal_->RunoffTotals.initStorage += subcatch_getStorage(j);
        massbal_->RunoffTotals.initSnowCover += snow_getSnowCover(j);
        massbal_->TotalArea += Subcatch[j].area;
    }

    // --- initialize groundwater totals
    massbal_->GwaterTotals.infil        = 0.0;
    massbal_->GwaterTotals.upperEvap    = 0.0;
    massbal_->GwaterTotals.lowerEvap    = 0.0;
    massbal_->GwaterTotals.lowerPerc    = 0.0;
    massbal_->GwaterTotals.gwater       = 0.0;
    massbal_->GwaterTotals.initStorage  = 0.0;
    massbal_->GwaterTotals.finalStorage = 0.0;
    for ( j = 0; j < Nobjects[SUBCATCH]; j++ )
    {
        massbal_->GwaterTotals.initStorage += gwater_getVolume(j) * Subcatch[j].area;
    }

    // --- initialize node flow & storage totals
    massbal_->FlowTotals.dwInflow = 0.0;
    massbal_->FlowTotals.wwInflow = 0.0;
    massbal_->FlowTotals.gwInflow = 0.0;
    massbal_->FlowTotals.iiInflow = 0.0;
    massbal_->FlowTotals.exInflow = 0.0;
    massbal_->FlowTotals.flooding = 0.0;
    massbal_->FlowTotals.outflow  = 0.0;
    massbal_->FlowTotals.evapLoss = 0.0; 
    massbal_->FlowTotals.seepLoss = 0.0;
    massbal_->FlowTotals.reacted  = 0.0;
    massbal_->FlowTotals.initStorage = 0.0;
    for (j = 0; j < Nobjects[NODE]; j++)
        massbal_->FlowTotals.initStorage += Node[j].newVolume;
    for (j = 0; j < Nobjects[LINK]; j++)
        massbal_->FlowTotals.initStorage += Link[j].newVolume;
    StepFlowTotals = massbal_->FlowTotals;

    // --- initialize arrays to null
    massbal_->LoadingTotals = NULL;
    massbal_->QualTotals = NULL;
    StepQualTotals = NULL;
    NodeInflow = NULL;
    NodeOutflow = NULL;

    // --- allocate memory for WQ washoff continuity totals
    n = Nobjects[POLLUT];
    if ( n > 0 )
    {
        massbal_->LoadingTotals = (TLoadingTotals *) calloc(n, sizeof(TLoadingTotals));
        if ( massbal_->LoadingTotals == NULL )
        {
             report_writeErrorMsg(ERR_MEMORY, "");
             return ErrorCode;
        }
        for (j = 0; j < n; j++)
        {
            massbal_->LoadingTotals[j].initLoad      = massbal_getBuildup(j);
            massbal_->LoadingTotals[j].buildup       = 0.0;
            massbal_->LoadingTotals[j].deposition    = 0.0;
            massbal_->LoadingTotals[j].sweeping      = 0.0;
            massbal_->LoadingTotals[j].infil         = 0.0;
            massbal_->LoadingTotals[j].bmpRemoval    = 0.0;
            massbal_->LoadingTotals[j].runoff        = 0.0;
            massbal_->LoadingTotals[j].finalLoad     = 0.0;
        }
    }

    // --- allocate memory for nodal WQ continuity totals
    if ( n > 0 )
    {
         massbal_->QualTotals = (TRoutingTotals *) calloc(n, sizeof(TRoutingTotals));
         StepQualTotals = (TRoutingTotals *) calloc(n, sizeof(TRoutingTotals));
         if ( massbal_->QualTotals == NULL || StepQualTotals == NULL )
         {
             report_writeErrorMsg(ERR_MEMORY, "");
             return ErrorCode;
         }
     }

    // --- initialize WQ totals
    for (j = 0; j < n; j++)
    {
        massbal_->QualTotals[j].dwInflow = 0.0;
        massbal_->QualTotals[j].wwInflow = 0.0;
        massbal_->QualTotals[j].gwInflow = 0.0;
        massbal_->QualTotals[j].exInflow = 0.0;
        massbal_->QualTotals[j].flooding = 0.0;
        massbal_->QualTotals[j].outflow  = 0.0;
        massbal_->QualTotals[j].evapLoss = 0.0;
        massbal_->QualTotals[j].seepLoss = 0.0; 
        massbal_->QualTotals[j].reacted  = 0.0;
        massbal_->QualTotals[j].initStorage = massbal_getStoredMass(j);
    }

    // --- initialize totals used over a single time step
    massbal_initTimeStepTotals();

    // --- allocate memory for nodal flow continuity
    if ( Nobjects[NODE] > 0 )
    {
        NodeInflow = (double *) calloc(Nobjects[NODE], sizeof(double));
        if ( NodeInflow == NULL )
        {
             report_writeErrorMsg(ERR_MEMORY, "");
             return ErrorCode;
        }
        NodeOutflow = (double *) calloc(Nobjects[NODE], sizeof(double));
        if ( NodeOutflow == NULL )
        {
             report_writeErrorMsg(ERR_MEMORY, "");
             return ErrorCode;
        }
        for (j = 0; j < Nobjects[NODE]; j++) NodeInflow[j] = Node[j].newVolume;
    }
    return ErrorCode;
}

//=============================================================================

void Project::massbal_close()
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory used by mass balance system.
//
{
    FREE(massbal_->LoadingTotals);
    FREE(massbal_->QualTotals);
    FREE(StepQualTotals);
    FREE(NodeInflow);
    FREE(NodeOutflow);
}

//=============================================================================

void Project::massbal_report()
//
//  Input:   none
//  Output:  none
//  Purpose: reports mass balance results.
//
{
    int    j;
    double gwArea = 0.0;

    if ( Nobjects[SUBCATCH] > 0 )
    {
        if ( massbal_getRunoffError() > MAX_RUNOFF_BALANCE_ERR ||
             RptFlags.continuity == TRUE
           ) report_writeRunoffError(&massbal_->RunoffTotals, massbal_->TotalArea);

        if ( Nobjects[POLLUT] > 0 && !IgnoreQuality )
        {
            if ( massbal_getLoadingError() > MAX_RUNOFF_BALANCE_ERR ||
                 RptFlags.continuity == TRUE
               ) report_writeLoadingError(massbal_->LoadingTotals);
        }
    }

    if ( Nobjects[AQUIFER] > 0  && !IgnoreGwater )
    {
        if ( massbal_getGwaterError() > MAX_RUNOFF_BALANCE_ERR ||
             RptFlags.continuity == TRUE )
        {
            for ( j = 0; j < Nobjects[SUBCATCH]; j++ )
            {
                if ( Subcatch[j].groundwater ) gwArea += Subcatch[j].area;
            }
            if ( gwArea > 0.0 ) report_writeGwaterError(&massbal_->GwaterTotals, gwArea);
       }
    }

    if ( Nobjects[NODE] > 0 && !IgnoreRouting )
    {
        if ( massbal_getFlowError() > MAX_FLOW_BALANCE_ERR ||
             RptFlags.continuity == TRUE
           ) report_writeFlowError(&massbal_->FlowTotals);
    
        if ( Nobjects[POLLUT] > 0 && !IgnoreQuality )
        {
            if ( massbal_getQualError() > MAX_FLOW_BALANCE_ERR ||
                 RptFlags.continuity == TRUE
               ) report_writeQualError(massbal_->QualTotals);
        }
    }
}

//=============================================================================

double Project::massbal_getBuildup(int p)
//
//  Input:   p = pollutant index
//  Output:  returns total pollutant buildup (lbs or kg)
//  Purpose: computes current total buildup of a pollutant over study area.
//
{
    int    i, j;
    double load = 0.0;

    for (j = 0; j < Nobjects[SUBCATCH]; j++)
    {
        for (i = 0; i < Nobjects[LANDUSE]; i++)
        {
            load += Subcatch[j].landFactor[i].buildup[p];
        }
        load += Subcatch[j].pondedQual[p] * Pollut[p].mcf;
    }
    return load;
}

//=============================================================================

void Project::massbal_updateRunoffTotals(int flowType, double v)
//
//  Input:   flowType = type of flow
//           v = flow volume (ft3)
//  Output:  none
//  Purpose: updates runoff totals after current time step.
//
{
    switch(flowType)
    {
    case RUNOFF_RAINFALL: massbal_->RunoffTotals.rainfall += v; break;
    case RUNOFF_EVAP:     massbal_->RunoffTotals.evap     += v; break;
    case RUNOFF_INFIL:    massbal_->RunoffTotals.infil    += v; break;
    case RUNOFF_RUNOFF:   massbal_->RunoffTotals.runoff   += v; break;
    case RUNOFF_DRAINS:   massbal_->RunoffTotals.drains   += v; break;
    case RUNOFF_RUNON:    massbal_->RunoffTotals.runon    += v; break;
    }
}

//=============================================================================

void Project::massbal_updateGwaterTotals(double vInfil, double vUpperEvap, double vLowerEvap,
                                double vLowerPerc, double vGwater)
//
//  Input:   vInfil = volume depth of infiltrated water (ft)
//           vUpperEvap = volume depth of upper evaporation (ft)
//           vLowerEvap = volume depth of lower evaporation (ft)
//           vLowerPerc = volume depth of percolation to deep GW (ft)
//           vGwater = volume depth of groundwater outflow (ft)
//  Output:  none
//  Purpose: updates groundwater totals after current time step.
//
{
    massbal_->GwaterTotals.infil     += vInfil;
    massbal_->GwaterTotals.upperEvap += vUpperEvap;
    massbal_->GwaterTotals.lowerEvap += vLowerEvap;
    massbal_->GwaterTotals.lowerPerc += vLowerPerc;
    massbal_->GwaterTotals.gwater    += vGwater;
}

//=============================================================================

void Project::massbal_initTimeStepTotals()
//
//  Input:   none
//  Output:  none
//  Purpose: initializes routing totals for current time step.
//
{
    int j;
    massbal_->OldStepFlowTotals = StepFlowTotals;
    StepFlowTotals.dwInflow  = 0.0;
    StepFlowTotals.wwInflow  = 0.0;
    StepFlowTotals.gwInflow  = 0.0;
    StepFlowTotals.iiInflow  = 0.0;
    StepFlowTotals.exInflow  = 0.0;
    StepFlowTotals.flooding  = 0.0;
    StepFlowTotals.outflow   = 0.0;
    StepFlowTotals.evapLoss  = 0.0;
    StepFlowTotals.seepLoss  = 0.0;
    StepFlowTotals.reacted   = 0.0;
    for (j=0; j<Nobjects[POLLUT]; j++)
    {
        StepQualTotals[j].dwInflow  = 0.0;
        StepQualTotals[j].wwInflow  = 0.0;
        StepQualTotals[j].gwInflow  = 0.0;
        StepQualTotals[j].iiInflow  = 0.0;
        StepQualTotals[j].exInflow  = 0.0;
        StepQualTotals[j].flooding  = 0.0;
        StepQualTotals[j].outflow   = 0.0;
        StepQualTotals[j].reacted   = 0.0;
        StepQualTotals[j].seepLoss  = 0.0;
        StepQualTotals[j].initStorage = 0.0;
        StepQualTotals[j].finalStorage = 0.0;
    }
}

//=============================================================================

void Project::massbal_addInflowFlow(int type, double q)
//
//  Input:   type = type of inflow
//           q    = inflow rate (cfs)
//  Output:  none
//  Purpose: adds flow inflow to routing totals for current time step.
//
{
    switch (type)
    {
      case DRY_WEATHER_INFLOW: StepFlowTotals.dwInflow += q; break;
      case WET_WEATHER_INFLOW: StepFlowTotals.wwInflow += q; break;
      case GROUNDWATER_INFLOW: StepFlowTotals.gwInflow += q; break;
      case RDII_INFLOW:        StepFlowTotals.iiInflow += q; break;
      case EXTERNAL_INFLOW:    StepFlowTotals.exInflow += q; break;
    }
}

//=============================================================================

void Project::massbal_updateLoadingTotals(int type, int p, double w)
//
//  Input:   type = type of inflow
//           p    = pollutant index
//           w    = mass loading
//  Output:  none
//  Purpose: adds inflow mass loading to loading totals for current time step.
//
{
    switch (type)
    {
      case BUILDUP_LOAD:     massbal_->LoadingTotals[p].buildup    += w; break;
      case DEPOSITION_LOAD:  massbal_->LoadingTotals[p].deposition += w; break;
      case SWEEPING_LOAD:    massbal_->LoadingTotals[p].sweeping   += w; break;
      case INFIL_LOAD:       massbal_->LoadingTotals[p].infil      += w; break;
      case BMP_REMOVAL_LOAD: massbal_->LoadingTotals[p].bmpRemoval += w; break;
      case RUNOFF_LOAD:      massbal_->LoadingTotals[p].runoff     += w; break;
      case FINAL_LOAD:       massbal_->LoadingTotals[p].finalLoad  += w; break;
    }
}

//=============================================================================

void Project::massbal_addInflowQual(int type, int p, double w)
//
//  Input:   type = type of inflow
//           p    = pollutant index
//           w    = mass flow rate (mass/sec)
//  Output:  none
//  Purpose: adds quality inflow to routing totals for current time step.
//
{
    if ( p < 0 || p >= Nobjects[POLLUT] ) return;
    switch (type)
    {
      case DRY_WEATHER_INFLOW: StepQualTotals[p].dwInflow += w; break;
      case WET_WEATHER_INFLOW: StepQualTotals[p].wwInflow += w; break;
      case GROUNDWATER_INFLOW: StepQualTotals[p].gwInflow += w; break;
      case EXTERNAL_INFLOW:    StepQualTotals[p].exInflow += w; break;
      case RDII_INFLOW:        StepQualTotals[p].iiInflow += w; break;
   }
}

//=============================================================================

void Project::massbal_addOutflowFlow(double q, int isFlooded)
//
//  Input:   q = outflow flow rate (cfs)
//           isFlooded = TRUE if outflow represents internal flooding
//  Output:  none
//  Purpose: adds flow outflow over current time step to routing totals.
//
{
    if ( isFlooded ) StepFlowTotals.flooding += q;
    else             StepFlowTotals.outflow += q;
}

//=============================================================================

void Project::massbal_addOutflowQual(int p, double w, int isFlooded)
//
//  Input:   p = pollutant index
//           w = mass outflow rate (mass/sec)
//           isFlooded = TRUE if outflow represents internal flooding
//  Output:  none
//  Purpose: adds pollutant outflow over current time step to routing totals.
//
{
    if ( p < 0 || p >= Nobjects[POLLUT] ) return;
    if ( w >= 0.0 )
    {
        if ( isFlooded ) StepQualTotals[p].flooding += w;
        else             StepQualTotals[p].outflow += w;
    }
    else StepQualTotals[p].exInflow -= w;
}

//=============================================================================

void Project::massbal_addReactedMass(int p, double w)
//
//  Input:   p = pollutant index
//           w = rate of mass reacted (mass/sec)
//  Output:  none
//  Purpose: adds mass reacted during current time step to routing totals.
//
{
    if ( p < 0 || p >= Nobjects[POLLUT] ) return;
    StepQualTotals[p].reacted += w;
}

//=============================================================================

void Project::massbal_addSeepageLoss(int p, double w)
//
//  Input:   p = pollutant index
//           w = mass seepage rate (mass/sec)
//  Output:  none
//  Purpose: adds mass lost to seepage during current time step to routing totals.
//
{
    if ( p < 0 || p >= Nobjects[POLLUT] ) return;
    StepQualTotals[p].seepLoss += w;
}

//=============================================================================

void Project::massbal_addToFinalStorage(int p, double w)
//
//  Input:   p = pollutant index
//           w = pollutant mass
//  Output:  none
//  Purpose: adds mass remaining on dry surface to routing totals.
//
{
    if ( p < 0 || p >= Nobjects[POLLUT] ) return;
    StepQualTotals[p].finalStorage += w;
}

//=============================================================================

void Project::massbal_addNodeLosses(double evapLoss, double seepLoss)
//
//  Input:   evapLoss = evaporation loss from all nodes (ft3/sec)
//           seepLoss = seepage loss from all nodes (ft3/sec)
//  Output:  none
//  Purpose: adds node losses over current time step to routing totals.
//
{
    StepFlowTotals.evapLoss += evapLoss;
    StepFlowTotals.seepLoss += seepLoss;
}

//=============================================================================

void Project::massbal_addLinkLosses(double evapLoss, double seepLoss)
//
//  Input:   evapLoss = evaporation loss from all links (ft3/sec)
//           infilLoss = infiltration loss from all links (ft3/sec)
//  Output:  none
//  Purpose: adds link losses over current time step to routing totals.
//
{
    StepFlowTotals.evapLoss += evapLoss;
    StepFlowTotals.seepLoss += seepLoss;
}

//=============================================================================

void Project::massbal_updateRoutingTotals(double tStep)
//
//  Input:   tStep = time step (sec)
//  Output:  none
//  Purpose: updates overall routing totals with totals from current time step.
//
{
    int j;
    massbal_->FlowTotals.dwInflow += StepFlowTotals.dwInflow * tStep;
    massbal_->FlowTotals.wwInflow += StepFlowTotals.wwInflow * tStep;
    massbal_->FlowTotals.gwInflow += StepFlowTotals.gwInflow * tStep;
    massbal_->FlowTotals.iiInflow += StepFlowTotals.iiInflow * tStep;
    massbal_->FlowTotals.exInflow += StepFlowTotals.exInflow * tStep;
    massbal_->FlowTotals.flooding += StepFlowTotals.flooding * tStep;
    massbal_->FlowTotals.outflow  += StepFlowTotals.outflow * tStep;
    massbal_->FlowTotals.evapLoss += StepFlowTotals.evapLoss * tStep;
    massbal_->FlowTotals.seepLoss += StepFlowTotals.seepLoss * tStep;

    for (j = 0; j < Nobjects[POLLUT]; j++)
    {
        massbal_->QualTotals[j].dwInflow += StepQualTotals[j].dwInflow * tStep;
        massbal_->QualTotals[j].wwInflow += StepQualTotals[j].wwInflow * tStep;
        massbal_->QualTotals[j].gwInflow += StepQualTotals[j].gwInflow * tStep;
        massbal_->QualTotals[j].iiInflow += StepQualTotals[j].iiInflow * tStep;
        massbal_->QualTotals[j].exInflow += StepQualTotals[j].exInflow * tStep;
        massbal_->QualTotals[j].flooding += StepQualTotals[j].flooding * tStep;
        massbal_->QualTotals[j].outflow  += StepQualTotals[j].outflow * tStep;
        massbal_->QualTotals[j].reacted  += StepQualTotals[j].reacted * tStep;
        massbal_->QualTotals[j].seepLoss += StepQualTotals[j].seepLoss * tStep;
        massbal_->QualTotals[j].finalStorage += StepQualTotals[j].finalStorage;
    }

    for (j = 0; j < Nobjects[NODE]; j++)
    {
        NodeInflow[j] += Node[j].inflow * tStep;
        if (Node[j].type == OUTFALL ||
            (Node[j].degree == 0 && Node[j].type != STORAGE))
        {
            NodeOutflow[j] += Node[j].inflow * tStep;
        }
        else
        {
            NodeOutflow[j] += Node[j].outflow * tStep;
            if (Node[j].newVolume <= Node[j].fullVolume)
                NodeOutflow[j] += Node[j].overflow * tStep;
        }
    }
}

//=============================================================================

double Project::massbal_getStorage(char isFinalStorage)
//
//  Input:   isFinalStorage = TRUE if at final time period
//  Output:  returns storage volume used (ft3)
//  Purpose: computes total system storage (nodes + links) filled
//
{
    int    j;
    double totalStorage = 0.0;
    double nodeStorage;

    // --- get volume in nodes
    for (j = 0; j < Nobjects[NODE]; j++)
    {
        nodeStorage = Node[j].newVolume;
        if ( isFinalStorage ) NodeOutflow[j] += nodeStorage;
        totalStorage += nodeStorage;
    }

    // --- skip final link storage for Steady Flow routing 
    if ( isFinalStorage && RouteModel == SF ) return totalStorage;

    // --- add on volume stored in links
    for (j = 0; j < Nobjects[LINK]; j++)
    {
        totalStorage += Link[j].newVolume;
    }
    return totalStorage;
}

//=============================================================================

void Project::massbal_getSysFlows(double f, double sysFlows[])
//
//  Input:   f = time weighting factor
//  Output:  sysFlows = array of total system flows
//  Purpose: retrieves time-weighted average of old and new system flows.
//
{
    double f1 = 1.0 - f;
    sysFlows[SYS_DWFLOW] = (f1 * massbal_->OldStepFlowTotals.dwInflow +
                             f * StepFlowTotals.dwInflow) * UCF(FLOW);
    sysFlows[SYS_GWFLOW] = (f1 * massbal_->OldStepFlowTotals.gwInflow +
                             f * StepFlowTotals.gwInflow) * UCF(FLOW);
    sysFlows[SYS_IIFLOW] = (f1 * massbal_->OldStepFlowTotals.iiInflow +
                             f * StepFlowTotals.iiInflow) * UCF(FLOW);
    sysFlows[SYS_EXFLOW] = (f1 * massbal_->OldStepFlowTotals.exInflow +
                             f * StepFlowTotals.exInflow) * UCF(FLOW);
    sysFlows[SYS_FLOODING] = (f1 * massbal_->OldStepFlowTotals.flooding +
                               f * StepFlowTotals.flooding) * UCF(FLOW);
    sysFlows[SYS_OUTFLOW] = (f1 * massbal_->OldStepFlowTotals.outflow +
                              f * StepFlowTotals.outflow) * UCF(FLOW);
    sysFlows[SYS_STORAGE] = (f1 * massbal_->OldStepFlowTotals.finalStorage +
                              f * StepFlowTotals.finalStorage) * UCF(VOLUME);
}

//=============================================================================

double Project::massbal_getRunoffError()
//
//  Input:   none
//  Output:  none
//  Purpose: computes runoff mass balance error.
//
{
    int    j;
    double totalInflow;
    double totalOutflow;

    // --- find final storage on all subcatchments
    massbal_->RunoffTotals.finalStorage = 0.0;
    massbal_->RunoffTotals.finalSnowCover = 0.0;
    for (j = 0; j < Nobjects[SUBCATCH]; j++)
    {
        massbal_->RunoffTotals.finalStorage += subcatch_getStorage(j);
        massbal_->RunoffTotals.finalSnowCover += snow_getSnowCover(j);
    }

    // --- get snow removed from system
    massbal_->RunoffTotals.snowRemoved = Snow.removed;

    // --- compute % difference between total inflow and outflow
    totalInflow  = massbal_->RunoffTotals.rainfall +
                   massbal_->RunoffTotals.runon +
                   massbal_->RunoffTotals.initStorage +
                   massbal_->RunoffTotals.initSnowCover;
    totalOutflow = massbal_->RunoffTotals.evap +
                   massbal_->RunoffTotals.infil +
                   massbal_->RunoffTotals.runoff +
                   massbal_->RunoffTotals.drains +
                   massbal_->RunoffTotals.snowRemoved +
                   massbal_->RunoffTotals.finalStorage +
                   massbal_->RunoffTotals.finalSnowCover;
    massbal_->RunoffTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        massbal_->RunoffTotals.pctError = TINY;
    }
    else if ( totalInflow > 0.0 )
    {
        massbal_->RunoffTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( totalOutflow > 0.0 )
    {
        massbal_->RunoffTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    RunoffError = massbal_->RunoffTotals.pctError;
    return massbal_->RunoffTotals.pctError;
}

//=============================================================================

double Project::massbal_getLoadingError()
//
//  Input:   none
//  Output:  none
//  Purpose: computes runoff load mass balance error.
//
{
    int    j;
    double loadIn;
    double loadOut;
    double maxError = 0.0;

    for (j = 0; j < Nobjects[POLLUT]; j++)
    {
        // --- get final pollutant loading remaining on land surface
        massbal_->LoadingTotals[j].finalLoad += massbal_getBuildup(j); 

        // --- compute total load added to study area
        loadIn = massbal_->LoadingTotals[j].initLoad +
                 massbal_->LoadingTotals[j].buildup +
                 massbal_->LoadingTotals[j].deposition;
    
        // --- compute total load removed from study area
        loadOut = massbal_->LoadingTotals[j].sweeping +
                  massbal_->LoadingTotals[j].infil +
                  massbal_->LoadingTotals[j].bmpRemoval +
                  massbal_->LoadingTotals[j].runoff +
                  massbal_->LoadingTotals[j].finalLoad;

        // --- compute mass balance error
        massbal_->LoadingTotals[j].pctError = 0.0;
        if ( fabs(loadIn - loadOut) < 0.001 )
        {
            massbal_->LoadingTotals[j].pctError = TINY;
        }
        else if ( loadIn > 0.0 )
        {
            massbal_->LoadingTotals[j].pctError = 100.0 * (1.0 - loadOut / loadIn);
        }
        else if ( loadOut > 0.0 )
        {
            massbal_->LoadingTotals[j].pctError = 100.0 * (loadIn / loadOut - 1.0);
        }
        maxError = MAX(maxError, massbal_->LoadingTotals[j].pctError);

        // --- report total counts as log10
        if ( Pollut[j].units == COUNT )
        {
            massbal_->LoadingTotals[j].initLoad   = LOG10(massbal_->LoadingTotals[j].initLoad);
            massbal_->LoadingTotals[j].buildup    = LOG10(massbal_->LoadingTotals[j].buildup);
            massbal_->LoadingTotals[j].deposition = LOG10(massbal_->LoadingTotals[j].deposition);
            massbal_->LoadingTotals[j].sweeping   = LOG10(massbal_->LoadingTotals[j].sweeping);
            massbal_->LoadingTotals[j].infil      = LOG10(massbal_->LoadingTotals[j].infil);
            massbal_->LoadingTotals[j].bmpRemoval = LOG10(massbal_->LoadingTotals[j].bmpRemoval);
            massbal_->LoadingTotals[j].runoff     = LOG10(massbal_->LoadingTotals[j].runoff);
            massbal_->LoadingTotals[j].finalLoad  = LOG10(massbal_->LoadingTotals[j].finalLoad);
        }
    }
    return maxError;
}

//=============================================================================

double Project::massbal_getGwaterError()
//
//  Input:   none
//  Output:  none
//  Purpose: computes groundwater mass balance error.
//
{
    int    j;
    double totalInflow;
    double totalOutflow;

    // --- find final storage in groundwater
    massbal_->GwaterTotals.finalStorage = 0.0;
    for ( j = 0; j < Nobjects[SUBCATCH]; j++ )
    {
        massbal_->GwaterTotals.finalStorage += gwater_getVolume(j) * Subcatch[j].area;
    }

    // --- compute % difference between total inflow and outflow
    totalInflow  = massbal_->GwaterTotals.infil +
                   massbal_->GwaterTotals.initStorage;
    totalOutflow = massbal_->GwaterTotals.upperEvap +
                   massbal_->GwaterTotals.lowerEvap +
                   massbal_->GwaterTotals.lowerPerc +
                   massbal_->GwaterTotals.gwater +
                   massbal_->GwaterTotals.finalStorage;
    massbal_->GwaterTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        massbal_->GwaterTotals.pctError = TINY;
    }
    else if ( totalInflow > 0.0 )
    {
        massbal_->GwaterTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( totalOutflow > 0.0 )
    {
        massbal_->GwaterTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    GwaterError = massbal_->GwaterTotals.pctError;
    return massbal_->GwaterTotals.pctError;
}

//=============================================================================

double Project::massbal_getFlowError()
//
//  Input:   none
//  Output:  none
//  Purpose: computes flow routing mass balance error.
//
{
    double totalInflow;
    double totalOutflow;

    // --- get final volume of nodes and links
    massbal_->FlowTotals.finalStorage = massbal_getStorage(TRUE);

    // --- add contributions to total inflow and outflow that are always positive
    totalInflow = massbal_->FlowTotals.initStorage + massbal_->FlowTotals.wwInflow  + massbal_->FlowTotals.iiInflow;
    totalOutflow = massbal_->FlowTotals.finalStorage + massbal_->FlowTotals.flooding + massbal_->FlowTotals.evapLoss +
                   massbal_->FlowTotals.seepLoss + massbal_->FlowTotals.reacted;

    // --- add on contributions that might be either positive or negative
    if ( massbal_->FlowTotals.dwInflow >= 0.0 ) totalInflow += massbal_->FlowTotals.dwInflow;
    else                              totalOutflow -= massbal_->FlowTotals.dwInflow;
    if ( massbal_->FlowTotals.gwInflow >= 0.0 ) totalInflow += massbal_->FlowTotals.gwInflow;
    else                              totalOutflow -= massbal_->FlowTotals.gwInflow;
    if ( massbal_->FlowTotals.exInflow >= 0.0 ) totalInflow += massbal_->FlowTotals.exInflow;
    else                              totalOutflow -= massbal_->FlowTotals.exInflow;
    if ( massbal_->FlowTotals.outflow >= 0.0 )  totalOutflow += massbal_->FlowTotals.outflow;
    else                              totalInflow -= massbal_->FlowTotals.outflow;

    // --- find percent difference between total inflow and outflow
    massbal_->FlowTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        massbal_->FlowTotals.pctError = TINY;
    }
    else if ( fabs(totalInflow) > 0.0 )
    {
        massbal_->FlowTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( fabs(totalOutflow) > 0.0 )
    {
        massbal_->FlowTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    FlowError = massbal_->FlowTotals.pctError;
    return massbal_->FlowTotals.pctError;
}

//=============================================================================

double Project::massbal_getQualError()
//
//  Input:   none
//  Output:  none
//  Purpose: computes water quality routing mass balance error.
//
{
    int    p;
    double maxQualError = 0.0;
    double totalInflow;
    double totalOutflow;
    double cf;

    // --- analyze each pollutant
    for (p = 0; p < Nobjects[POLLUT]; p++)
    {
        // --- get final mass stored in nodes and links
        massbal_->QualTotals[p].finalStorage += massbal_getStoredMass(p);

        // --- compute % difference between total inflow and outflow
        totalInflow  = massbal_->QualTotals[p].dwInflow +
                       massbal_->QualTotals[p].wwInflow +
                       massbal_->QualTotals[p].gwInflow +
                       massbal_->QualTotals[p].iiInflow +
                       massbal_->QualTotals[p].exInflow +
                       massbal_->QualTotals[p].initStorage;
        totalOutflow = massbal_->QualTotals[p].flooding +
                       massbal_->QualTotals[p].outflow +
                       massbal_->QualTotals[p].reacted +
                       massbal_->QualTotals[p].seepLoss +
                       massbal_->QualTotals[p].finalStorage;
        massbal_->QualTotals[p].pctError = 0.0;
        if ( fabs(totalInflow - totalOutflow) < 0.001 )
        {
            massbal_->QualTotals[p].pctError = TINY;
        }
        else if ( totalInflow > 0.0 )
        {
            massbal_->QualTotals[p].pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
        }
        else if ( totalOutflow > 0.0 )
        {
            massbal_->QualTotals[p].pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
        }

        // --- update max. error among all pollutants
        if ( fabs(massbal_->QualTotals[p].pctError) > fabs(maxQualError) )
        {
            maxQualError = massbal_->QualTotals[p].pctError;
        }

        // --- convert totals to reporting units (lbs, kg, or Log(Count))
        cf = LperFT3;
        if ( Pollut[p].units == COUNT )
        {
            massbal_->QualTotals[p].dwInflow     = LOG10(cf * massbal_->QualTotals[p].dwInflow);
            massbal_->QualTotals[p].wwInflow     = LOG10(cf * massbal_->QualTotals[p].wwInflow);
            massbal_->QualTotals[p].gwInflow     = LOG10(cf * massbal_->QualTotals[p].gwInflow);
            massbal_->QualTotals[p].iiInflow     = LOG10(cf * massbal_->QualTotals[p].iiInflow);
            massbal_->QualTotals[p].exInflow     = LOG10(cf * massbal_->QualTotals[p].exInflow);
            massbal_->QualTotals[p].flooding     = LOG10(cf * massbal_->QualTotals[p].flooding);
            massbal_->QualTotals[p].outflow      = LOG10(cf * massbal_->QualTotals[p].outflow);
            massbal_->QualTotals[p].reacted      = LOG10(cf * massbal_->QualTotals[p].reacted);
            massbal_->QualTotals[p].seepLoss     = LOG10(cf * massbal_->QualTotals[p].seepLoss);
            massbal_->QualTotals[p].initStorage  = LOG10(cf * massbal_->QualTotals[p].initStorage);
            massbal_->QualTotals[p].finalStorage = LOG10(cf * massbal_->QualTotals[p].finalStorage);
        }
        else
        {
            cf = cf * UCF(MASS);
            if ( Pollut[p].units == UG ) cf /= 1000.0;
            massbal_->QualTotals[p].dwInflow     *= cf;
            massbal_->QualTotals[p].wwInflow     *= cf; 
            massbal_->QualTotals[p].gwInflow     *= cf; 
            massbal_->QualTotals[p].iiInflow     *= cf; 
            massbal_->QualTotals[p].exInflow     *= cf; 
            massbal_->QualTotals[p].flooding     *= cf; 
            massbal_->QualTotals[p].outflow      *= cf; 
            massbal_->QualTotals[p].reacted      *= cf; 
            massbal_->QualTotals[p].seepLoss     *= cf; 
            massbal_->QualTotals[p].initStorage  *= cf; 
            massbal_->QualTotals[p].finalStorage *= cf; 
        }
    }
    QualError = maxQualError;
    return maxQualError;
}
//=============================================================================

double Project::massbal_getStepFlowError()
//
//  Input:   none
//  Output:  returns fractional difference between total inflow and outflow.
//  Purpose: computes flow routing mass balance error at current time step.
//
{
    double totalInflow;
    double totalOutflow;

    // --- compute % difference between total inflow and outflow
    totalInflow  = StepFlowTotals.dwInflow +
                   StepFlowTotals.wwInflow +
                   StepFlowTotals.gwInflow +
                   StepFlowTotals.iiInflow +
                   StepFlowTotals.exInflow;
    totalOutflow = StepFlowTotals.flooding +
                   StepFlowTotals.outflow +
                   StepFlowTotals.evapLoss +
                   StepFlowTotals.seepLoss +
                   StepFlowTotals.reacted;
    if ( fabs(totalInflow) > 0.0 )
        return 1.0 - totalOutflow / totalInflow;
    else if ( fabs(totalOutflow) > 0.0 )
        return totalInflow / totalOutflow - 1.0;
    else return 0.0;
}

//=============================================================================

double Project::massbal_getStoredMass(int p)
//
//  Input:   p = pollutant index
//  Output:  returns mass of pollutant.
//  Purpose: computes mass of pollutant stored in conveyance network.
//
{
    int j;
    double storedMass = 0.0;

    // --- get mass stored in nodes
    for (j = 0; j < Nobjects[NODE]; j++)
        storedMass += Node[j].newVolume * Node[j].newQual[p];

    // --- get mass stored in links (except for Steady Flow routing)
    if ( RouteModel != SF )
    {
        for (j = 0; j < Nobjects[LINK]; j++)
            storedMass += Link[j].newVolume * Link[j].newQual[p];
    }
    return storedMass;
}
