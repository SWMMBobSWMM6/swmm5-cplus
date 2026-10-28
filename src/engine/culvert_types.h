// culvert_types.h -- module-local types of culvert.c, exported because they appear
// in member-function signatures of class Project.
#ifndef CULVERT_TYPES_H
#define CULVERT_TYPES_H



enum CulvertParam {FORM, K, M, C, Y};

typedef struct
{
    double  yFull;                  // full depth of culvert (ft)
    double  scf;                    // slope correction factor
    double  dQdH;                   // Derivative of flow w.r.t. head
    double  qc;                     // Unsubmerged critical flow
    double  kk;
	double  mm;                     // Coeffs. for unsubmerged flow
    double  ad;
	double  hPlus;                  // Intermediate terms
    TXsect* xsect;                  // Pointer to culvert cross section
} TCulvert;

#endif
