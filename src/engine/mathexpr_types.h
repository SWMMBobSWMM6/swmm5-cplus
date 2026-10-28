// mathexpr_types.h -- module-local types of mathexpr.c, exported because they appear
// in member-function signatures of class Project.
#ifndef MATHEXPR_TYPES_H
#define MATHEXPR_TYPES_H

#define MAX_STACK_SIZE  1024

struct TreeNode
{
    int    opcode;                // operator code
    int    ivar;                  // variable index
    double fvalue;                // numerical value
    struct TreeNode *left;        // left sub-tree of tokenized formula
    struct TreeNode *right;       // right sub-tree of tokenized formula
};

typedef struct TreeNode ExprTree;

#endif
