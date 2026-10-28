/******************************************************************************
**  MODULE:        MATHEXPR.H
**  PROJECT:       SWMM5
**  DESCRIPTION:   header file for the math expression parser in mathexpr.c.
**  AUTHORS:       L. Rossman, US EPA - NRMRL
**                 F. Shang, University of Cincinnati
**  VERSION:       5.1.001
**  LAST UPDATE:   03/20/14
******************************************************************************/

#ifndef MATHEXPR_H
#define MATHEXPR_H

#include <functional>


//  Node in a tokenized math expression list
struct ExprNode
{
    int    opcode;                // operator code
    int    ivar;                  // variable index
    double fvalue;                // numerical value
	struct ExprNode *prev;        // previous node
    struct ExprNode *next;        // next node
};
typedef struct ExprNode MathExpr;

//  Creates a tokenized math expression from a string


//  Evaluates a tokenized math expression


//  Deletes a tokenized math expression



// swmm5-cplus: free functions with callable callbacks (no Project needed)
using MathExprVarIndex = std::function<int(char*)>;     // variable name  -> index (or -1)
using MathExprVarValue = std::function<double(int)>;    // variable index -> current value

MathExpr* mathexpr_create(char* s, const MathExprVarIndex& getVar);
double    mathexpr_eval(MathExpr* expr, const MathExprVarValue& getVal);
void      mathexpr_delete(MathExpr* expr);

#endif //MATHEXPR_H
