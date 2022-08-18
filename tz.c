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
static int64_t ts_to_tm_utc(struct tm *tm, int64_t ts)
{
    // Adjust to seconds since 2001-01-01.
    ts -= alt_ref_ts;

    // Divide out blocks of 400 years to the timestamp into a
    // convenient range.
    int64_t year = alt_ref_year;
    year -= ts / secs_per_400_years;
    ts %= secs_per_400_years;
    if (ts < 0) {
        year -= 400;
        ts += secs_per_400_years;
    }
    
    // Divide the timestamp into hours, minutes and seconds.
    tm->tm_sec = ts % secs_per_min;
    ts /= secs_per_min;
    
    tm->tm_min = ts % mins_per_hour;
    ts /= mins_per_hour;
    
    tm->tm_hour = ts % hours_per_day;
    ts /= hours_per_day;

    // The ts now represents days.
    int64_t days = ts;

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


struct tm *localtime_rz(const struct tz64* restrict tz, time_t const *restrict ts, struct tm *restrict tm)
{
    int64_t t = *ts;

    // Don't even bother if we know the year will overflow 32 bits.
    if (t < min_tm_ts || t > max_tm_ts) {
        errno = EOVERFLOW;
        return NULL;
    }

    // Do a binary search to find the index of latest timestamp no
    // later than ts.    
    uint32_t lo = 0, hi = tz->ts_count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (tz->timestamps[i] <= t) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }
    uint32_t i = lo;

    // Fill in the tm with the timestamp adjusted by the offset.
    const struct tz_offset *offset = &tz->offsets[tz->offset_map[i]];
    int64_t year = ts_to_tm_utc(tm, t + offset->utoff);

    // Fill in the remaining fields from the offset.
    tm->tm_isdst = offset->isdst;
    tm->tm_gmtoff = offset->utoff;
    tm->tm_zone = tz->desig + offset->desig;

    // If the year overflowed/underflowed then indicate an error.
    if (year - base_year < INT32_MIN || year - base_year > INT32_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }

    return tm;
}

