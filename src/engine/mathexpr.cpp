/******************************************************************************
**  MODULE:        MATHEXPR.C
**  PROJECT:       EPA SWMM5
**  DESCRIPTION:   Evaluates symbolic mathematical expression consisting
**                 of numbers, variable names, math functions & arithmetic
**                 operators.
**  AUTHORS:       L. Rossman, US EPA - NRMRL
**                 F. Shang, University of Cincinnati
**  VERSION:       5.2.2
**  LAST UPDATE:   09/02/2022
**  BUG FIXES:     Problems related to '^' operator (F. Shang, 09/02/2022)
******************************************************************************/
/*
**   Operand codes:
** 	   1 = (
** 	   2 = )
** 	   3 = +
** 	   4 = - (subtraction)
** 	   5 = *
** 	   6 = /
** 	   7 = number
** 	   8 = user-defined variable
** 	   9 = - (negative)
**	  10 = cos
**	  11 = sin
**	  12 = tan
**	  13 = cot
**	  14 = abs
**	  15 = sgn
**	  16 = sqrt
**	  17 = log
**	  18 = exp
**	  19 = asin
**	  20 = acos
**	  21 = atan
**	  22 = acot
**        23 = sinh
**	  24 = cosh
**	  25 = tanh
**	  26 = coth
**	  27 = log10
**        28 = step (x<=0 ? 0 : 1)
**	  31 = ^
******************************************************************************/
#define _CRT_SECURE_NO_DEPRECATE

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "mathexpr.h"
#include "project.hpp"   // swmm5-cplus: functions are Project members

#define MAX_STACK_SIZE  1024

//  Local declarations
//--------------------
//  Structure for binary tree representation of math expression



// Local variables
//----------------









// math function names
char *MathFunc[] =  {"COS", "SIN", "TAN", "COT", "ABS", "SGN",
                     "SQRT", "LOG", "EXP", "ASIN", "ACOS", "ATAN",
                     "ACOT", "SINH", "COSH", "TANH", "COTH", "LOG10",
                     "STEP", NULL};

// Local functions
//----------------
















// Callback functions

// ---- mathexpr module state: formerly file-scope variables of mathexpr.c.
// ---- One instance per Project, so two models never share it.
struct MathexprState
{
    int Err{};   // was static
    int Bc{};   // was static
    int PrevLex{};   // was static
    int CurLex{};   // was static
    int Len{};   // was static
    int Pos{};   // was static
    char * S{};   // was static
    char Token[255]{};   // was static
    int Ivar{};   // was static
    double Fvalue{};   // was static
    using getVariableIndex_fn_t = int (Project::*)(char *);
    getVariableIndex_fn_t getVariableIndex{};   // was static
};
void Project::mathexpr_createState() { mathexpr_ = new MathexprState(); }
void Project::mathexpr_destroyState() { delete mathexpr_; mathexpr_ = nullptr; }
 // return index of named variable

//=============================================================================

int  Project::sametext(char *s1, char *s2)
/*
**  Purpose:
**    performs case insensitive comparison of two strings.
**
**  Input:
**    s1 = character string
**    s2 = character string.
**  
**  Returns:
**    1 if strings are the same, 0 otherwise.
*/
{
   int i;
   for (i=0; toupper(s1[i]) == toupper(s2[i]); i++)
     if (!s1[i+1] && !s2[i+1]) return(1);
   return(0);
}

//=============================================================================

int Project::isDigit(char c)
{
    if (c >= '1' && c <= '9') return 1;
    if (c == '0') return 1;
    return 0;
}

//=============================================================================

int Project::isLetter(char c)
{
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c == '_') return 1;
    return 0;
}

//=============================================================================

void Project::getToken()
{
    char c[] = " ";
    mathexpr_->Token[0] = '\0';
    while ( mathexpr_->Pos <= mathexpr_->Len &&
        ( isLetter(mathexpr_->S[mathexpr_->Pos]) || isDigit(mathexpr_->S[mathexpr_->Pos]) ) )
    {
        c[0] = mathexpr_->S[mathexpr_->Pos];
        strcat(mathexpr_->Token, c);
        mathexpr_->Pos++;
    }
    mathexpr_->Pos--;
}

