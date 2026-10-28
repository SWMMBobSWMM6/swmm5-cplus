//-----------------------------------------------------------------------------
//   treatmnt.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     11/01/21   (Build 5.2.0)
//   Author:   L. Rossman
//
//   Pollutant treatment functions.
//
//   Update History
//   ==============
//   Build 5.1.008:
//   - A bug in evaluating recursive calls to treatment functions was fixed. 
//   Build 5.2.0:
//   - Changed enumerated constant used to indicate a math expression error.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
static const int PVMAX = 5;            // number of process variables
enum   ProcessVarType {pvHRT,          // hydraulic residence time
                       pvDT,           // time step duration
                       pvFLOW,         // flow rate
                       pvDEPTH,        // water height above invert
                       pvAREA};        // storage surface area

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
                // treatment error code
                      // index of node being analyzed
                     // curent time step (sec)
                      // node inflow (cfs)
                      // node volume (ft3)
                      // array of pollut. removals

// ---- treatmnt module state: formerly file-scope variables of treatmnt.c.
// ---- One instance per Project, so two models never share it.
struct TreatmntState
{
    int ErrCode{};   // was static
    int J{};   // was static
    double Dt{};   // was static
    double Q{};   // was static
    double V{};   // was static
    double * R{};   // was static
    double * Cin{};   // was static
};
void Project::treatmnt_createState() { treatmnt_ = new TreatmntState(); }
void Project::treatmnt_destroyState() { delete treatmnt_; treatmnt_ = nullptr; }
                    // node inflow concentrations

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  treatmnt_open           (called from routing_open)
//  treatment_close         (called from routing_close)
//  treatmnt_readExpression (called from parseLine in input.c)
//  treatmnt_delete         (called from deleteObjects in project.c)
//  treatmnt_setInflow      (called from qualrout_execute)
//  treatmnt_treat          (called from findNodeQual in qualrout.c)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------






//=============================================================================

int  Project::treatmnt_open(void)
//
//  Input:   none
//  Output:  returns TRUE if successful, FALSE if not
//  Purpose: allocates memory for computing pollutant removals by treatment.
//
{
    treatmnt_->R = NULL;
    treatmnt_->Cin = NULL;
    if ( Nobjects[POLLUT] > 0 )
    {
        treatmnt_->R = (double *) calloc(Nobjects[POLLUT], sizeof(double));
        treatmnt_->Cin = (double *) calloc(Nobjects[POLLUT], sizeof(double));
        if ( treatmnt_->R == NULL || treatmnt_->Cin == NULL)
        {
            report_writeErrorMsg(ERR_MEMORY, "");
            return FALSE;
        }
    }
    return TRUE;
}

//=============================================================================

void Project::treatmnt_close(void)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: frees memory used for computing pollutant removals by treatment.
//
{
    FREE(treatmnt_->R);
    FREE(treatmnt_->Cin);
}

//=============================================================================

