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
#include <errno.h>
#include "constants.h"
#include "tz64.h"
#include "tz64file.h"


// Populate most of the fields of a struct tm from a UTC timestamp.
static int64_t ts_to_tm_utc(struct tm *tm, int64_t ts)
{
    // Adjust to seconds since 2001-01-01.
    ts -= alt_ref_ts;

    // Divide out blocks of 400 years to the timestamp into a
    // convenient range.
    int64_t year = alt_ref_year;
    year += 400 * (ts / secs_per_400_years);
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


static int64_t tm_utc_to_ts(const struct tm *tm)
{
    int64_t ts = alt_ref_ts;
    ts += tm->tm_sec;
    ts += tm->tm_min * secs_per_min;
    ts += tm->tm_hour * secs_per_hour;
    ts += tm->tm_yday * secs_per_day;

    int64_t year = tm->tm_year + base_year - alt_ref_year;
    int64_t block = year / 400;
    year %= 400;
    if (year < 0) {
        year += 400;
        block -= 1;
    }

    ts += secs_per_400_years * block;
    ts += secs_per_nyear * year + secs_per_day * (year / 4 - year / 100);
    return ts;
}


static uint32_t find_fwd_index(const int64_t *timestamps, uint32_t count, int64_t ts)
{
    uint32_t lo = 0, hi = count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (timestamps[i] <= ts) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }

    return lo;
}


static uint32_t find_extra_fwd_index(const int32_t *timestamps, uint32_t count, int64_t ts)
{
    uint32_t lo = 0, hi = count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (tz64_year_starts[i / 2] + timestamps[tz64_year_types[i / 2] * 2 + (i & 1)] <= ts) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }

    return lo;
}


static uint32_t find_rev_leap(const struct tz64 *restrict tz, int64_t ymdhm)
{
    uint32_t lo = 0, hi = tz->leap_count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (tz->rev_leap_ts[i] <= ymdhm) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }

    return lo;
}


static uint32_t find_rev_offset(const struct tz64* restrict tz, int64_t ts)
{
    uint32_t lo = 0, hi = tz->ts_count - 1;
    while (lo < hi) {
        uint32_t i = (lo + hi + 1) / 2;
        if (tz->timestamps[i] <= ts - tz->offsets[tz->offset_map[i]].utoff) {
            lo = i;
        } else {
            hi = i - 1;
        }
    }

    return lo;
}


struct tm *localtime_rz(const struct tz64* restrict tz, time_t const *restrict ts, struct tm *restrict tm)
{
    int64_t t = *ts;

    // Don't even bother if we know the year will overflow 32 bits.
    if (t < min_tm_ts || t > max_tm_ts) {
        errno = EOVERFLOW;
        return NULL;
    }

    // Figure out how many leap seconds we're dealing wih.
    int32_t lsec = 0, extra = 0;
    if (tz->leap_count != 0) {
        const uint32_t li = find_fwd_index(tz->leap_ts, tz->leap_count, t);
        extra = (tz->leap_ts[li] == t) ? 1 : 0;
        lsec = tz->leap_secs[li] - extra;
    }

    // Figure out which offset to apply.
    const struct tz_offset *offset;
    if (tz->extra_ts == NULL || t < tz->timestamps[tz->ts_count - 1]) {
        // Do a binary search to find the index of latest timestamp no
        // later than t, and adjust the timestamp.
        const uint32_t i = find_fwd_index(tz->timestamps, tz->ts_count, t);
        offset = &tz->offsets[tz->offset_map[i]];
    } else {
        // Adjust the timestamp to seconds since 2001-01-01 00:00:00.
        int64_t adj_ts = (t - alt_ref_ts) % secs_per_400_years;

        // Bisect to find the offset that applies.
        const int i = find_extra_fwd_index(tz->extra_ts, 800, adj_ts);
        offset = &tz->offsets[tz->offset_map[(i & 1) - 2]];
    }

    // Convert that to broken-down time as if it were UTC.
    int64_t year = ts_to_tm_utc(tm, t + offset->utoff - lsec - extra);