//=============================================================================

int Project::getMathFunc()
{
    int i = 0;
    while (MathFunc[i] != NULL)
    {
        if (sametext(MathFunc[i], mathexpr_->Token)) return i+10;
        i++;
    }
    return(0);
}

//=============================================================================

int Project::getVariable()
{
    if ( !mathexpr_->getVariableIndex ) return 0;
    mathexpr_->Ivar = (this->*mathexpr_->getVariableIndex)(mathexpr_->Token);
    if (mathexpr_->Ivar >= 0) return 8;
    return 0;
}

//=============================================================================

double Project::getNumber()
{
    char c[] = " ";
    char sNumber[255];
    int  errflag = 0;
    int  decimalCount = 0;

    /* --- get whole number portion of number */
    sNumber[0] = '\0';
    while (mathexpr_->Pos < mathexpr_->Len && isDigit(mathexpr_->S[mathexpr_->Pos]))
    {
        c[0] = mathexpr_->S[mathexpr_->Pos];
        strcat(sNumber, c);
        mathexpr_->Pos++;
    }

    /* --- get fractional portion of number */
    if (mathexpr_->Pos < mathexpr_->Len)
    {
        if (mathexpr_->S[mathexpr_->Pos] == '.')
        {
            decimalCount++;
            if (decimalCount > 1) mathexpr_->Err = 1;
            strcat(sNumber, ".");
            mathexpr_->Pos++;
            while (mathexpr_->Pos < mathexpr_->Len && isDigit(mathexpr_->S[mathexpr_->Pos]))
            {
                c[0] = mathexpr_->S[mathexpr_->Pos];
                strcat(sNumber, c);  
                mathexpr_->Pos++;
            }
        }

        /* --- get exponent */
        if (mathexpr_->Pos < mathexpr_->Len && (mathexpr_->S[mathexpr_->Pos] == 'e' || mathexpr_->S[mathexpr_->Pos] == 'E'))
        {
            strcat(sNumber, "E");  
            mathexpr_->Pos++;
            if (mathexpr_->Pos >= mathexpr_->Len) errflag = 1;
            else
            {
                if (mathexpr_->S[mathexpr_->Pos] == '-' || mathexpr_->S[mathexpr_->Pos] == '+')
                {
                    c[0] = mathexpr_->S[mathexpr_->Pos];
                    strcat(sNumber, c);  
                    mathexpr_->Pos++;
                }
                if (mathexpr_->Pos >= mathexpr_->Len || !isDigit(mathexpr_->S[mathexpr_->Pos])) errflag = 1;
                else while ( mathexpr_->Pos < mathexpr_->Len && isDigit(mathexpr_->S[mathexpr_->Pos]))
                {
                    c[0] = mathexpr_->S[mathexpr_->Pos];
                    strcat(sNumber, c);  
                    mathexpr_->Pos++;
                }
            }
        }
    }
    mathexpr_->Pos--;
    if (errflag) return 0;
    else return atof(sNumber);
}

//=============================================================================

int Project::getOperand()
{
    int code;
    switch(mathexpr_->S[mathexpr_->Pos])
    {
      case '(': code = 1;  break;
      case ')': code = 2;  break;
      case '+': code = 3;  break;
      case '-': code = 4;
        if (mathexpr_->Pos < mathexpr_->Len-1 &&
            isDigit(mathexpr_->S[mathexpr_->Pos+1]) &&
            (mathexpr_->CurLex <= 6 || mathexpr_->CurLex == 31))
        {
            mathexpr_->Pos++;
            mathexpr_->Fvalue = -getNumber();
            code = 7;
        }
        break;
      case '*': code = 5;  break;
      case '/': code = 6;  break;
      case '^': code = 31; break;
      default:  code = 0;
    }
    return code;
}

//=============================================================================

