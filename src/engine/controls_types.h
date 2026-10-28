// controls_types.h -- module-local types of controls.c, exported because they appear
// in member-function signatures of class Project.
#ifndef CONTROLS_TYPES_H
#define CONTROLS_TYPES_H

#define MAXVARNAME  32

enum RuleState    {r_RULE, r_IF, r_AND, r_OR, r_THEN, r_ELSE, r_PRIORITY,
                   r_VARIABLE, r_EXPRESSION, r_ERROR};

enum RuleObject   {r_GAGE, r_NODE, r_LINK, r_CONDUIT, r_PUMP, r_ORIFICE,
                   r_WEIR, r_OUTLET, r_SIMULATION};

enum RuleAttrib   {r_DEPTH, r_MAXDEPTH, r_HEAD, r_VOLUME, r_INFLOW,
                   r_FLOW, r_FULLFLOW, r_FULLDEPTH, r_STATUS, r_SETTING,
                   r_LENGTH, r_SLOPE, r_VELOCITY, r_TIMEOPEN, r_TIMECLOSED,
                   r_TIME, r_DATE, r_CLOCKTIME, r_DAYOFYEAR, r_DAY, r_MONTH};

enum RuleRelation {EQ, NE, LT, LE, GT, GE};

enum RuleSetting  {r_CURVE, r_TIMESERIES, r_PID, r_NUMERIC};

struct TVariable
{
   int      object;          // type of object
   int      index;           // index in object's array
   int      attribute;       // object's attribute
};

struct TNamedVariable
{
    struct TVariable variable;           // a rule premise variable 
    char             name[MAXVARNAME+1]; // name used in math expression
};

struct TExpression
{
    MathExpr*  expression;               // tokenized math expression
    char       name[MAXVARNAME+1];       // expression name
};

struct  TPremise
{
    int     type;                 // clause type (IF/AND/OR)
    int     exprIndex;            // expression index (-1 if N/A)
    struct  TVariable lhsVar;     // left hand side variable
    struct  TVariable rhsVar;     // right hand side variable 
    int     relation;             // relational operator (>, <, =, etc)
    double  value;                // right hand side value
    struct  TPremise *next;       // next premise clause of rule
};

struct  TAction              
{
   int     rule;             // index of rule that action belongs to
   int     link;             // index of link being controlled
   int     attribute;        // attribute of link being controlled
   int     curve;            // index of curve for modulated control
   int     tseries;          // index of time series for modulated control
   double  value;            // control setting for link attribute
   double  kp, ki, kd;       // coeffs. for PID modulated control
   double  e1, e2;           // PID set point error from previous time steps
   struct  TAction *next;    // next action clause of rule
};

struct  TActionList          
{
   struct  TAction* action;
   struct  TActionList* next;
};

struct  TRule
{
   char*    ID;                        // rule ID
   double   priority;                  // priority level
   struct   TPremise* firstPremise;    // pointer to first premise of rule
   struct   TPremise* lastPremise;     // pointer to last premise of rule
   struct   TAction*  thenActions;     // linked list of actions if true
   struct   TAction*  elseActions;     // linked list of actions if false
};

#endif
