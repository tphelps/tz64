#include <inttypes.h>
#include <errno.h>
#include "tz.h"
#include "tzfile.h"

static const int64_t secs_per_min = 60;
static const int64_t mins_per_hour = 60;
static const int64_t secs_per_hour = mins_per_hour * secs_per_min;
static const int64_t hours_per_day = 24;
static const int64_t secs_per_day = hours_per_day * secs_per_hour;

static const int64_t days_per_week = 7;
static const int64_t days_per_nyear = 365;

static const int64_t days_per_4_nyears = 4 * days_per_nyear + 1;
static const int64_t days_per_ncentury = 100 * days_per_nyear + 100 / 4 - 1;
static const int64_t days_per_400_years = 400 * days_per_nyear + 400 / 4 - 4 + 1;

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


static int is_leap(int year)
{
    if (year % 4 != 0) {
        return 0;
    } else if (year % 100 != 0) {
        return 1;
    } else if (year % 400 != 0) {
        return 0;
    } else {
        return 1;
    }
}



// Populate most of the fields of a struct tm from a UTC timestamp.
static int64_t ts_to_tm_utc(struct tm *tm, time_t ts)
{
    // Adjust to seconds since 2001-01-01.
    ts -= alt_ref_ts;
    
    // Divide the timestamp into hours, minutes and seconds.
    tm->tm_sec = ts % secs_per_min;
    ts /= secs_per_min;
    if (tm->tm_sec < 0) {
        tm->tm_sec += secs_per_min;
        ts -= 1;
    }
    
    tm->tm_min = ts % mins_per_hour;
    ts /= mins_per_hour;
    if (tm->tm_min < 0) {
        tm->tm_min += mins_per_hour;
        ts -= 1;
    }
    
    tm->tm_hour = ts % hours_per_day;
    ts /= hours_per_day;
    if (tm->tm_hour < 0) {
        tm->tm_hour += hours_per_day;
        ts -= 1;
    }

    // Divide into days in a 400 year block.
    int64_t days = ts % days_per_400_years;
    int64_t year = alt_ref_year + 400 * (ts / days_per_400_years);
    if (days < 0) {
        days += days_per_400_years;
        year -= 400;
    }

    // Every block of 400 days starts on the same day of the week, and
    // 2001-01-01 was a Monday.  Compute the day of the week.
    tm->tm_wday = (days + 1) % days_per_week;

    // Subtract to get within one century.  Due to our choice of
    // reference time, the leap year that divides by 100 is the last
    // year in the set.
    if (days >= days_per_ncentury * 2) {
        days -= days_per_ncentury * 2;
        year += 200;
    }

    if (days >= days_per_ncentury) {
        days -= days_per_ncentury;
        year += 100;
    }

    // Convert the remaining into years and days.
    int y = (days * 4 + 3) / days_per_4_nyears;
    year += y;
    days -= (y * days_per_nyear) + y / 4;

    tm->tm_yday = days;
    tm->tm_year = year - base_year;

    int leap = is_leap(year);
    tm->tm_mon = days / 32;
    if (days >= month_starts[leap][tm->tm_mon + 1]) {
        tm->tm_mon++;
    }

    tm->tm_mday = days - month_starts[leap][tm->tm_mon] + 1;
    return year;
}


struct tm *localtime_rz(struct time_zone* restrict tz, time_t const *restrict ts, struct tm *restrict tm)
{
    // Do a binary search to find the index of latest timestamp no
    // later than ts.    
    uint32_t lo = 0, hi = tz->ts_count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (tz->timestamps[i] <= *ts) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }
    uint32_t i = lo;

    // Fill in the tm with the timestamp adjusted by the offset.
    const struct tz_offset *offset = &tz->offsets[tz->offset_map[i]];
    int64_t year = ts_to_tm_utc(tm, *ts + offset->utoff);

    // Fill in the remaining fields from the offset.
    tm->tm_isdst = offset->isdst;
    tm->tm_gmtoff = offset->utoff;
    tm->tm_zone = tz->desig + offset->desig;

    // If the year overflowed/underflowed then indicate an error.
    if (year < INT32_MIN || year > INT32_MAX) {
        errno = ERANGE;
        return NULL;
    }

    return tm;
}

