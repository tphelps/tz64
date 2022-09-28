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

#include <inttypes.h>
#include <time.h>

#ifndef CONSTANTS_H

static const int64_t secs_per_min = 60;
static const int64_t mins_per_hour = 60;
static const int64_t secs_per_hour = mins_per_hour * secs_per_min;
static const int64_t hours_per_day = 24;
static const int64_t secs_per_day = hours_per_day * secs_per_hour;

static const int64_t days_per_week = 7;
static const int64_t days_per_nyear = 365;
static const int64_t secs_per_nyear = days_per_nyear * secs_per_day;

static const int64_t days_per_4_nyears = 4 * days_per_nyear + 1;
static const int64_t days_per_ncentury = 100 * days_per_nyear + 100 / 4 - 1;
static const int64_t days_per_400_years = 400 * days_per_nyear + 400 / 4 - 4 + 1;
static const int64_t secs_per_400_years = days_per_400_years * secs_per_day;
static const int64_t avg_secs_per_year = secs_per_400_years / 400;

static const int month_starts[2][13] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};


// We use 2001-01-01 00:00:00 UTC as our reference time because it
// simplifies the maths.  Compute seconds from 1970-01-01 00:00:00.
// That span includes 8 leap years 1972, 1976, 1980, 1984, 1988, 1992,
// 1996 and 2000.
static const int64_t base_year = 1900;
static const int64_t ref_year = 1970;
static const int64_t alt_ref_year = 2001;
static const int64_t alt_ref_ts = (((alt_ref_year - ref_year) * days_per_nyear) + 8) * secs_per_day;

// Upper and lower bounds on the maximum timestamp that can be
// represented in a struct tm.
static const int64_t max_tm_ts = (ref_year - base_year + INT32_MAX + 1) * avg_secs_per_year;
static const int64_t min_tm_ts = (ref_year - base_year + INT32_MIN - 1) * avg_secs_per_year;

extern const int64_t *const tz64_year_starts;
extern const uint8_t *const tz64_year_types;

static inline int is_leap(int64_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}


static inline int64_t encode_ymdhm(const struct tm *tm)
{
    return (((uint64_t)tm->tm_year) << 32) |
        (((uint32_t)tm->tm_mon) << 24) |
        (((uint32_t)tm->tm_mday) << 16) |
        (((uint32_t)tm->tm_hour) << 8) |
        (uint32_t)tm->tm_min;
}

#endif // CONSTANTS_H
