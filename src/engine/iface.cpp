//-----------------------------------------------------------------------------
//   iface.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     11/01/21 (Build 5.2.0)
//   Author:   L. Rossman
//
//   Routing interface file functions.
//
//   Build 5.2.0:
//   - Support added for relative file names.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <string.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Imported variables
//-----------------------------------------------------------------------------
extern double Qcf[];                   // flow units conversion factors
                                       // (see swmm5.c)

//-----------------------------------------------------------------------------                  
//  Shared variables
//-----------------------------------------------------------------------------                  
        // flow units for routing interface file
             // interface file time step (sec)
       // number of pollutants in interface file
          // indexes of interface file pollutants
         // number of nodes on interface file
            // indexes of nodes on interface file
        // interface flows & WQ at previous time
        // interface flows & WQ at next time
             // fraction of interface file time step
          // previous date of interface values

// ---- iface module state: formerly file-scope variables of iface.c.
// ---- One instance per Project, so two models never share it.
struct IfaceState
{
    int IfaceFlowUnits{};   // was static
    int IfaceStep{};   // was static
    int NumIfacePolluts{};   // was static
    int * IfacePolluts{};   // was static
    int NumIfaceNodes{};   // was static
    int * IfaceNodes{};   // was static
    double ** OldIfaceValues{};   // was static
    double ** NewIfaceValues{};   // was static
    double IfaceFrac{};   // was static
    DateTime OldIfaceDate{};   // was static
    DateTime NewIfaceDate{};   // was static
};
void Project::iface_createState() { iface_ = new IfaceState(); }
void Project::iface_destroyState() { delete iface_; iface_ = nullptr; }
          // next date of interface values

//-----------------------------------------------------------------------------
//  External Functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  iface_readFileParams     (called by input_readLine)
//  iface_openRoutingFiles   (called by routing_open)
//  iface_closeRoutingFiles  (called by routing_close)
//  iface_getNumIfaceNodes   (called by addIfaceInflows in routing.c)
//  iface_getIfaceNode       (called by addIfaceInflows in routing.c)
//  iface_getIfaceFlow       (called by addIfaceInflows in routing.c)
//  iface_getIfaceQual       (called by addIfaceInflows in routing.c)
//  iface_saveOutletResults  (called by output_saveResults)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------








//=============================================================================

