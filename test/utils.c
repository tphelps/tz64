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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>
#include "utils.h"

void init_tm(struct tm *tm, int year, int month, int day, int hour, int min, int sec, int isdst)
{
    memset(tm, 0, sizeof(*tm));
    tm->tm_year = year - 1900;
    tm->tm_mon = month - 1;
    tm->tm_mday = day;
    tm->tm_hour = hour;
    tm->tm_min = min;
    tm->tm_sec = sec;
    tm->tm_isdst = isdst;
}


void init_tm_full(struct tm *tm,
                  int year, int month, int day,
                  int hour, int min, int sec,
                  int isdst, int wday, int yday,
                  int utoff, const char *desig)
{
    init_tm(tm, year, month, day, hour, min, sec, isdst);
    tm->tm_wday = wday;
    tm->tm_yday = yday - 1;
    tm->tm_gmtoff = utoff;
    tm->tm_zone = desig;
}


static char *format_tm(char *buffer, size_t buflen, const struct tm *tm)
{
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%a %Y-%m-%d %H:%M:%S %M", tm);
    snprintf(buffer, buflen, "%s yday=%d, dst=%d, offset=%ld", timebuf, tm->tm_yday + 1, tm->tm_isdst, tm->tm_gmtoff);
    return buffer;
}


void report_tm_neq(int64_t ts, const struct tm *expected, const struct tm *actual)
{
    char buffer[128];
    fprintf(stderr, "error: broken-down times do not match at time %" PRId64 ":\n", ts);
    fprintf(stderr, "expected: %s\n", format_tm(buffer, sizeof(buffer), expected));
    fprintf(stderr, "  actual: %s\n", format_tm(buffer, sizeof(buffer), actual));
    abort();
}


void assert_tm(int64_t ts,
               int year, int month, int day,
               int hour, int min, int sec,
               int isdst, int wday, int yday,
               int utoff, const char *desig,
               const struct tm *actual)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    init_tm_full(&tm, year, month, day, hour, min, sec, isdst, wday, yday, utoff, desig);
    assert_tm_eq(ts, &tm, actual);
}

////////////////////////////////////////////////////////////////////////
// End of utils.c