    // Bump the second up to 60 if appropriate.
    tm->tm_sec += extra;

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


static void clamp(int *value, int64_t *overflow, int max)
{
    *overflow += *value;
    *value = *overflow % max;
    *overflow /= max;
    if (*value < 0) {
        *value += max;
        *overflow -= 1;
    }
}


static int64_t canonicalize_tm(struct tm *tm)
{
    // Convert to the time to canonical form.
    int64_t overflow = 0;
    clamp(&tm->tm_sec, &overflow, secs_per_min);
    clamp(&tm->tm_min, &overflow, mins_per_hour);
    clamp(&tm->tm_hour, &overflow, hours_per_day);

    // Add the overflow to the day of the month.
    int64_t days = tm->tm_mday + overflow - 1;

    // Limit that to no more than 400 days.
    int64_t year = tm->tm_year + base_year + (overflow / days_per_400_years) * 400;
    days %= days_per_400_years;
    if (days < 0) {
        days += days_per_400_years;
        year -= 400;
    }

    // Convert excess months to years.
    overflow = 0;
    clamp(&tm->tm_mon, &overflow, 12);
    year += overflow;
    int leap = is_leap(year);
    days += month_starts[leap][tm->tm_mon];

    // We have at most 400 extra years of days.  Convert those to
    // years in chunks where possible.
    while (days > days_per_nyear + leap) {
        if (days >= days_per_ncentury + leap && year % 100 == 0) {
            days -= days_per_ncentury + leap;
            year += 100;
        } else if (days >= days_per_nyear * 20 + 4 + leap && year % 20 == 0) {
            days -= days_per_nyear * 20 + 4 + leap;
            year += 20;
        } else if (days >= days_per_nyear * 4 + leap && year % 4 == 0) {
            days -= days_per_nyear * 4 + leap;
            year += 4;
        } else {
            days -= days_per_nyear + leap;
            year += 1;
        }
        leap = is_leap(year);
    }

    // Record the day of the year.
    tm->tm_yday = days;
    tm->tm_year = year - base_year;

    // Figure out which month that is.
    int month = days / 32;
    if (month_starts[leap][month + 1] <= days) {
        month++;
    }

    // Calculate month and mday.
    tm->tm_mon = month;
    tm->tm_mday = days - month_starts[leap][month] + 1;

    // Figure out the day of the week.
    int64_t yr = (year - alt_ref_year) % 400;
    if (yr < 0) {
        yr += 400;
    }
    days += yr * days_per_nyear + yr / 4 - yr / 100;
    tm->tm_wday = (days + 1) % days_per_week;

    return year;
}


int64_t mktime_z(const struct tz64 *tz, struct tm *tm)
{
    // Sequester the seconds when dealing with time zones that support
    // leap seconds.
    int sec = tm->tm_sec;
    if (tz->leap_count != 0) {
        tm->tm_sec = 0;
    }

    // Try to convert tm to canonical form.  If the tm overflows then
    // return -1.
    int64_t year = canonicalize_tm(tm);
    if (year - base_year != tm->tm_year) {
        tm->tm_sec = sec;
        return -1;
    }

    // Convert that to a timestamp as if it were UTC.
    int64_t ts = tm_utc_to_ts(tm);

    // Restore the seconds if necessary.
    int recalc = 0;
    if (tz->leap_count != 0) {
        tm->tm_sec = sec;
        ts += sec;
        recalc = (sec < 0 || sec > 59) ? 1 : 0;
    }

    // Adjust for leap seconds.
    const uint32_t li = (tz->leap_count == 0) ? 0 : find_rev_leap(tz, encode_ymdhm(tm));
    const int32_t lsec = (li == 0) ? 0 : tz->leap_secs[li];
    ts += lsec;

    // Do a binary search to find the latest offset that could
    // correspond to ts.
    uint32_t i = find_rev_offset(tz, ts);

    // Cope with problematic timestamps.
    if (i + 1 < tz->ts_count && ts - tz->offsets[tz->offset_map[i]].utoff >= tz->timestamps[i + 1]) {
        // The time stamp is both after this offset's range and before
        // the next one: it's not a real time.  If the DST indicator
        // matches this one then we assume that something was added to
        // to a valid struct tm to push it into the next; otherwise
        // we'll assume subtraction from the next.
        if (tm->tm_isdst >= 0 && !tm->tm_isdst == !tz->offsets[tz->offset_map[i]].isdst &&
            !tm->tm_isdst != !tz->offsets[tz->offset_map[i + 1]].isdst) {
            // Adjust the timestamp by the current offset.
            ts -= tz->offsets[tz->offset_map[i]].utoff;

            // Recompute the broken-down time using the next offset.
            ts_to_tm_utc(tm, ts + tz->offsets[tz->offset_map[i + 1]].utoff);
            i++;
        } else {
            // Adjust the timestamp by the next offset.
            ts -= tz->offsets[tz->offset_map[i + 1]].utoff;

            // Recompute broken-down time using the current offset.
            recalc = 1;
        }
    } else {
        if (tm->tm_isdst >= 0 && !tm->tm_isdst != !tz->offsets[tz->offset_map[i]].isdst &&
            i > 0 && !tm->tm_isdst == !tz->offsets[tz->offset_map[i - 1]].isdst &&
            ts - tz->offsets[tz->offset_map[i - 1]].utoff < tz->timestamps[i]) {
            // The time could belong in either this offset or the previous
            // one, but the DST indicators match for the previous one so
            // use that.
            i--;
        }

        ts -= tz->offsets[tz->offset_map[i]].utoff;
    }

    if (recalc) {
        int extra = (li < tz->leap_count && tz->leap_ts[li + 1] == ts) ? 1 : 0;
        ts_to_tm_utc(tm, ts + tz->offsets[tz->offset_map[i]].utoff - lsec - extra);
        tm->tm_sec += extra;
    }

    // We've chosen our offset.  Use it to fill in the remaining
    // parts of broken-down time.
    const struct tz_offset *offset = &tz->offsets[tz->offset_map[i]];
    tm->tm_isdst = offset->isdst;
    tm->tm_gmtoff = offset->utoff;
    tm->tm_zone = tz->desig + offset->desig;

    if (sizeof(time_t) == sizeof(int32_t) && (ts < INT32_MIN || ts > INT32_MAX)) {
        return -1;
    }

    return ts;
}
