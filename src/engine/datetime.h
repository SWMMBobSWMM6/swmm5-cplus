//-----------------------------------------------------------------------------
//   datetime.h
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     11/01/21  (Build 5.2.0)
//   Author:   L. Rossman
//
//   The DateTime type is used to store date and time values. It is
//   equivalent to a double floating point type.
//
//   The integral part of a DateTime value is the number of days that have
//   passed since 12/31/1899. The fractional part of a DateTime value is the
//   fraction of a 24 hour day that has elapsed.
//
//   Update History
//   ==============
//   Build 5.1.011:
//   - New getTimeStamp function added.
//-----------------------------------------------------------------------------

#ifndef DATETIME_H
#define DATETIME_H


typedef double DateTime;

#define Y_M_D 0
#define M_D_Y 1
#define D_M_Y 2
#define NO_DATE -693594 // 1/1/0001
#define DATE_STR_SIZE 12
#define TIME_STR_SIZE 9
#define TIME_STAMP_SIZE 21

// Functions for encoding a date or time value to a DateTime value



// Functions for decoding a DateTime value to a date and time



// Function for finding day of week for a date (1 = Sunday)
// month of year, days per month, and hour of day






// Functions for converting a DateTime value to a string




// Functions for converting a string date or time to a DateTime value




// Function for setting date format


// Functions for adding and subtracting dates





#endif //DATETIME_H
