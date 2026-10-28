//-----------------------------------------------------------------------------
//   climate.c
//
//   Project: EPA SWMM5
//   Version: 5.2
//   Date:    11/01/21  (Build 5.2.0)
//   Author:  L. Rossman
//
//   Climate related functions.
//
//   Update History
//   ==============
//   Build 5.1.007:
//   - NCDC GHCN climate file format added.
//   - Monthly adjustments for temperature, evaporation & rainfall added.
//   Build 5.1.008:
//   - Monthly adjustments for hyd. conductivity added.
//   - Time series evaporation rates can now vary within a day.
//   - Evaporation rates are now properly updated when only flow routing
//     is being simulated.
//   Build 5.1.010:
//   - Hargreaves evaporation now computed using 7-day average temperatures.
//   Build 5.1.011:
//   - Monthly adjustment for hyd. conductivity <= 0 is ignored.
//   Build 5.1.013:
//   - Reads names of monthly adjustment patterns for various parameters
//     of a subcatchment from the [ADJUSTMENTS] section of input file.
//   Build 5.2.0:
//   - Reads temperature units for use with GHCND climate files.
//   - Support added for relative file names.
///-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum ClimateFileFormats {UNKNOWN_FORMAT,
                         USER_PREPARED,     // SWMM 5's own user format
                         GHCND,             // NCDC GHCN Daily format
                         TD3200,            // NCDC TD3200 format
                         DLY0204};          // Canadian DLY02 or DLY04 format
static const int    MAXCLIMATEVARS  = 4;
static const int    MAXDAYSPERMONTH = 32;

// These variables are used when processing climate files.
enum   ClimateVarType {TMIN, TMAX, EVAP, WIND};
enum   WindSpeedType  {WDMV, AWND};
enum   TempUnitsType {DEG_C10, DEG_C, DEG_F};
static char* ClimateVarWords[] = {"TMIN", "TMAX", "EVAP", "WDMV", "AWND",
                                  NULL};
static char* TempUnitsWords[] = {"C10", "C", "F", NULL};

//-----------------------------------------------------------------------------
//  Data Structures
//-----------------------------------------------------------------------------
typedef struct
{
    double    tAve;          // moving avg. for daily temperature (deg F)
    double    tRng;          // moving avg. for daily temp. range (deg F)
    double    ta[7];         // data window for tAve
    double    tr[7];         // data window for tRng
    int       count;         // length of moving average window
    int       maxCount;      // maximum length of moving average window
    int       front;         // index of front of moving average window
} TMovAve;


//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
// Temperature variables
                 // min. daily temperature (deg F)
                 // max. daily temperature (deg F)
                 // 1/2 range of daily temperatures
                // prev. max - current min. temp.
                 // average daily temperature (deg F)
                 // time of min. temp. (hrs)
                 // time of max. temp (hrs)
                // avg. of min/max temp times
                // hrs. between min. & max. temp. times
                // hrs. between max. & min. temp. times
              // date of last day with temp. data
                  // moving average of daily temperatures

// Evaporation variables
         // next date when evap. rate changes
         // next evaporation rate (user units)

// Climate file variables
            // file format (see ClimateFileFormats)
              // current year of file data
             // current month of year of file data
               // current day of month of file data
           // last day of current month of file data
       // number of days read from file
          // current day's values of climate data
       // month's worth of daily climate data
   // line from climate data file

       // start of data fields for file record
      // start of date field for file record 
          // wind speed type

// ---- climate module state: formerly file-scope variables of climate.c.
// ---- One instance per Project, so two models never share it.
struct ClimateState
{
    double Tmin{};   // was static
    double Tmax{};   // was static
    double Trng{};   // was static
    double Trng1{};   // was static
    double Tave{};   // was static
    double Hrsr{};   // was static
    double Hrss{};   // was static
    double Hrday{};   // was static
    double Dhrdy{};   // was static
    double Dydif{};   // was static
    DateTime LastDay{};   // was static
    TMovAve Tma{};   // was static
    DateTime NextEvapDate{};   // was static
    double NextEvapRate{};   // was static
    int FileFormat{};   // was static
    int FileYear{};   // was static
    int FileMonth{};   // was static
    int FileDay{};   // was static
    int FileLastDay{};   // was static
    int FileElapsedDays{};   // was static
    double FileValue[4]{};   // was static
    double FileData[4][32]{};   // was static
    char FileLine[1025]{};   // was static
    int FileFieldPos[4]{};   // was static
    int FileDateFieldPos{};   // was static
    int FileWindType{};   // was static
    int FileTempUnits{};   // was static
};
void Project::climate_createState() { climate_ = new ClimateState(); }
void Project::climate_destroyState() { delete climate_; climate_ = nullptr; }
         // GHCND file temperature units (C10, C or F)

//-----------------------------------------------------------------------------
//  External functions (defined in funcs.h)
//-----------------------------------------------------------------------------
//  climate_readParams                 // called by input_parseLine
//  climate_readEvapParams             // called by input_parseLine
//  climate_validate                   // called by project_validate
//  climate_openFile                   // called by runoff_open
//  climate_initState                  // called by project_init
//  climate_setState                   // called by runoff_execute
//  climate_getNextEvapDate            // called by runoff_getTimeStep

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------


























//=============================================================================