int Project::getLex()
{
    int n;

    /* --- skip spaces */
    while ( mathexpr_->Pos < mathexpr_->Len && mathexpr_->S[mathexpr_->Pos] == ' ' ) mathexpr_->Pos++;
    if ( mathexpr_->Pos >= mathexpr_->Len ) return 0;

    /* --- check for operand */
    n = getOperand();

    /* --- check for function/variable/number */
    if ( n == 0 )
    {
        if ( isLetter(mathexpr_->S[mathexpr_->Pos]) )
        {
            getToken();
            n = getMathFunc();
            if ( n == 0 ) n = getVariable();
        }
        else if ( mathexpr_->S[mathexpr_->Pos] == '.' || isDigit(mathexpr_->S[mathexpr_->Pos]) )
        {
            n = 7;
            mathexpr_->Fvalue = getNumber();
        }
    }
    mathexpr_->Pos++;
    mathexpr_->PrevLex = mathexpr_->CurLex;
    mathexpr_->CurLex = n;
    return n;
}

//=============================================================================

ExprTree * Project::newNode()
{
    ExprTree *node;
    node = (ExprTree *) malloc(sizeof(ExprTree));
    if (!node) mathexpr_->Err = 2;
    else
    {
        node->opcode = 0;
        node->ivar   = -1;
        node->fvalue = 0.;
        node->left   = NULL;
        node->right  = NULL;
    }
    return node;
}

//=============================================================================

ExprTree * Project::getSingleOp(int *lex)
{
    int opcode;
    ExprTree *left;
    ExprTree *node;

    /* --- open parenthesis, so continue to grow the tree */
    if ( *lex == 1 )
    {
        mathexpr_->Bc++;
        left = getTree();
    }

    else
    {
        /* --- Error if not a singleton operand */
        if ( *lex < 7 || *lex == 9 || *lex > 30)
        {
            mathexpr_->Err = 1;
            return NULL;
        }

        opcode = *lex;

        /* --- simple number or variable name */
        if ( *lex == 7 || *lex == 8 )
        {
            left = newNode();
            left->opcode = opcode;
            if ( *lex == 7 ) left->fvalue = mathexpr_->Fvalue;
            if ( *lex == 8 ) left->ivar = mathexpr_->Ivar;
        }

        /* --- function which must have a '(' after it */
        else
        {
            *lex = getLex();
            if ( *lex != 1 )
            {
               mathexpr_->Err = 1;
               return NULL;
            }
            mathexpr_->Bc++;
            left = newNode();
            left->left = getTree();
            left->opcode = opcode;
        }
    }   
    *lex = getLex();

    /* --- exponentiation */
	if (*lex == 31)
	{
		node = newNode();
		node->left = left;
		node->opcode = *lex;
		*lex = getLex();
		node->right = getSingleOp(lex);
		left = node;
    }
    return left;
}

//=============================================================================

ExprTree * Project::getOp(int *lex)
{
    int opcode;
    ExprTree *left;
    ExprTree *right;
    ExprTree *node;
    int neg = 0;

    *lex = getLex();
    if (mathexpr_->PrevLex == 0 || mathexpr_->PrevLex == 1)
    {
        if ( *lex == 4 )
        {
            neg = 1;
            *lex = getLex();
        }
        else if ( *lex == 3) *lex = getLex();
    }
    left = getSingleOp(lex);
    while ( *lex == 5 || *lex == 6)
    {
        opcode = *lex;
        *lex = getLex();
        right = getSingleOp(lex);
        node = newNode();
        if (mathexpr_->Err) return NULL;
        node->left = left;
        node->right = right;
        node->opcode = opcode;
        left = node;
    }
    if ( neg )
    {
        node = newNode();
        if (mathexpr_->Err) return NULL;
        node->left = left;
        node->right = NULL;
        node->opcode = 9;
        left = node;
    }
    return left;
}

//=============================================================================

ExprTree * Project::getTree()
{
    int      lex;
    int      opcode;
    ExprTree *left;
    ExprTree *right;
    ExprTree *node;

    left = getOp(&lex);
    for (;;)
    {
        if ( lex == 0 || lex == 2 )
        {
            if ( lex == 2 ) mathexpr_->Bc--;
            break;
        }

        if (lex != 3 && lex != 4 )
        {
            mathexpr_->Err = 1;
            break;
        }

        opcode = lex;
        right = getOp(&lex);
        node = newNode();
        if (mathexpr_->Err) break;
        node->left = left;
        node->right = right;
        node->opcode = opcode;
        left = node;
    } 
    return left;
}

