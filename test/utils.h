// Copyright 2022 Ted Phelps
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stddef.h>
#include <time.h>
#include <string.h>
#include <assert.h>

enum day_of_week {
    DOW_SUN = 0,
    DOW_MON = 1,
    DOW_TUE = 2,
    DOW_WED = 3,
    DOW_THU = 4,
    DOW_FRI = 5,
    DOW_SAT = 6
};


void init_tm(struct tm *tm, int year, int month, int day, int hour, int min, int sec, int isdst);
void init_tm_full(struct tm *tm,
                  int year, int month, int day,
                  int hour, int min, int sec,
                  int isdst, int wday, int yday,
                  int utoff, const char *desig);


void report_tm_neq(int64_t ts, const struct tm *expected, const struct tm *actual);

static inline void assert_tm_eq(int64_t ts, const struct tm *expected, const struct tm *actual)
{
    if (expected->tm_sec != actual->tm_sec ||
        expected->tm_min != actual->tm_min ||
        expected->tm_hour != actual->tm_hour ||
        expected->tm_mday != actual->tm_mday ||
        expected->tm_mon != actual->tm_mon ||
        expected->tm_year != actual->tm_year ||
        expected->tm_wday != actual->tm_wday ||
        expected->tm_yday != actual->tm_yday ||
        expected->tm_isdst != actual->tm_isdst ||
        expected->tm_gmtoff != actual->tm_gmtoff ||
        strcmp(actual->tm_zone, expected->tm_zone) != 0) {
        report_tm_neq(ts, expected, actual);
    }
}


void assert_tm(int64_t ts,
               int year, int month, int day,
               int hour, int min, int sec,
               int isdst, int wday, int yday,
               int utoff, const char *desig,
               const struct tm *actual);

////////////////////////////////////////////////////////////////////////
// End of utils.h