int  Project::climate_readParams(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads climate/temperature parameters from input line of data
//
//  Format of data can be
//    TIMESERIES  name
//    FILE        name  (start)  (units)
//    WINDSPEED   MONTHLY  v1  v2  ...  v12
//    WINDSPEED   FILE
//    SNOWMELT    v1  v2  ...  v6
//    ADC         IMPERV/PERV  v1  v2  ...  v10
//
{
    int      i, j, k;
    double   x[6], y;
    DateTime aDate;
    char  fname[MAXFNAME + 1];

    // --- identify keyword
    k = findmatch(tok[0], TempKeyWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
    switch (k)
    {
      case 0: // Time series name
        // --- check that time series name exists
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(TSERIES, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);

        // --- record the time series as being the data source for temperature
        Temp.dataSource = TSERIES_TEMP;
        Temp.tSeries = i;
        Tseries[i].refersTo = TSERIES_TEMP;
        break;

      case 1: // Climate file
        // --- record file as being source of temperature data
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
        Temp.dataSource = FILE_TEMP;

        // --- save name and usage mode of external climate file
        Fclimate.mode = USE_FILE;
        sstrncpy(fname, tok[1], MAXFNAME);
        sstrncpy(Fclimate.name, addAbsolutePath(fname), MAXFNAME);

        // --- save starting date to read from file if one is provided
        Temp.fileStartDate = NO_DATE;
        if ( ntoks > 2 )
        {
            if ( *tok[2] != '*')
            {
                if ( !datetime_strToDate(tok[2], &aDate) )
                    return error_setInpError(ERR_DATETIME, tok[2]);
                Temp.fileStartDate = aDate;
            }
        }

        // --- file temperature units
        climate_->FileTempUnits = DEG_F;
        if (UnitSystem == SI)
            climate_->FileTempUnits = DEG_C;
        if (ntoks > 3)
        {
            i = findmatch(tok[3], TempUnitsWords);
            if (i < 0)
                return error_setInpError(ERR_KEYWORD, tok[3]);
            climate_->FileTempUnits = i;
        }
        break;

      case 2: // Wind speeds
        // --- check if wind speeds will be supplied from climate file
        if ( strcomp(tok[1], w_FILE) )
        {
            Wind.type = FILE_WIND;
        }

        // --- otherwise read 12 monthly avg. wind speed values
        else
        {
            if ( ntoks < 14 ) return error_setInpError(ERR_ITEMS, "");
            Wind.type = MONTHLY_WIND;
            for (i=0; i<12; i++)
            {
                if ( !getDouble(tok[i+2], &y) )
                    return error_setInpError(ERR_NUMBER, tok[i+2]);
                Wind.aws[i] = y;
            }
        }
        break;

      case 3: // Snowmelt params
        if ( ntoks < 7 ) return error_setInpError(ERR_ITEMS, "");
        for (i=1; i<7; i++)
        {
            if ( !getDouble(tok[i], &x[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        // --- convert deg. C to deg. F for snowfall temperature
        if ( UnitSystem == SI ) x[0] = 9./5.*x[0] + 32.0;
        Snow.snotmp = x[0];
        Snow.tipm   = x[1];
        Snow.rnm    = x[2];
        Temp.elev   = x[3] / UCF(LENGTH);
        Temp.anglat = x[4];
        Temp.dtlong = x[5] / 60.0;
        break;

      case 4:  // Areal Depletion Curve data
        // --- check if data is for impervious or pervious areas
        if ( ntoks < 12 ) return error_setInpError(ERR_ITEMS, "");
        if      ( match(tok[1], w_IMPERV) ) i = 0;
        else if ( match(tok[1], w_PERV)   ) i = 1;
        else return error_setInpError(ERR_KEYWORD, tok[1]);

        // --- read 10 fractional values
        for (j=0; j<10; j++)
        {
            if ( !getDouble(tok[j+2], &y) || y < 0.0 || y > 1.0 )
                return error_setInpError(ERR_NUMBER, tok[j+2]);
            Snow.adc[i][j] = y;
        }
        break;
    }
    return 0;
}

//=============================================================================

int Project::climate_readEvapParams(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads evaporation parameters from input line of data.
//
//  Data formats are:
//    CONSTANT  value
//    MONTHLY   v1 ... v12
//    TIMESERIES name
//    TEMPERATURE
//    FILE      (v1 ... v12)
//    RECOVERY   name
//    DRY_ONLY   YES/NO
//
{
    int i, k;
    double x;

    // --- find keyword indicating what form the evaporation data is in
    k = findmatch(tok[0], EvapTypeWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);

    // --- check for RECOVERY pattern data
    if ( k == RECOVERY )
    {
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(TIMEPATTERN, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);
        Evap.recoveryPattern = i;
        return 0;
    }

    // --- check for no evaporation in wet periods
    if ( k == DRYONLY )
    {
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
        if      ( strcomp(tok[1], w_NO ) )  Evap.dryOnly = FALSE;
        else if ( strcomp(tok[1], w_YES ) ) Evap.dryOnly = TRUE;
        else return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;
    }

    // --- process data depending on its form
    Evap.type = k;
    if ( k != TEMPERATURE_EVAP && ntoks < 2 )
        return error_setInpError(ERR_ITEMS, "");
    switch ( k )
    {
      case CONSTANT_EVAP:
        // --- for constant evap., fill monthly avg. values with same number
        if ( !getDouble(tok[1], &x) )
            return error_setInpError(ERR_NUMBER, tok[1]);
        for (i=0; i<12; i++) Evap.monthlyEvap[i] = x;
        break;

      case MONTHLY_EVAP:
        // --- for monthly evap., read a value for each month of year
        if ( ntoks < 13 ) return error_setInpError(ERR_ITEMS, "");
        for ( i=0; i<12; i++)
            if ( !getDouble(tok[i+1], &Evap.monthlyEvap[i]) )
                return error_setInpError(ERR_NUMBER, tok[i+1]);
        break;

      case TIMESERIES_EVAP:
        // --- for time series evap., read name of time series
        i = project_findObject(TSERIES, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);
        Evap.tSeries = i;
        Tseries[i].refersTo = TIMESERIES_EVAP;
        break;

      case FILE_EVAP:
        // --- for evap. from climate file, read monthly pan coeffs.
        //     if they are provided (default values are 1.0)
        if ( ntoks > 1 )
        {
            if ( ntoks < 13 ) return error_setInpError(ERR_ITEMS, "");
            for (i=0; i<12; i++)
            {
                if ( !getDouble(tok[i+1], &Evap.panCoeff[i]) )
                    return error_setInpError(ERR_NUMBER, tok[i+1]);
            }
        }
        break;
    }
    return 0;
}

//=============================================================================

int Project::climate_readAdjustments(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads adjustments to monthly evaporation or rainfall
//           from input line of data.
//
//  Data formats are:
//    TEMPERATURE   v1 ... v12
//    EVAPORATION   v1 ... v12
//    RAINFALL      v1 ... v12
//    CONDUCTIVITY  v1 ... v12
//    N-PERV        subcatchID  patternID 
//    DSTORE        subcatchID  patternID
//    INFIL         subcatchID  patternID
{
    int i, j;

    if (ntoks == 1) return 0;

    if ( match(tok[0], "TEMP") )
    {
        if ( ntoks < 13 )  return error_setInpError(ERR_ITEMS, "");
        for (i = 1; i < 13; i++)
        {
            if ( !getDouble(tok[i], &Adjust.temp[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        return 0;
    }

    if ( match(tok[0], "EVAP") )
    {
        if ( ntoks < 13 )  return error_setInpError(ERR_ITEMS, "");
        for (i = 1; i < 13; i++)
        {
            if ( !getDouble(tok[i], &Adjust.evap[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        return 0;
    }

    if ( match(tok[0], "RAIN") )
    {
        if ( ntoks < 13 )  return error_setInpError(ERR_ITEMS, "");
        for (i = 1; i < 13; i++)
        {
            if ( !getDouble(tok[i], &Adjust.rain[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        return 0;
    }

    if ( match(tok[0], "CONDUCT") )
    {
        if ( ntoks < 13 )  return error_setInpError(ERR_ITEMS, "");
        for (i = 1; i < 13; i++)
        {
            if ( !getDouble(tok[i], &Adjust.hydcon[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
            if ( Adjust.hydcon[i-1] <= 0.0 ) Adjust.hydcon[i-1] = 1.0;
        }
        return 0;
    }

    if ( match(tok[0], "N-PERV") )
    {
        if ( ntoks < 3 ) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(SUBCATCH, tok[1]);
        if (i < 0) return error_setInpError(ERR_NAME, tok[1]);
        j = project_findObject(TIMEPATTERN, tok[2]);
        if (j < 0) return error_setInpError(ERR_NAME, tok[2]);
        Subcatch[i].nPervPattern = j;
        return 0;
    }

    if ( match(tok[0], "DSTORE") )
    {
        if (ntoks < 3) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(SUBCATCH, tok[1]);
        if (i < 0) return error_setInpError(ERR_NAME, tok[1]);
        j = project_findObject(TIMEPATTERN, tok[2]);
        if (j < 0) return error_setInpError(ERR_NAME, tok[2]);
        Subcatch[i].dStorePattern = j;
        return 0;
    }

    if (match(tok[0], "INFIL"))
    {
        if (ntoks < 3) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(SUBCATCH, tok[1]);
        if (i < 0) return error_setInpError(ERR_NAME, tok[1]);
        j = project_findObject(TIMEPATTERN, tok[2]);
        if (j < 0) return error_setInpError(ERR_NAME, tok[2]);
        Subcatch[i].infilPattern = j;
        return 0;
    }
    return error_setInpError(ERR_KEYWORD, tok[0]);
}

//=============================================================================

void Project::climate_validate()
//
//  Input:   none
//  Output:  none
//  Purpose: validates climatological variables
//
{
    int       i;
    double    a, z, pa;

    // --- check if climate data comes from external data file 
    if ( Wind.type == FILE_WIND || Evap.type == FILE_EVAP ||
         Evap.type == TEMPERATURE_EVAP )
    {
        if ( Fclimate.mode == NO_FILE )
        {
            report_writeErrorMsg(ERR_NO_CLIMATE_FILE, "");
        }
    }

    // --- open the climate data file
    if ( Fclimate.mode == USE_FILE ) climate_openFile();

    // --- snow melt parameters tipm & rnm must be fractions
    if ( Snow.tipm < 0.0 ||
         Snow.tipm > 1.0 ||
         Snow.rnm  < 0.0 ||
         Snow.rnm  > 1.0 ) report_writeErrorMsg(ERR_SNOWMELT_PARAMS, "");

    // --- latitude should be between -90 & 90 degrees
    a = Temp.anglat;
    if ( a <= -89.99 ||
         a >= 89.99  ) report_writeErrorMsg(ERR_SNOWMELT_PARAMS, "");
    else Temp.tanAnglat = tan(a * PI / 180.0);

    // --- compute psychrometric constant
    z = Temp.elev / 1000.0;
    if ( z <= 0.0 ) pa = 29.9;
    else  pa = 29.9 - 1.02*z + 0.0032*pow(z, 2.4); // atmos. pressure
    Temp.gamma = 0.000359 * pa;

    // --- convert units of monthly temperature & evap adjustments
    for (i = 0; i < 12; i++)
    {
        if (UnitSystem == SI) Adjust.temp[i] *= 9.0/5.0;
        Adjust.evap[i] /= UCF(EVAPRATE);
    }
}

//=============================================================================

void Project::climate_openFile()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a climate file and reads in first set of values.
//
{
    int i, m, y;

    // --- open the file
    if ( (Fclimate.file = fopen(Fclimate.name, "rt")) == NULL )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_OPEN, Fclimate.name);
        return;
    }

    // --- initialize values of file's climate variables
    //     (Temp.ta was previously initialized in project.c)
    climate_->FileValue[TMIN] = Temp.ta;
    climate_->FileValue[TMAX] = Temp.ta;
    climate_->FileValue[EVAP] = 0.0;
    climate_->FileValue[WIND] = 0.0;

    // --- find climate file's format
    climate_->FileFormat = getFileFormat();
    if ( climate_->FileFormat == UNKNOWN_FORMAT )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_READ, Fclimate.name);
        return;
    }

    // --- position file to begin reading climate file at either user-specified
    //     month/year or at start of simulation period.
    rewind(Fclimate.file);
    sstrncpy(climate_->FileLine, "", 0);
    if ( Temp.fileStartDate == NO_DATE )
        datetime_decodeDate(StartDate, &climate_->FileYear, &climate_->FileMonth, &climate_->FileDay);
    else
        datetime_decodeDate(Temp.fileStartDate, &climate_->FileYear, &climate_->FileMonth, &climate_->FileDay);
    while ( !feof(Fclimate.file) )
    {
        sstrncpy(climate_->FileLine, "", 0);
        readFileLine(&y, &m);
        if ( y == climate_->FileYear && m == climate_->FileMonth ) break;
    }
    if ( feof(Fclimate.file) )
    {
        report_writeErrorMsg(ERR_CLIMATE_END_OF_FILE, Fclimate.name);
        return;
    }

    // --- initialize file dates and current climate variable values
    if ( !ErrorCode )
    {
        climate_->FileElapsedDays = 0;
        climate_->FileLastDay = datetime_daysPerMonth(climate_->FileYear, climate_->FileMonth);
        readFileValues();
        for (i=TMIN; i<=WIND; i++)
        {
            if ( climate_->FileData[i][climate_->FileDay] == MISSING ) continue;
            climate_->FileValue[i] = climate_->FileData[i][climate_->FileDay];
        }
    }
}

//=============================================================================

void Project::climate_initState()
//
//  Input:   none
//  Output:  none
//  Purpose: initializes climate state variables.
//
{
    climate_->LastDay = NO_DATE;
    Temp.tmax = MISSING;
    Snow.removed = 0.0;
    climate_->NextEvapDate = StartDate;
    climate_->NextEvapRate = 0.0;

    // --- initialize variables for time series evaporation
    if ( Evap.type == TIMESERIES_EVAP && Evap.tSeries >= 0  )
    {
        // --- initialize NextEvapDate & NextEvapRate to first entry of
        //     time series whose date <= the simulation start date
        table_getFirstEntry(&Tseries[Evap.tSeries],
                            &climate_->NextEvapDate, &climate_->NextEvapRate);
        if ( climate_->NextEvapDate < StartDate )
        {  
            setNextEvapDate(StartDate);
        }
        Evap.rate = climate_->NextEvapRate / UCF(EVAPRATE);

        // --- find the next time evaporation rates change after this
        setNextEvapDate(climate_->NextEvapDate); 
    }

    // --- initialize variables for temperature evaporation
    if ( Evap.type == TEMPERATURE_EVAP )
    {
        climate_->Tma.maxCount = sizeof(climate_->Tma.ta) / sizeof(double);
        climate_->Tma.count = 0;
        climate_->Tma.front = 0;
        climate_->Tma.tAve = 0.0;
        climate_->Tma.tRng = 0.0;
    }
}

//=============================================================================

void Project::climate_setState(DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets climate variables for current date.
//
{
    if ( Fclimate.mode == USE_FILE ) updateFileValues(theDate);
    if ( Temp.dataSource != NO_TEMP ) setTemp(theDate);
    setEvap(theDate);
    setWind(theDate);
    Adjust.rainFactor = Adjust.rain[datetime_monthOfYear(theDate)-1];
    Adjust.hydconFactor = Adjust.hydcon[datetime_monthOfYear(theDate)-1];
    setNextEvapDate(theDate);
}

//=============================================================================

DateTime Project::climate_getNextEvapDate()
//
//  Input:   none
//  Output:  returns the current value of NextEvapDate
//  Purpose: gets the next date when evaporation rate changes.
//
{
    return climate_->NextEvapDate;
}

//=============================================================================

void Project::setNextEvapDate(DateTime theDate)
//
//  Input:   theDate = current simulation date
//  Output:  sets a new value for NextEvapDate
//  Purpose: finds date for next change in evaporation after the current date.
//
{
    int    yr, mon, day, k;
    double d, e;

    // --- do nothing if current date hasn't reached the current next date
    if ( climate_->NextEvapDate > theDate ) return;

    switch ( Evap.type )
    {
      // --- for constant evaporation, use a next date far in the future
      case CONSTANT_EVAP:
         climate_->NextEvapDate = theDate + 365.;
         break;

      // --- for monthly evaporation, use the start of the next month
      case MONTHLY_EVAP:
        datetime_decodeDate(theDate, &yr, &mon, &day);
        if ( mon == 12 )
        {
            mon = 1;
            yr++;
        }
        else mon++;
        climate_->NextEvapDate = datetime_encodeDate(yr, mon, 1);
        break;

      // --- for time series evaporation, find the next entry in the
      //     series on or after the current date
      case TIMESERIES_EVAP:
        k = Evap.tSeries;
        if ( k >= 0 )
        {
            climate_->NextEvapDate = theDate + 365.;
            while ( table_getNextEntry(&Tseries[k], &d, &e) &&
                    d <= EndDateTime )
            {
                if ( d >= theDate )
                {
                    climate_->NextEvapDate = d;
                    climate_->NextEvapRate = e;
                    break;
                }
            }
        }
        break;

      // --- for climate file daily evaporation, use the next day
      case FILE_EVAP:
        climate_->NextEvapDate = floor(theDate) + 1.0;
        break;

      default: climate_->NextEvapDate = theDate + 365.;
    }
}

//=============================================================================

void Project::updateFileValues(DateTime theDate)
//
//  Input:   theDate = current simulation date
//  Output:  none
//  Purpose: updates daily climate variables for new day or reads in
//           another month worth of values if a new month begins.
//
//  NOTE:    counters FileElapsedDays, FileDay, FileMonth, FileYear and
//           FileLastDay were initialized in climate_openFile().
//
{
    int i;
    int deltaDays;

    // --- see if a new day has begun
    deltaDays = (int)(floor(theDate) - floor(StartDateTime));
    if ( deltaDays > climate_->FileElapsedDays )
    {
        // --- advance day counters
        climate_->FileElapsedDays++;
        climate_->FileDay++;

        // --- see if new month of data needs to be read from file
        if ( climate_->FileDay > climate_->FileLastDay )
        {
            climate_->FileMonth++;
            if ( climate_->FileMonth > 12 )
            {
                climate_->FileMonth = 1;
                climate_->FileYear++;
            }
            readFileValues();
            climate_->FileDay = 1;
            climate_->FileLastDay = datetime_daysPerMonth(climate_->FileYear, climate_->FileMonth);
        }

        // --- set climate variables for new day
        for (i=TMIN; i<=WIND; i++)
        {
            // --- no change in current value if its missing
            if ( climate_->FileData[i][climate_->FileDay] == MISSING ) continue;
            climate_->FileValue[i] = climate_->FileData[i][climate_->FileDay];
        }
    }
}

//=============================================================================

void Project::setTemp(DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: updates temperatures for new simulation date.
//
{
    int      j;                        // snow data object index
    int      k;                        // time series index
    int      mon;                      // month of year 
    int      day;                      // day of year
    DateTime theDay;                   // calendar day
    double   hour;                     // hour of day
    double   tmp;                      // temporary temperature

    // --- see if a new day has started
    mon = datetime_monthOfYear(theDate);
    theDay = floor(theDate);
    if ( theDay > climate_->LastDay )
    {
        // --- update min. & max. temps & their time of day
        day = datetime_dayOfYear(theDate);
        if ( Temp.dataSource == FILE_TEMP )
        {
            climate_->Tmin = climate_->FileValue[TMIN] + Adjust.temp[mon-1];
            climate_->Tmax = climate_->FileValue[TMAX] + Adjust.temp[mon-1];
            if ( climate_->Tmin > climate_->Tmax )
            {
                tmp = climate_->Tmin;
                climate_->Tmin = climate_->Tmax;
                climate_->Tmax = tmp;
            }
            updateTempTimes(day);
            if ( Evap.type == TEMPERATURE_EVAP )
            {
                updateTempMoveAve(climate_->Tmin, climate_->Tmax); 
                climate_->FileValue[EVAP] = getTempEvap(day, climate_->Tma.tAve, climate_->Tma.tRng);
            }
        }

        // --- compute snow melt coefficients based on day of year
        Snow.season = sin(0.0172615*(day-81.0));
        for (j=0; j<Nobjects[SNOWMELT]; j++)
        {
            snow_setMeltCoeffs(j, Snow.season);
        }

        // --- update date of last day analyzed
        climate_->LastDay = theDate;
    }

    // --- for min/max daily temps. from climate file,
    //     compute hourly temp. by sinusoidal interp.
    if ( Temp.dataSource == FILE_TEMP )
    {
        hour = (theDate - theDay) * 24.0;
        if ( hour < climate_->Hrsr )
            Temp.ta = climate_->Tmin + climate_->Trng1/2.0 * sin(PI/climate_->Dydif * (climate_->Hrsr - hour));
        else if ( hour >= climate_->Hrsr && hour <= climate_->Hrss )
            Temp.ta = climate_->Tave + climate_->Trng * sin(PI/climate_->Dhrdy * (climate_->Hrday - hour));
        else
            Temp.ta = climate_->Tmax - climate_->Trng * sin(PI/climate_->Dydif * (hour - climate_->Hrss));
    }

    // --- for user-supplied temperature time series,
    //     get temperature value from time series
    if ( Temp.dataSource == TSERIES_TEMP )
    {
        k = Temp.tSeries;
        if ( k >= 0)
        {
            Temp.ta = table_tseriesLookup(&Tseries[k], theDate, TRUE);

            // --- convert from deg. C to deg. F if need be
            if ( UnitSystem == SI )
            {
                Temp.ta = (9./5.) * Temp.ta + 32.0;
            }

            // --- apply climate change adjustment factor 
            Temp.ta += Adjust.temp[mon-1];
        }
    }

    // --- compute saturation vapor pressure
    Temp.ea = 8.1175e6 * exp(-7701.544 / (Temp.ta + 405.0265) );
}

//=============================================================================

void Project::setEvap(DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets evaporation rate (ft/sec) for a specified date.
//
{
    int k;
    int mon = datetime_monthOfYear(theDate);

    switch ( Evap.type )
    {
      case CONSTANT_EVAP:
        Evap.rate = Evap.monthlyEvap[0] / UCF(EVAPRATE);
        break;

      case MONTHLY_EVAP:
        Evap.rate = Evap.monthlyEvap[mon-1] / UCF(EVAPRATE);
        break;

      case TIMESERIES_EVAP:
        if ( theDate >= climate_->NextEvapDate )
            Evap.rate = climate_->NextEvapRate / UCF(EVAPRATE);
        break;

      case FILE_EVAP:
        Evap.rate = climate_->FileValue[EVAP] / UCF(EVAPRATE);
        Evap.rate *= Evap.panCoeff[mon-1];
        break;

      case TEMPERATURE_EVAP:
        Evap.rate = climate_->FileValue[EVAP] / UCF(EVAPRATE);
        break;

      default: Evap.rate = 0.0;
    }

    // --- apply climate change adjustment
    Evap.rate += Adjust.evap[mon-1];

    // --- set soil recovery factor
    Evap.recoveryFactor = 1.0;
    k = Evap.recoveryPattern;
    if ( k >= 0 && Pattern[k].type == MONTHLY_PATTERN )
    {
        Evap.recoveryFactor = Pattern[k].factor[mon-1];
    }
}

//=============================================================================

void Project::setWind(DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets wind speed (mph) for a specified date.
//
{
    int yr, mon, day;

    switch ( Wind.type )
    {
      case MONTHLY_WIND:
        datetime_decodeDate(theDate, &yr, &mon, &day);
        Wind.ws = Wind.aws[mon-1] / UCF(WINDSPEED);
        break;

      case FILE_WIND:
        Wind.ws = climate_->FileValue[WIND];
        break;

      default: Wind.ws = 0.0;
    }
}

//=============================================================================

void Project::updateTempTimes(int day)
//
//  Input:   day = day of year
//  Output:  none
//  Purpose: computes time of day when min/max temperatures occur.
//           (min. temp occurs at sunrise, max. temp. at 3 hrs. < sunset)
//
{
    double decl;                       // earth's declination
    double hrang;                      // hour angle of sunrise/sunset
    double arg;

    decl  = 0.40928*cos(0.017202*(172.0-day));
    arg = -tan(decl)*Temp.tanAnglat;
    if      ( arg <= -1.0 ) arg = PI;
    else if ( arg >= 1.0 )  arg = 0.0;
    else                    arg = acos(arg);
    hrang = 3.8197 * arg;
    climate_->Hrsr  = 12.0 - hrang + Temp.dtlong;
    climate_->Hrss  = 12.0 + hrang + Temp.dtlong - 3.0;
    climate_->Dhrdy = climate_->Hrsr - climate_->Hrss;
    climate_->Dydif = 24.0 + climate_->Hrsr - climate_->Hrss;
    climate_->Hrday = (climate_->Hrsr + climate_->Hrss) / 2.0;
    climate_->Tave  = (climate_->Tmin + climate_->Tmax) / 2.0;
    climate_->Trng  = (climate_->Tmax - climate_->Tmin) / 2.0;
    if ( Temp.tmax == MISSING ) climate_->Trng1 = climate_->Tmax - climate_->Tmin;
    else                        climate_->Trng1 = Temp.tmax - climate_->Tmin;
    Temp.tmax = climate_->Tmax;
}

//=============================================================================

double Project::getTempEvap(int day, double tave, double trng)
//
//  Input:   day = day of year
//           tave = 7-day average temperature (deg F)
//           trng = 7-day average daily temperature range (deg F)
//  Output:  returns evaporation rate in user's units (US:in/day, SI:mm/day)
//  Purpose: uses Hargreaves method to compute daily evaporation rate
//           from daily average temperatures and Julian day.
//
{
    double a = 2.0*PI/365.0;
    double ta = (tave - 32.0)*5.0/9.0;           //average temperature (deg C)
    double tr = trng*5.0/9.0;                    //temperature range (deg C)
    double lamda = 2.50 - 0.002361 * ta;         //latent heat of vaporization
    double dr = 1.0 + 0.033*cos(a*day);          //relative earth-sun distance
    double phi = Temp.anglat*2.0*PI/360.0;       //latitude angle (rad)
    double del = 0.4093*sin(a*(284.+(double)day)); //solar declination angle (rad)
    double omega = acos(-tan(phi)*tan(del));     //sunset hour angle (rad)
    double ra = 37.6*dr*                         //extraterrestrial radiation
                (omega*sin(phi)*sin(del) +
                 cos(phi)*cos(del)*sin(omega));
    double e = 0.0023*ra/lamda*sqrt(tr)*(ta+17.8);    //evap. rate (mm/day)
    if ( e < 0.0 ) e = 0.0;
    if ( UnitSystem == US ) e /= MMperINCH;           //evap rate (in/day)
    return e;
}

//=============================================================================

int  Project::getFileFormat()
//
//  Input:   none
//  Output:  returns code number of climate file's format
//  Purpose: determines what format the climate file is in.
//
{
    char recdType[4] = "";
    char elemType[4] = "";
    char filler[5] = "";
    char staID[80];
    char s[80];
    char line[MAXLINE];

    int  y, m, d, n;

    // --- read first line of file
    if ( fgets(line, MAXLINE, Fclimate.file) == NULL ) return UNKNOWN_FORMAT;

    // --- check for TD3200 format
    sstrncpy(recdType, line, 3);
    sstrncpy(filler, &line[23], 4);
    if ( strcmp(recdType, "DLY") == 0 &&
         strcmp(filler, "9999")  == 0 ) return TD3200;

    // --- check for DLY0204 format
    if ( strlen(line) >= 233 )
    {
        sstrncpy(elemType, &line[13], 3);
        n = atoi(elemType);
        if ( n == 1 || n == 2 || n == 151 ) return DLY0204;
    }

    // --- check for USER_PREPARED format
    n = sscanf(line, "%s %d %d %d %s", staID, &y, &m, &d, s);
    if ( n == 5 ) return USER_PREPARED;

    // --- check for GHCND format
    if ( isGhcndFormat(line) ) return GHCND;

    return UNKNOWN_FORMAT;
}

//=============================================================================

void Project::readFileLine(int *y, int *m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from next line of climate file.
//
{
    // --- read next line from climate data file
    while ( strlen(climate_->FileLine) == 0 )
    {
        if ( fgets(climate_->FileLine, MAXLINE, Fclimate.file) == NULL ) return;
     	if ( climate_->FileLine[0] == '\n' ) climate_->FileLine[0] = '\0';
    }

    // --- parse year & month from line
    switch (climate_->FileFormat)
    {
    case  USER_PREPARED: readUserFileLine(y, m);   break;
    case  TD3200:        readTD3200FileLine(y,m);  break;
    case  DLY0204:       readDLY0204FileLine(y,m); break;
    case  GHCND:         readGhcndFileLine(y,m);   break; 
    }
}

//=============================================================================

void Project::readUserFileLine(int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of User-Prepared climate file.
//
{
    int n;
    char staID[80];
    n = sscanf(climate_->FileLine, "%s %d %d", staID, y, m);
    if ( n < 3 )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_READ, Fclimate.name);
    }
}

//=============================================================================

void Project::readTD3200FileLine(int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of TD-3200 climate file.
//
{
    char recdType[4] = "";
    char year[5] = "";
    char month[3] = "";

    // --- check for minimum number of characters
    if ( strlen(climate_->FileLine) < 30 )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_READ, Fclimate.name);
        return;
    }

    // --- check for proper type of record
    sstrncpy(recdType, climate_->FileLine, 3);
    if ( strcmp(recdType, "DLY") != 0 )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_READ, Fclimate.name);
        return;
    }

    // --- get record's date
    sstrncpy(year,  &climate_->FileLine[17], 4);
    sstrncpy(month, &climate_->FileLine[21], 2);
    *y = atoi(year);
    *m = atoi(month);
}

//=============================================================================

void Project::readDLY0204FileLine(int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of DLY02 or DLY04 climate file.
//
{
    char year[5] = "";
    char month[3] = "";

    // --- check for minimum number of characters
    if ( strlen(climate_->FileLine) < 16 )
    {
        report_writeErrorMsg(ERR_CLIMATE_FILE_READ, Fclimate.name);
        return;
    }

    // --- get record's date
    sstrncpy(year,  &climate_->FileLine[7], 4);
    sstrncpy(month, &climate_->FileLine[11], 2);
    *y = atoi(year);
    *m = atoi(month);
}

//=============================================================================

void Project::readFileValues()
//
//  Input:   none
//  Output:  none
//  Purpose: reads next month's worth of data from climate file.
//
{
    int  i, j;
    int  y, m;

    // --- initialize FileData array to missing values
    for ( i=0; i<MAXCLIMATEVARS; i++)
    {
        for (j=0; j<MAXDAYSPERMONTH; j++) climate_->FileData[i][j] = MISSING;
    }

    while ( !ErrorCode )
    {
        // --- return when date on line is after current file date
        if ( feof(Fclimate.file) ) return;
        readFileLine(&y, &m);
        if ( y > climate_->FileYear || m > climate_->FileMonth ) return;

        // --- parse climate values from file line
        switch (climate_->FileFormat)
        {
        case  USER_PREPARED: parseUserFileLine();   break;
        case  TD3200:        parseTD3200FileLine();  break;
        case  DLY0204:       parseDLY0204FileLine(); break;
        case  GHCND:         parseGhcndFileLine();   break; 
        }
        sstrncpy(climate_->FileLine, "", 0);
    }
}

//=============================================================================

void Project::parseUserFileLine()
//
//  Input:   none
//  Output:  none
//  Purpose: parses climate variable values from a line of a user-prepared
//           climate file.
//
{
    int   n;
    int   y, m, d;
    char  staID[80];
    char  s0[80] = "";
    char  s1[80] = "";
    char  s2[80] = "";
    char  s3[80] = "";
    double x;

    // --- read day, Tmax, Tmin, Evap, & Wind from file line
    n = sscanf(climate_->FileLine, "%s %d %d %d %s %s %s %s",
        staID, &y, &m, &d, s0, s1, s2, s3);
    if ( n < 4 ) return;
    if ( d < 1 || d > 31 ) return;

    // --- process TMAX
    if ( strlen(s0) > 0 && *s0 != '*' )
    {
        x = atof(s0);
        if ( UnitSystem == SI ) x = 9./5.*x + 32.0;
        climate_->FileData[TMAX][d] =  x;
    }

    // --- process TMIN
    if ( strlen(s1) > 0 && *s1 != '*' )
    {
        x = atof(s1);
        if ( UnitSystem == SI ) x = 9./5.*x + 32.0;
        climate_->FileData[TMIN][d] =  x;
    }

    // --- process EVAP
    if ( strlen(s2) > 0 && *s2 != '*' ) climate_->FileData[EVAP][d] = atof(s2);

    // --- process WIND
    if ( strlen(s3) > 0 && *s3 != '*' ) climate_->FileData[WIND][d] = atof(s3);
}

//=============================================================================

void Project::parseTD3200FileLine()
//
//  Input:   none
//  Output:  none
//  Purpose: parses climate variable values from a line of a TD3200 file.
//
{
    int  i;
    char param[5] = "";

    // --- parse parameter name
    sstrncpy(param, &climate_->FileLine[11], 4);

    // --- see if parameter is temperature, evaporation or wind speed
    for (i=0; i<MAXCLIMATEVARS; i++)
    {
        if (strcmp(param, ClimateVarWords[i]) == 0 ) setTD3200FileValues(i);
    }
}

//=============================================================================

void Project::setTD3200FileValues(int i)
//
//  Input:   i = climate variable code
//  Output:  none
//  Purpose: reads month worth of values for climate variable from TD-3200 file.
//
{
    char valCount[4] = "";
    char day[3] = "";
    char sign[2] = "";
    char value[6] = "";
    char flag2[2] = "";
    double x;
    int  nValues;
    int  j, k, d;

    // --- parse number of days with data from cols. 27-29 of file line
    sstrncpy(valCount, &climate_->FileLine[27], 3);
    nValues = atoi(valCount);

    // --- check for enough characters on line
    if ( (int)strlen(climate_->FileLine) >= 12*nValues + 30 )
    {
        // --- for each day's value
        for (j=0; j<nValues; j++)
        {
            // --- parse day, value & flag from file line
            k = 30 + j*12;
            sstrncpy(day,   &climate_->FileLine[k], 2);
            sstrncpy(sign,  &climate_->FileLine[k+4], 1);
            sstrncpy(value, &climate_->FileLine[k+5], 5);
            sstrncpy(flag2, &climate_->FileLine[k+11], 1);

            // --- if value is valid then store it in FileData array
            d = atoi(day);
            if ( strcmp(value, "99999") != 0
                 && ( flag2[0] == '0' || flag2[0] == '1')
                 &&   d > 0
                 &&   d <= 31 )
            {
                // --- convert from string value to numerical value
                x = atof(value);
                if ( sign[0] == '-' ) x = -x;

                // --- convert evaporation from hundreths of inches
                if ( i == EVAP )
                {
                    x /= 100.0;

                    // --- convert to mm if using SI units
                    if ( UnitSystem == SI ) x *= MMperINCH;
                }

                // --- convert wind speed from miles/day to miles/hour
                if ( i == WIND ) x /= 24.0;

                // --- store value
                climate_->FileData[i][d] = x;
            }
        }
    }
}

//=============================================================================

void Project::parseDLY0204FileLine()
//
//  Input:   none
//  Output:  none
//  Purpose: parses a month's worth of climate variable values from a line of
//           a DLY02 or DLY04 climate file.
//
{
    int  j, k, p;
    char param[4] = "";
    char sign[2]  = "";
    char value[6] = "";
    char code[2]  = "";
    double x;

    // --- parse parameter name
    sstrncpy(param, &climate_->FileLine[13], 3);

    // --- see if parameter is min or max temperature
    p = atoi(param);
    if ( p == 1 ) p = TMAX;
    else if ( p == 2 ) p = TMIN;
    else if ( p == 151 ) p = EVAP;
    else return;

    // --- check for 233 characters on line
    if ( strlen(climate_->FileLine) < 233 ) return;

    // --- for each of 31 days
    k = 16;
    for (j=1; j<=31; j++)
    {
        // --- parse value & flag from file line
        sstrncpy(sign,  &climate_->FileLine[k], 1);
        sstrncpy(value, &climate_->FileLine[k+1], 5);
        sstrncpy(code,  &climate_->FileLine[k+6], 1);
        k += 7;

        // --- if value is valid then store it in FileData array

        if ( strcmp(value, "99999") != 0 && strcmp(value, "     ") != 0 )
        {
            switch (p)
            {
            case TMAX:
            case TMIN:
                // --- convert from integer tenths of a degree C to degrees F
                x = atof(value) / 10.0;
                if ( sign[0] == '-' ) x = -x;
                x = 9./5.*x + 32.0;
                break;
            case EVAP:
                // --- convert from 0.1 mm to inches or mm
                x = atof(value) / 10.0;
                if ( UnitSystem == US ) x /= MMperINCH;
                break;
			default: return;
            }
            climate_->FileData[p][j] = x;
        }
    }
}

//=============================================================================

int Project::isGhcndFormat(char* line)
//
//  Input:   line = first line of text from a climate file
//  Output:  returns TRUE if climate file is in NCDC GHCN Daily format.
//  Purpose: Checks if a climate file is in the NCDC GHCN Daily format
//           and determines the position of each climate variable field.
//
{
    int i;
    char* ptr;

    // --- find starting position of the DATE field
    ptr = strstr(line, "DATE");
    if ( ptr == NULL ) return FALSE;
    climate_->FileDateFieldPos = (int)(ptr - line);

    // --- initialize starting position of each data field
    for ( i = TMIN; i <= WIND; i++) climate_->FileFieldPos[i] = -1;

    // --- find starting position of each climate variable's data field
    ptr = strstr(line, "TMIN");
    if ( ptr ) climate_->FileFieldPos[TMIN] = (int)(ptr - line);
    ptr = strstr(line, "TMAX");
    if ( ptr ) climate_->FileFieldPos[TMAX] = (int)(ptr - line);
    ptr = strstr(line, "EVAP");
    if ( ptr ) climate_->FileFieldPos[EVAP] = (int)(ptr - line);

    // --- WIND can either be daily movement or average speed
    climate_->FileWindType = WDMV;
    ptr = strstr(line, "WDMV");
    if ( ptr == NULL )
    {
        climate_->FileWindType = AWND;
        ptr = strstr(line, "AWND");
    }
    if ( ptr ) climate_->FileFieldPos[WIND] = (int)(ptr - line);

    // --- check if at least one climate variable was found
    for (i = TMIN; i <= WIND; i++) if (climate_->FileFieldPos[i] >= 0 ) return TRUE;
    return FALSE;
}

//=============================================================================

void Project::readGhcndFileLine(int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of a NCDC GHCN Daily climate file.
//
{
    int n = sscanf(&climate_->FileLine[climate_->FileDateFieldPos], "%4d%2d", y, m);
    if ( n != 2 )
    {
        *y = -99999;
        *m = -99999;
    }
}

//=============================================================================

void Project::parseGhcndFileLine()
//
//  Input:   none
//  Output:  none
//  Purpose: parses a line of a NCDC GHCN Daily file for daily
//           values of max/min temperature, pan evaporation and
//           wind speed.
//
{
    int y, m, d, n, i;
    double v;

    // --- parse day of month from date field
    n = sscanf(&climate_->FileLine[climate_->FileDateFieldPos], "%4d%2d%2d", &y, &m, &d);
    if ( n < 3 ) return;
    if ( d < 1 || d > 31 ) return;

    // --- parse climate variables
    for (i = TMIN; i <= WIND; i++)
    {
        if ( climate_->FileFieldPos[i] >= 0 )
        {
            if ( sscanf(&climate_->FileLine[climate_->FileFieldPos[i]], "%8lf", &v) > 0 )
            {
                if ( fabs(v) < 9999. )
                    climate_->FileData[i][d] = convertGhcndValue(i, v);
            }
        }
    }
}

//=============================================================================

double Project::convertGhcndValue(int var, double v)
//
//  Input:   var = climate variable code
//           v = climate variable value
//  Output:  climate variable value in SWMM's internal units
//  Purpose: converts a climate variable value read from a NCDC GHCN Daily file
//           to SWMM's internal units.
//
{
    switch (var)
    {
        case TMIN:
        case TMAX:
            switch (climate_->FileTempUnits)
            {
                case DEG_C10:  // tenths deg. C ==> deg. F
                    return v / 10. * 9.0 / 5.0 + 32.0;

                case DEG_C:    // deg. C ==> deg. F
                    return v * 9.0 / 5.0 + 32.0;

                default:       // deg. F
                    return v;
            }
        case EVAP:
            switch (climate_->FileTempUnits)
            {
                case DEG_C10:  // tenths mm ==> inches or mm
                    v /= 10.;
                    if (UnitSystem == US) v /= MMperINCH;
                    return v;

                case DEG_C:    // mm ==> inches or mm
                    if (UnitSystem == US) v /= MMperINCH;
                    return v;

                default:       // inches ==> inches or mm
                    if (UnitSystem == SI) v *= MMperINCH;
                    return v;
            }
        case WIND:
            switch (climate_->FileTempUnits)
            {
                case DEG_C10:
                    // km/day ==> miles/hr
                    if (climate_->FileWindType == WDMV)
                        return v * 0.62137 / 24.;
                    // tenths m/s ==> miles/hr
                    else
                        return v / 10. / 1000. * 0.62137 * 3600.;
                case DEG_C:
                    // km/day ==> miles/hr
                    if (climate_->FileWindType == WDMV)
                        return v * 0.62137 / 24.;
                    // m/s ==> miles/hr
                    else
                        return v / 1000. * 0.62137 * 3600.;

                default:
                    // miles ==> miles/hr
                    if (climate_->FileWindType == WDMV)
                        return v / 24.;
                    // miles/hr
                    else
                        return v;
            }
        default:
            return v;
    }
}

//=============================================================================

void Project::updateTempMoveAve(double tmin, double tmax)
//
//  Input:   tmin = minimum daily temperature (deg F)
//           tmax = maximum daily temperature (deg F)
//  Output:  none
//  Purpose: updates moving averages of average daily temperature
//           and daily temperature range stored in structure Tma.
//
{
    double ta,               // new day's average temperature (deg F)
           tr;               // new day's temperature range (deg F)
    int    kount = climate_->Tma.count;
    double count = kount;

    // --- find ta and tr from new day's min and max temperature
    ta = (tmin + tmax) / 2.0;
    tr = fabs(tmax - tmin);

    // --- if the array used to store previous days' temperatures is full
    if ( kount == climate_->Tma.maxCount )
    {
        // --- update the moving averages with the new day's value
        climate_->Tma.tAve = (climate_->Tma.tAve * count + ta - climate_->Tma.ta[climate_->Tma.front]) / count;
        climate_->Tma.tRng = (climate_->Tma.tRng * count + tr - climate_->Tma.tr[climate_->Tma.front]) / count;

        // --- replace the values at the front of the moving average window
        climate_->Tma.ta[climate_->Tma.front] = ta;
        climate_->Tma.tr[climate_->Tma.front] = tr;

        // --- move the front one position forward
        climate_->Tma.front++;
        if ( climate_->Tma.front == count ) climate_->Tma.front = 0;
    }

    // --- array of previous day's values not full (at start of simulation)
    else
    {
        // --- find new moving averages by adding new values to previous ones
        climate_->Tma.tAve = (climate_->Tma.tAve * count + ta) / (count + 1);
        climate_->Tma.tRng = (climate_->Tma.tRng * count + tr) / (count + 1);

        // --- save new day's values
        climate_->Tma.ta[climate_->Tma.front] = ta;
        climate_->Tma.tr[climate_->Tma.front] = tr;

        // --- increment count and front of moving average window
        climate_->Tma.count++;
        climate_->Tma.front++;
        if ( climate_->Tma.count == climate_->Tma.maxCount ) climate_->Tma.front = 0;
    }
}
