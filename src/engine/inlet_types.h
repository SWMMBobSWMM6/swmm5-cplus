// inlet_types.h -- module-local types of inlet.c, exported because they appear
// in member-function signatures of class Project.
#ifndef INLET_TYPES_H
#define INLET_TYPES_H



typedef struct
{
    int       type;               // type of grate used
    double    length;             // length (parallel to flow) (ft)
    double    width;              // width (perpendicular to flow) (ft)
    double    fracOpenArea;       // fraction of grate area that is open
    double    splashVeloc;        // splash-over velocity (ft/s)
} TGrateInlet;

typedef struct
{
    double    length;             // length (parallel to flow) (ft)
    double    width;              // width (perpendicular to flow) (ft)
} TSlottedInlet;

typedef struct
{
    double    length;             // length of curb opening (ft)
    double    height;             // height of curb opening (ft)
    int       throatAngle;        // type of throat angle
} TCurbInlet;

typedef struct
{
    int       onGradeCurve;       // flow diversion curve index
    int       onSagCurve;         // flow rating curve index
} TCustomInlet;

typedef struct
{
    char *         ID;            // name assigned to inlet design
    int            type;          // type of inlet used (grate, curb, etc)
    TGrateInlet    grateInlet;    // length = 0 if not used
    TSlottedInlet  slottedInlet;  // length = 0 if not used
    TCurbInlet     curbInlet;     // length = 0 if not used
    int            customCurve;   // curve index = -1 if not used
} TInletDesign;

typedef struct
{
    int       flowPeriods;        // # periods with approach flow
    int       capturePeriods;     // # periods with captured flow
    int       backflowPeriods;    // # periods with backflow
    double    peakFlow;           // peak flow seen by inlet (cfs)
    double    peakFlowCapture;    // capture efficiency at peak flow
    double    avgFlowCapture;     // average capture efficiency
    double    bypassFreq;         // frequency of bypass flow
} TInletStats;

struct TInlet
{
    int         linkIndex;        // index of conduit link with the inlet
    int         designIndex;      // index of inlet's design
    int         nodeIndex;        // index of node receiving captured flow
    int         numInlets;        // # inlets on each side of street or in channel
    int         placement;        // whether inlet is on-grade or on-sag
    double      clogFactor;       // fractional degree of inlet clogging
    double      flowLimit;        // inlet flow restriction (cfs)
    double      localDepress;     // local gutter depression (ft)
    double      localWidth;       // local depression width (ft)

    double      flowFactor;       // flow = flowFactor * (flow spread)^2.67
    double      flowCapture;      // captured flow rate (cfs)
    double      backflow;         // backflow from capture node (cfs)
    double      backflowRatio;    // inlet backflow / capture node overflow
    TInletStats stats;            // inlet performance statistics
    TInlet *    nextInlet;        // next inlet in list
};

enum InletType {
    GRATE_INLET, CURB_INLET, COMBO_INLET, SLOTTED_INLET,
    DROP_GRATE_INLET, DROP_CURB_INLET, CUSTOM_INLET
};

enum GrateType {
    P50, P50x100, P30, CURVED_VANE, TILT_BAR_45,
    TILT_BAR_30, RETICULINE, GENERIC
};

enum InletPlacementType { AUTOMATIC, ON_GRADE, ON_SAG };

enum ThroatAngleType { HORIZONTAL_THROAT, INCLINED_THROAT, VERTICAL_THROAT };

#endif