//=============================================================================

void Project::traverseTree(ExprTree *tree, MathExpr **expr)
// Converts binary tree to linked list (postfix format)
{
    MathExpr *node;
    if ( tree == NULL) return;
    traverseTree(tree->left,  expr);
    traverseTree(tree->right, expr);
    node = (MathExpr *) malloc(sizeof(MathExpr));
    if (node)
    {
        node->fvalue = tree->fvalue;
        node->opcode = tree->opcode;
        node->ivar = tree->ivar;
        node->next = NULL;
        node->prev = (*expr);
    }
    if (*expr) (*expr)->next = node;
    (*expr) = node;
}

//=============================================================================

void Project::deleteTree(ExprTree *tree)
{
    if (tree)
    {
        if (tree->left)  deleteTree(tree->left);
        if (tree->right) deleteTree(tree->right);
        free(tree);
    }
}

//=============================================================================

// Turn on "precise" floating point option
#pragma float_control(precise, on, push)

double Project::mathexpr_eval(MathExpr *expr, double (Project::*getVariableValue) (int))
//  Mathematica expression evaluation using a stack
{
    
// --- Note: the ExprStack array must be declared locally and not globally
//     since this function can be called recursively.

    double ExprStack[MAX_STACK_SIZE];
    MathExpr *node = expr;
    double r1, r2;
    int stackindex = 0;
    
    ExprStack[0] = 0.0;
    while(node != NULL && stackindex >= 0)
    {
	switch (node->opcode)
	{
	    case 3:
 		r1 = ExprStack[stackindex];
		stackindex--;
                if (stackindex < 0) break;
		r2 = ExprStack[stackindex];
		ExprStack[stackindex] = r2 + r1;
		break;

            case 4:  
                r1 = ExprStack[stackindex];
		stackindex--;
                if (stackindex < 0) break;
		r2 = ExprStack[stackindex];
		ExprStack[stackindex] = r2 - r1;
		break;

            case 5:  
                r1 = ExprStack[stackindex];
		stackindex--;
                if (stackindex < 0) break;
		r2 = ExprStack[stackindex];
		ExprStack[stackindex] = r2 * r1;
		break;

            case 6:  
		r1 = ExprStack[stackindex];
		stackindex--;
                if (stackindex < 0) break;
                r2 = ExprStack[stackindex];
		ExprStack[stackindex] = r2 / r1;
		break;				

            case 7:  
		stackindex++;
                if (stackindex >= MAX_STACK_SIZE) break;
		ExprStack[stackindex] = node->fvalue;
		break;

            case 8:
                if (getVariableValue != NULL)
                {
                    r1 = (this->*getVariableValue)(node->ivar);
                }
                else r1 = 0.0;
		stackindex++;
                if (stackindex >= MAX_STACK_SIZE) break;
                ExprStack[stackindex] = r1;
		break;

            case 9: 
		ExprStack[stackindex] = -ExprStack[stackindex];
		break;

            case 10: 
		r1 = ExprStack[stackindex];
		r2 = cos(r1);
		ExprStack[stackindex] = r2;
		break;

            case 11: 
		r1 = ExprStack[stackindex];
		r2 = sin(r1);
		ExprStack[stackindex] = r2;
		break;

            case 12: 
		r1 = ExprStack[stackindex];
		r2 = tan(r1);
		ExprStack[stackindex] = r2;
		break;

            case 13: 
		r1 = ExprStack[stackindex];
		if (r1 == 0.0) r2 = 0.0;
		else r2 = 1.0/tan( r1 );    
		ExprStack[stackindex] = r2;
		break;

            case 14: 
		r1 = ExprStack[stackindex];
		r2 = fabs( r1 );       
		ExprStack[stackindex] = r2;
		break;

            case 15: 
		r1 = ExprStack[stackindex];
		if (r1 < 0.0) r2 = -1.0;
		else if (r1 > 0.0) r2 = 1.0;
		else r2 = 0.0;
		ExprStack[stackindex] = r2;
		break;

            case 16: 
		r1 = ExprStack[stackindex];
		if (r1 < 0.0) r2 = 0.0;
		else r2 = sqrt( r1 );     
		ExprStack[stackindex] = r2;
		break;

            case 17: 
		r1 = ExprStack[stackindex];
		if (r1 <= 0) r2 = 0.0;
		else r2 = log(r1);
		ExprStack[stackindex] = r2;
		break;

            case 18: 
		r1 = ExprStack[stackindex];
		r2 = exp(r1);
		ExprStack[stackindex] = r2;
		break;

            case 19: 
		r1 = ExprStack[stackindex];
		r2 = asin( r1 );
		ExprStack[stackindex] = r2;
		break;

            case 20: 
		r1 = ExprStack[stackindex];
		r2 = acos( r1 );      
		ExprStack[stackindex] = r2;
		break;

            case 21: 
		r1 = ExprStack[stackindex];
		r2 = atan( r1 );      
		ExprStack[stackindex] = r2;
		break;

            case 22: 
		r1 = ExprStack[stackindex];
		r2 = 1.57079632679489661923 - atan(r1);  
		ExprStack[stackindex] = r2;
		break;

            case 23:
		r1 = ExprStack[stackindex];
		r2 = (exp(r1)-exp(-r1))/2.0;
		ExprStack[stackindex] = r2;
		break;

            case 24: 
		r1 = ExprStack[stackindex];
		r2 = (exp(r1)+exp(-r1))/2.0;
		ExprStack[stackindex] = r2;
		break;

            case 25: 
		r1 = ExprStack[stackindex];
		r2 = (exp(r1)-exp(-r1))/(exp(r1)+exp(-r1));
		ExprStack[stackindex] = r2;
		break;

            case 26: 
		r1 = ExprStack[stackindex];
		r2 = (exp(r1)+exp(-r1))/(exp(r1)-exp(-r1));
		ExprStack[stackindex] = r2;
		break;

            case 27: 
		r1 = ExprStack[stackindex];
		if (r1 == 0.0) r2 = 0.0;
		else r2 = log10( r1 );     
		ExprStack[stackindex] = r2;
		break;

            case 28:
 		r1 = ExprStack[stackindex];
		if (r1 <= 0.0) r2 = 0.0;
		else           r2 = 1.0;
		ExprStack[stackindex] = r2;
		break;
               
            case 31: 
		r1 = ExprStack[stackindex];
                stackindex--;
                if (stackindex < 0) break;
                r2 = ExprStack[stackindex];
		if (r2 <= 0.0) r2 = 0.0;
		else r2 = pow(r2, r1);
		ExprStack[stackindex] = r2;
		break;
        }
        node = node->next;
    }
    if (stackindex >= 0)
        r1 = ExprStack[stackindex];
    else
        r1 = 0.0;

    // Set result to 0 if it is NaN due to an illegal math op
    if ( r1 != r1 ) r1 = 0.0;

    return r1;
}

// Turn off "precise" floating point option
#pragma float_control(pop)

//=============================================================================

void Project::mathexpr_delete(MathExpr *expr)
{
    if (expr) mathexpr_delete(expr->next);
    free(expr);
}

//=============================================================================

MathExpr * Project::mathexpr_create(char *formula, int (Project::*getVar) (char *))
{
    ExprTree *tree;
    MathExpr *expr = NULL;
    MathExpr *result = NULL;
    mathexpr_->getVariableIndex = getVar;
    mathexpr_->Err = 0;
    mathexpr_->PrevLex = 0;
    mathexpr_->CurLex = 0;
    mathexpr_->S = formula;
    mathexpr_->Len = (int)strlen(mathexpr_->S);
    mathexpr_->Pos = 0;
    mathexpr_->Bc = 0;
    tree = getTree();
    if (mathexpr_->Bc == 0 && mathexpr_->Err == 0)
    {
        traverseTree(tree, &expr);
        while (expr)
        {
            result = expr;
            expr = expr->prev;
        }
    }
    deleteTree(tree);
    return result;
}