int Project::iface_readFileParams(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads interface file information from a line of input data.
//
//  Data format is:
//  USE/SAVE  FileType  FileName
//
{
    char  k;
    int   j;
    char  fname[MAXFNAME+1];

    // --- determine file disposition and type
    if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
    k = (char)findmatch(tok[0], FileModeWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
    j = findmatch(tok[1], FileTypeWords);
    if ( j < 0 ) return error_setInpError(ERR_KEYWORD, tok[1]);
    if ( ntoks < 3 ) return 0;
    sstrncpy(fname, tok[2], MAXFNAME);

    // --- process file name
    switch ( j )
    {
      case RAINFALL_FILE:
        Frain.mode = k;
        sstrncpy(Frain.name, addAbsolutePath(fname), MAXFNAME);
        break;

      case RUNOFF_FILE:
        Frunoff.mode = k;
        sstrncpy(Frunoff.name, addAbsolutePath(fname), MAXFNAME);
        break;

      case HOTSTART_FILE:
        if ( k == USE_FILE )
        {
            Fhotstart1.mode = k;
            sstrncpy(Fhotstart1.name, addAbsolutePath(fname), MAXFNAME);
        }
        else if ( k == SAVE_FILE )
        {
            Fhotstart2.mode = k;
            sstrncpy(Fhotstart2.name, addAbsolutePath(fname), MAXFNAME);
        }
        break;

      case RDII_FILE:
        Frdii.mode = k;
        sstrncpy(Frdii.name, fname, MAXFNAME);
        break;

      case INFLOWS_FILE:
        if ( k != USE_FILE ) return error_setInpError(ERR_ITEMS, "");
        Finflows.mode = k;
        sstrncpy(Finflows.name, addAbsolutePath(fname), MAXFNAME);
        break;

      case OUTFLOWS_FILE:
        if ( k != SAVE_FILE ) return error_setInpError(ERR_ITEMS, "");
        Foutflows.mode = k;
        sstrncpy(Foutflows.name, addAbsolutePath(fname), MAXFNAME);
        break;
    }
    return 0;
}

//=============================================================================

void Project::iface_openRoutingFiles()
//
//  Input:   none
//  Output:  none
//  Purpose: opens routing interface files.
//
{
    // --- initialize shared variables
    iface_->NumIfacePolluts = 0;
    iface_->IfacePolluts = NULL;
    iface_->NumIfaceNodes = 0;
    iface_->IfaceNodes = NULL;
    iface_->OldIfaceValues = NULL;
    iface_->NewIfaceValues = NULL;

    // --- check that inflows & outflows files are not the same
    if ( Foutflows.mode != NO_FILE && Finflows.mode != NO_FILE )
    {
        if ( strcomp(Foutflows.name, Finflows.name) )
        {
            report_writeErrorMsg(ERR_ROUTING_FILE_NAMES, "");
            return;
        }
    }

    // --- open the file for reading or writing
    if ( Foutflows.mode == SAVE_FILE ) openFileForOutput();
    if ( Finflows.mode == USE_FILE ) openFileForInput();
}

//=============================================================================

void Project::iface_closeRoutingFiles()
//
//  Input:   none
//  Output:  none
//  Purpose: closes routing interface files.
//
{
    FREE(iface_->IfacePolluts);
    FREE(iface_->IfaceNodes);
    if ( iface_->OldIfaceValues != NULL ) project_freeMatrix(iface_->OldIfaceValues);
    if ( iface_->NewIfaceValues != NULL ) project_freeMatrix(iface_->NewIfaceValues);
    if ( Finflows.file )  fclose(Finflows.file);
    if ( Foutflows.file ) fclose(Foutflows.file);
}

//=============================================================================

int Project::iface_getNumIfaceNodes(DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  returns number of interface nodes if data exists or
//           0 otherwise
//  Purpose: reads inflow data from interface file at current date.
//
{
    // --- return 0 if file begins after current date
    if ( iface_->OldIfaceDate > currentDate ) return 0;

    // --- keep updating new interface values until current date bracketed
    while ( iface_->NewIfaceDate < currentDate && iface_->NewIfaceDate != NO_DATE )
    {
        setOldIfaceValues();
        readNewIfaceValues();
    }

    // --- return 0 if no data available
    if ( iface_->NewIfaceDate == NO_DATE ) return 0;

    // --- find fraction current date is bewteen old & new interface dates
    iface_->IfaceFrac = (currentDate - iface_->OldIfaceDate) / (iface_->NewIfaceDate - iface_->OldIfaceDate);
    iface_->IfaceFrac = MAX(0.0, iface_->IfaceFrac);
    iface_->IfaceFrac = MIN(iface_->IfaceFrac, 1.0);

    // --- return number of interface nodes
    return iface_->NumIfaceNodes;
}

//=============================================================================

int Project::iface_getIfaceNode(int index)
//
//  Input:   index = interface file node index
//  Output:  returns project node index
//  Purpose: finds index of project node associated with interface node index
//
{
    if ( index >= 0 && index < iface_->NumIfaceNodes ) return iface_->IfaceNodes[index];
    else return -1;
}

//=============================================================================

double Project::iface_getIfaceFlow(int index)
//
//  Input:   index = interface file node index
//  Output:  returns inflow to node
//  Purpose: finds interface flow for particular node index.
//
{
    double q1, q2;

    if ( index >= 0 && index < iface_->NumIfaceNodes )
    {
        // --- interpolate flow between old and new values
        q1 = iface_->OldIfaceValues[index][0];
        q2 = iface_->NewIfaceValues[index][0];
        return (1.0 - iface_->IfaceFrac)*q1 + iface_->IfaceFrac*q2;
    }
    else return 0.0;
}

//=============================================================================

double Project::iface_getIfaceQual(int index, int pollut)
//
//  Input:   index = index of node on interface file
//           pollut = index of pollutant on interface file
//  Output:  returns inflow pollutant concentration
//  Purpose: finds interface concentration for particular node index & pollutant.
//
{
    int    j;
    double c1, c2;

    if ( index >= 0 && index < iface_->NumIfaceNodes )
    {
        // --- find index of pollut on interface file
        j = iface_->IfacePolluts[pollut];
        if ( j < 0 ) return 0.0;

        // --- interpolate flow between old and new values
        //     (remember that 1st col. of values matrix is for flow)
        c1 = iface_->OldIfaceValues[index][j+1];
        c2 = iface_->NewIfaceValues[index][j+1];
        return (1.0 - iface_->IfaceFrac)*c1 + iface_->IfaceFrac*c2;
    }
    else return 0.0;
}

//=============================================================================

void Project::iface_saveOutletResults(DateTime reportDate, FILE* file)
//
//  Input:   reportDate = reporting date/time
//           file = ptr. to interface file
//  Output:  none
//  Purpose: saves system outflows to routing interface file.
//
{
    int i, p, yr, mon, day, hr, min, sec;
    char theDate[26];
    datetime_decodeDate(reportDate, &yr, &mon, &day);
    datetime_decodeTime(reportDate, &hr, &min, &sec);
    snprintf(theDate, 26, " %04d %02d  %02d  %02d  %02d  %02d ",
            yr, mon, day, hr, min, sec);
    for (i=0; i<Nobjects[NODE]; i++)
    {
        // --- check that node is an outlet node
        if ( !isOutletNode(i) ) continue;

        // --- write node ID, date, flow, and quality to file
        fprintf(file, "\n%-16s", Node[i].ID);
        fprintf(file, "%s", theDate);
        fprintf(file, " %-10f", Node[i].inflow * UCF(FLOW));
        for ( p = 0; p < Nobjects[POLLUT]; p++ )
        {
            fprintf(file, " %-10f", Node[i].newQual[p]);
        }
    }
}

//=============================================================================

void Project::openFileForOutput()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a routing interface file for writing.
//
{
    int i, n;

    // --- open the routing file for writing text
    Foutflows.file = fopen(Foutflows.name, "wt");
    if ( Foutflows.file == NULL )
    {
        report_writeErrorMsg(ERR_ROUTING_FILE_OPEN, Foutflows.name);
        return;
    }

    // --- write title & reporting time step to file
    fprintf(Foutflows.file, "SWMM5 Interface File");
    fprintf(Foutflows.file, "\n%s", Title[0]);
    fprintf(Foutflows.file, "\n%-4d - reporting time step in sec", ReportStep);

    // --- write number & names of each constituent (including flow) to file
    fprintf(Foutflows.file, "\n%-4d - number of constituents as listed below:",
            Nobjects[POLLUT] + 1);
    fprintf(Foutflows.file, "\nFLOW %s", FlowUnitWords[FlowUnits]);
    for (i=0; i<Nobjects[POLLUT]; i++)
    {
        fprintf(Foutflows.file, "\n%s %s", Pollut[i].ID,
            QualUnitsWords[Pollut[i].units]);
    }

    // --- count number of outlet nodes
    n = 0;
    for (i=0; i<Nobjects[NODE]; i++)
    {
        if ( isOutletNode(i) ) n++;
    }

    // --- write number and names of outlet nodes to file
    fprintf(Foutflows.file, "\n%-4d - number of nodes as listed below:", n);
    for (i=0; i<Nobjects[NODE]; i++)
    {
          if ( isOutletNode(i) )
            fprintf(Foutflows.file, "\n%s", Node[i].ID);
    }

    // --- write column headings
    fprintf(Foutflows.file,
        "\nNode             Year Mon Day Hr  Min Sec FLOW      ");
    for (i=0; i<Nobjects[POLLUT]; i++)
    {
        fprintf(Foutflows.file, " %-10s", Pollut[i].ID);
    }

    // --- if reporting starts immediately, save initial outlet values
    if ( ReportStart == StartDateTime )
    {
        iface_saveOutletResults(ReportStart, Foutflows.file);
    }
}

//=============================================================================

void Project::openFileForInput()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a routing interface file for reading.
//
{
    int   err;                         // error code
    char  line[MAXLINE+1];             // line from Routing interface file
    char  s[MAXLINE+1];                // general string variable

    // --- open the routing interface file for reading text
    Finflows.file = fopen(Finflows.name, "rt");
    if ( Finflows.file == NULL )
    {
        report_writeErrorMsg(ERR_ROUTING_FILE_OPEN, Finflows.name);
        return;
    }

    // --- check for correct file type
    fgets(line, MAXLINE, Finflows.file);
    if ( !sscanf(line, "%s", s) || !strcomp(s, "SWMM5") )
    {
        report_writeErrorMsg(ERR_ROUTING_FILE_FORMAT, Finflows.name);
        return;
    }

    // --- skip title line
    fgets(line, MAXLINE, Finflows.file);

    // --- read reporting time step (sec)
    iface_->IfaceStep = 0;
    fgets(line, MAXLINE, Finflows.file);
    if ( !sscanf(line, "%d", &iface_->IfaceStep) || iface_->IfaceStep <= 0 )
    {
        report_writeErrorMsg(ERR_ROUTING_FILE_FORMAT, Finflows.name);
        return;
    }

    // --- match constituents in file with those in project
    err = getIfaceFilePolluts();
    if ( err > 0 )
    {
        report_writeErrorMsg(err, Finflows.name);
        return;
    }

    // --- match nodes in file with those in project
    err = getIfaceFileNodes();
    if ( err > 0 )
    {
        report_writeErrorMsg(err, Finflows.name);
        return;
    }

    // --- create matrices for old & new interface flows & WQ values
    iface_->OldIfaceValues = project_createMatrix(iface_->NumIfaceNodes,
                                         1+iface_->NumIfacePolluts);
    iface_->NewIfaceValues = project_createMatrix(iface_->NumIfaceNodes,
                                         1+iface_->NumIfacePolluts);
    if ( iface_->OldIfaceValues == NULL || iface_->NewIfaceValues == NULL )
    {
        report_writeErrorMsg(ERR_MEMORY, "");
        return;
    }

    // --- read in new interface flows & WQ values
    readNewIfaceValues();
    iface_->OldIfaceDate = iface_->NewIfaceDate;
}

//=============================================================================

int  Project::getIfaceFilePolluts()
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: reads names of pollutants saved on the inflows interface file.
//
{
    int   i, j;
    char  line[MAXLINE+1];             // line from inflows interface file
    char  s1[MAXLINE+1];               // general string variable
    char  s2[MAXLINE+1];         

    // --- read number of pollutants (minus FLOW)
    fgets(line, MAXLINE, Finflows.file);
    iface_->NumIfacePolluts = -1;
    if (sscanf(line, "%d", &iface_->NumIfacePolluts))
        iface_->NumIfacePolluts--;
    if ( iface_->NumIfacePolluts < 0 ) return ERR_ROUTING_FILE_FORMAT;

    // --- read flow units
    fgets(line, MAXLINE, Finflows.file);
    if (sscanf(line, "%s %s", s1, s2) < 2)
        return ERR_ROUTING_FILE_FORMAT;
    if ( !strcomp(s1, "FLOW") )
        return ERR_ROUTING_FILE_FORMAT;
    iface_->IfaceFlowUnits = findmatch(s2, FlowUnitWords);
    if ( iface_->IfaceFlowUnits < 0 )
        return ERR_ROUTING_FILE_FORMAT;

    // --- allocate memory for pollutant index array
    if ( Nobjects[POLLUT] > 0 )
    {
        iface_->IfacePolluts = (int *) calloc(Nobjects[POLLUT], sizeof(int));
        if ( !iface_->IfacePolluts ) return ERR_MEMORY;
        for (i=0; i<Nobjects[POLLUT]; i++) iface_->IfacePolluts[i] = -1;
    }

    // --- read pollutant names & units
    if ( iface_->NumIfacePolluts > 0 && Nobjects[POLLUT] > 0 )
    {
        // --- check each pollutant name on file with project's pollutants
        for (i=0; i<iface_->NumIfacePolluts; i++)
        {
            if ( feof(Finflows.file) )
                return ERR_ROUTING_FILE_FORMAT;
            fgets(line, MAXLINE, Finflows.file);
            if ( sscanf(line, "%s %s", s1, s2) < 2 )
                return ERR_ROUTING_FILE_FORMAT;
            j = project_findObject(POLLUT, s1);
            if ( j < 0 ) continue;
            if ( !strcomp(s2, QualUnitsWords[Pollut[j].units]) )
                return ERR_ROUTING_FILE_NOMATCH;
            iface_->IfacePolluts[j] = i;
        }
    }
    return 0;
}

//=============================================================================

int Project::getIfaceFileNodes()
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: reads names of nodes contained on inflows interface file.
//
{
    int   i;
    char  line[MAXLINE+1];             // line from inflows interface file
    char  s[MAXLINE+1];                // general string variable

    // --- read number of interface nodes
    fgets(line, MAXLINE, Finflows.file);
    if ( !sscanf(line, "%d", &iface_->NumIfaceNodes) || iface_->NumIfaceNodes <= 0 )
        return ERR_ROUTING_FILE_FORMAT;

    // --- allocate memory for interface nodes index array
    iface_->IfaceNodes = (int *) calloc(iface_->NumIfaceNodes, sizeof(int));
    if ( !iface_->IfaceNodes ) return ERR_MEMORY;

    // --- read names of interface nodes from file & save their indexes
    for ( i=0; i<iface_->NumIfaceNodes; i++ )
    {
        if ( feof(Finflows.file) )
            return ERR_ROUTING_FILE_FORMAT;
        iface_->IfaceNodes[i] = 0;
        fgets(line, MAXLINE, Finflows.file);
        if (!sscanf(line, "%s", s))
            return ERR_ROUTING_FILE_FORMAT;
        iface_->IfaceNodes[i] = project_findObject(NODE, s);
    }

    // --- skip over column headings line
    if ( feof(Finflows.file) ) return ERR_ROUTING_FILE_FORMAT;
    fgets(line, MAXLINE, Finflows.file);
    return 0;
}

//=============================================================================

void Project::readNewIfaceValues()
//
//  Input:   none
//  Output:  none
//  Purpose: reads data from inflows interface file for next date.
//
{
    int    i, j;
    char*  s;
    int    yr = 0, mon = 0, day = 0,
		   hr = 0, min = 0, sec = 0;   // year, month, day, hour, minute, second
    char   line[MAXLINE+1];            // line from interface file

    // --- read a line for each interface node
    iface_->NewIfaceDate = NO_DATE;
    for (i=0; i<iface_->NumIfaceNodes; i++)
    {
        if ( feof(Finflows.file) ) return;
        fgets(line, MAXLINE, Finflows.file);

        // --- parse date & time from line
        if ( strtok(line, SEPSTR) == NULL ) return;
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        yr  = atoi(s);
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        mon = atoi(s);
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        day = atoi(s);
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        hr  = atoi(s);
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        min = atoi(s);
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        sec = atoi(s);

        // --- parse flow value
        s = strtok(NULL, SEPSTR);
        if ( s == NULL ) return;
        iface_->NewIfaceValues[i][0] = atof(s) / Qcf[iface_->IfaceFlowUnits]; 

        // --- parse pollutant values
        for (j=1; j<=iface_->NumIfacePolluts; j++)
        {
            s = strtok(NULL, SEPSTR);
            if ( s == NULL ) return;
            iface_->NewIfaceValues[i][j] = atof(s);
        }

    }

    // --- encode date & time values
    iface_->NewIfaceDate = datetime_encodeDate(yr, mon, day) +
                   datetime_encodeTime(hr, min, sec);
}

//=============================================================================

void Project::setOldIfaceValues()
//
//  Input:   none
//  Output:  none
//  Purpose: replaces old values read from routing interface file with new ones. 
//
{
    int i, j;
    iface_->OldIfaceDate = iface_->NewIfaceDate;
    for ( i=0; i<iface_->NumIfaceNodes; i++)
    {
        for ( j=0; j<iface_->NumIfacePolluts+1; j++ )
        {
            iface_->OldIfaceValues[i][j] = iface_->NewIfaceValues[i][j];
        }
    }
}

//=============================================================================

int  Project::isOutletNode(int i)
//
//  Input:   i = node index
//  Output:  returns 1 if node is an outlet, 0 if not.
//  Purpose: determines if a node is an outlet point or not.
//
{
    // --- for DW routing only outfalls are outlets
    if ( RouteModel == DW )
    {
        return (Node[i].type == OUTFALL);
    }

    // --- otherwise outlets are nodes with no outflow links (degree is 0)
    else return (Node[i].degree == 0);
}