int  Project::treatmnt_readExpression(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads a treatment expression from a tokenized line of input.
//
{
    char  s[MAXLINE+1];
    char* expr;
    int   i, j, k, p;
    MathExpr* equation;                // ptr. to a math. expression

    // --- retrieve node & pollutant
    if ( ntoks < 3 ) return error_setInpError(ERR_ITEMS, "");
    j = project_findObject(NODE, tok[0]);
    if ( j < 0 ) return error_setInpError(ERR_NAME, tok[0]);
    p = project_findObject(POLLUT, tok[1]);
    if ( p < 0 ) return error_setInpError(ERR_NAME, tok[1]);

    // --- concatenate remaining tokens into a single string
    sstrncpy(s, tok[2], MAXLINE);
    for ( i=3; i<ntoks; i++)
    {
        sstrcat(s, " ", MAXLINE);
        sstrcat(s, tok[i], MAXLINE);
    }

    // --- check treatment type
    if      ( UCHAR(s[0]) == 'R' ) k = 0;
    else if ( UCHAR(s[0]) == 'C' ) k = 1;
    else return error_setInpError(ERR_KEYWORD, tok[2]);

    // --- start treatment expression after equals sign
    expr = strchr(s, '=');
    if ( expr == NULL ) return error_setInpError(ERR_KEYWORD, "");
    else expr++;

    // --- create treatment objects at node j if they don't already exist
    if ( Node[j].treatment == NULL )
    {
        if ( !createTreatment(j) ) return error_setInpError(ERR_MEMORY, "");
    }

    // --- create a parsed expression tree from the string expr
    //     (getVariableIndex is the function that converts a treatment
    //      variable's name into an index number) 
    equation = mathexpr_create(expr, &Project::treatmnt_getVariableIndex);
    if ( equation == NULL )
        return error_setInpError(ERR_MATH_EXPR, "");

    // --- save the treatment parameters in the node's treatment object
    if (Node[j].treatment != NULL)
    {
        Node[j].treatment[p].treatType = k;
        Node[j].treatment[p].equation = equation;
    }
    return 0;
}

//=============================================================================

void Project::treatmnt_delete(int j)
//
//  Input:   j = node index
//  Output:  none
//  Purpose: deletes the treatment objects for each pollutant at a node.
//
{
    int p;
    if ( Node[j].treatment )
    {
        for (p=0; p<Nobjects[POLLUT]; p++)
            mathexpr_delete(Node[j].treatment[p].equation);
        free(Node[j].treatment);
    }
    Node[j].treatment = NULL;
}

//=============================================================================

void  Project::treatmnt_setInflow(double qIn, double wIn[])
//
//  Input:   j = node index
//           qIn = flow inflow rate (cfs)
//           wIn = pollutant mass inflow rate (mass/sec)
//  Output:  none
//  Purpose: computes and saves array of inflow concentrations to a node.
//
{
    int    p;
    if ( qIn > 0.0 )
        for (p = 0; p < Nobjects[POLLUT]; p++) treatmnt_->Cin[p] = wIn[p]/qIn;
    else
        for (p = 0; p < Nobjects[POLLUT]; p++) treatmnt_->Cin[p] = 0.0;
}

//=============================================================================

void  Project::treatmnt_treat(int j, double q, double v, double tStep)
//
//  Input:   j     = node index
//           q     = inflow to node (cfs)
//           v     = volume of node (ft3)
//           tStep = routing time step (sec)
//  Output:  none
//  Purpose: updates pollutant concentrations at a node after treatment.
//
{
    int    p;                          // pollutant index
    double cOut;                       // concentration after treatment
    double massLost;                   // mass lost by treatment per time step
    TTreatment* treatment;             // pointer to treatment object

    // --- set locally shared variables for node j
    if ( Node[j].treatment == NULL ) return;
    treatmnt_->ErrCode = 0;
    treatmnt_->J  = j;                            // current node
    treatmnt_->Dt = tStep;                        // current time step
    treatmnt_->Q  = q;                            // current inflow rate
    treatmnt_->V  = v;                            // current node volume

    // --- initialze each removal to indicate no value 
    for ( p = 0; p < Nobjects[POLLUT]; p++) treatmnt_->R[p] = -1.0;

    // --- determine removal of each pollutant
    for ( p = 0; p < Nobjects[POLLUT]; p++)
    {
        // --- removal is zero if there is no treatment equation
        treatment = &Node[j].treatment[p];
        if ( treatment->equation == NULL ) treatmnt_->R[p] = 0.0;

        // --- no removal for removal-type expression when there is no inflow 
	    else if ( treatment->treatType == REMOVAL && q <= ZERO ) treatmnt_->R[p] = 0.0;

        // --- otherwise evaluate the treatment expression to find R[p]
        else getRemoval(p);
    }

    // --- check for error condition
    if ( treatmnt_->ErrCode == ERR_CYCLIC_TREATMENT )
    {
         report_writeErrorMsg(ERR_CYCLIC_TREATMENT, Node[treatmnt_->J].ID);
    }

    // --- update nodal concentrations and mass balances
    else for ( p = 0; p < Nobjects[POLLUT]; p++ )
    {
        if ( treatmnt_->R[p] == 0.0 ) continue;
        treatment = &Node[j].treatment[p];

        // --- removal-type treatment equations get applied to inflow stream

        if ( treatment->treatType == REMOVAL )
        {
            // --- if no pollutant in inflow then cOut is current nodal concen.
            if ( treatmnt_->Cin[p] == 0.0 ) cOut = Node[j].newQual[p];

            // ---  otherwise apply removal to influent concen.
            else cOut = (1.0 - treatmnt_->R[p]) * treatmnt_->Cin[p];

            // --- cOut can't be greater than mixture concen. at node
            //     (i.e., in case node is a storage unit) 
            cOut = MIN(cOut, Node[j].newQual[p]);
        }

        // --- concentration-type equations get applied to nodal concentration
        else
        {
            cOut = (1.0 - treatmnt_->R[p]) * Node[j].newQual[p];
        }

        // --- mass lost must account for any initial mass in storage 
        massLost = (treatmnt_->Cin[p]*q*tStep + Node[j].oldQual[p]*Node[j].oldVolume - 
                   cOut*(q*tStep + Node[j].oldVolume)) / tStep; 
        massLost = MAX(0.0, massLost); 

        // --- add mass loss to mass balance totals and revise nodal concentration
        massbal_addReactedMass(p, massLost);
        Node[j].newQual[p] = cOut;
    }
}

//=============================================================================

int  Project::createTreatment(int j)
//
//  Input:   j = node index
//  Output:  returns TRUE if successful, FALSE if not
//  Purpose: creates a treatment object for each pollutant at a node.
//
{
    int p;
    Node[j].treatment = (TTreatment *) calloc(Nobjects[POLLUT],
                                              sizeof(TTreatment));
    if ( Node[j].treatment == NULL )
    {
        return FALSE;
    }
    for (p = 0; p < Nobjects[POLLUT]; p++)
    {
        Node[j].treatment[p].equation = NULL;
    }
    return TRUE;
}

//=============================================================================

int  Project::treatmnt_getVariableIndex(char* s)
//
//  Input:   s = name of a process variable or pollutant
//  Output:  returns index of process variable or pollutant
//  Purpose: finds position of process variable/pollutant in list of names.
//
{
    // --- check for a process variable first
    int k;
    int m = PVMAX;                     // PVMAX is number of process variables

    k = findmatch(s, ProcessVarWords);
    if ( k >= 0 ) return k;

    // --- then check for a pollutant concentration
    k = project_findObject(POLLUT, s);
    if ( k >= 0 ) return (k + m);

    // --- finally check for a pollutant removal
    if ( UCHAR(s[0]) == 'R' && s[1] == '_')
    {
        k = project_findObject(POLLUT, s+2);
        if ( k >= 0 ) return (Nobjects[POLLUT] + k + m);
    }
    return -1;
}

//=============================================================================

double Project::treatmnt_getVariableValue(int varCode)
//
//  Input:   varCode = code number of process variable or pollutant
//  Output:  returns current value of variable
//  Purpose: finds current value of a process variable or pollutant concen.,
//           making reference to the node being evaluated which is stored in
//           shared variable J.
//
{
    int    p;
    double a1, a2, y;
    TTreatment* treatment;

    // --- variable is a process variable
    if ( varCode < PVMAX )
    {
        switch ( varCode )
        {
          case pvHRT:                                 // HRT in hours
            if ( Node[treatmnt_->J].type == STORAGE )
            {
                return Storage[Node[treatmnt_->J].subIndex].hrt / 3600.0;
            }
            else return 0.0;

          case pvDT:
            return treatmnt_->Dt;                                // time step in seconds

          case pvFLOW:
            return treatmnt_->Q * UCF(FLOW);                     // flow in user's units

          case pvDEPTH:
            y = (Node[treatmnt_->J].oldDepth + Node[treatmnt_->J].newDepth) / 2.0;
            return y * UCF(LENGTH);                   // depth in ft or m

          case pvAREA:
            a1 = node_getSurfArea(treatmnt_->J, Node[treatmnt_->J].oldDepth);
            a2 = node_getSurfArea(treatmnt_->J, Node[treatmnt_->J].newDepth);
            return (a1 + a2) / 2.0 * UCF(LENGTH) * UCF(LENGTH);
            
          default: return 0.0;
        }
    }

    // --- variable is a pollutant concentration
    else if ( varCode < PVMAX + Nobjects[POLLUT] )
    {
        p = varCode - PVMAX;
        treatment = &Node[treatmnt_->J].treatment[p];
        if ( treatment->treatType == REMOVAL ) return treatmnt_->Cin[p];
        return Node[treatmnt_->J].newQual[p];
    }

    // --- variable is a pollutant removal
    else
    {
        p = varCode - PVMAX - Nobjects[POLLUT];
        if ( p >= Nobjects[POLLUT] ) return 0.0;
        return getRemoval(p);
    }
}

//=============================================================================

double  Project::getRemoval(int p)
//
//  Input:   p = pollutant index
//  Output:  returns fractional removal of pollutant
//  Purpose: computes removal of a specific pollutant
//
{
    double c0 = Node[treatmnt_->J].newQual[p];    // initial node concentration
    double r;                          // removal value
    TTreatment* treatment;

    // --- case where removal already being computed for another pollutant
    if ( treatmnt_->R[p] > 1.0 || treatmnt_->ErrCode )
    {
        treatmnt_->ErrCode = 1;
        return 0.0;
    }

    // --- case where removal already computed
    if ( treatmnt_->R[p] >= 0.0 && treatmnt_->R[p] <= 1.0 ) return treatmnt_->R[p];

    // --- set R[p] to value > 1 to show that value is being sought
    //     (prevents infinite recursive calls in case two removals
    //     depend on each other)
    treatmnt_->R[p] = 10.0;

    // --- case where current concen. is zero
    if ( c0 == 0.0 )
    {
        treatmnt_->R[p] = 0.0;
        return 0.0;
    }

    // --- apply treatment eqn.
    treatment = &Node[treatmnt_->J].treatment[p];
    r = mathexpr_eval(treatment->equation, &Project::treatmnt_getVariableValue);
    r = MAX(0.0, r);

    // --- case where treatment eqn. is for removal
    if ( treatment->treatType == REMOVAL )
    {
        r = MIN(1.0, r);
        treatmnt_->R[p] = r;
    }

    // --- case where treatment eqn. is for effluent concen.
    else
    {
        r = MIN(c0, r);
        treatmnt_->R[p] = 1.0 - r/c0;
    }
    return treatmnt_->R[p];
}
