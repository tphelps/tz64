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


static inline int64_t populate_ymd(struct tm *tm, int64_t days)
{
    // Pretend the year is 1 so we can use it for leap calculations.
    int64_t year = 1;

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

    int leap = is_leap(year);
    tm->tm_mon = days / 32;
    if (days >= month_starts[leap][tm->tm_mon + 1]) {
        tm->tm_mon++;
    }

    tm->tm_mday = days - month_starts[leap][tm->tm_mon] + 1;
    return year - 1;
}


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

    year += populate_ymd(tm, ts);
    tm->tm_year = year - base_year;
    return year + populate_ymd(tm, ts);
}


static inline int64_t daynum(int64_t year, int mon, int day)
{
    // Rotate the start of the year to March so that the troublesome
    // leap day is last.  Also, make March month number 4 to simplify
    // the calculation below.
    if (mon > 2) {
        mon += 1;
    } else {
        mon += 13;
        year -= 1;
    }

    // Compute the day number since the start of year 1.  This clever
    // expression is thanks to Tony Finch; see his blog post for a
    // detailed explanation of what's going on here:
    //     https://dotat.at/@/2008-09-10-counting-the-days.html
    return year * 1461 / 4 - year / 100 + year / 400 + mon * 153 / 5 + day - 428;
}


static int64_t tm_utc_to_ts(const struct tm *tm)
{
    int64_t ts = 0;
    ts += tm->tm_sec;
    ts += tm->tm_min * secs_per_min;
    ts += tm->tm_hour * secs_per_hour;
    int64_t days = daynum(tm->tm_year + base_year, tm->tm_mon + 1, tm->tm_mday) - daynum(ref_year, 1, 1);
    ts += days * secs_per_day;
    return ts;
}


static int64_t expand_ts(const int32_t *timestamps, int i)
{
    return tz64_year_starts[i / 2] + timestamps[tz64_year_types[i / 2] * 2 + (i & 1)];
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


static int find_extra_fwd_index(const int32_t *timestamps, int64_t adj_ts)
{
    int i = adj_ts / avg_secs_per_year * 2;
    for (; i < 800; i++) {
        if (adj_ts < expand_ts(timestamps, i)) {
            break;
        }
    }

    return i;
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


static uint32_t find_rev_index(const struct tz64* restrict tz, int64_t ts)
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


static inline int64_t calc_adj_ts(int64_t ts)
{
    int64_t adj_ts = (ts - alt_ref_ts) % secs_per_400_years;
    if (adj_ts < 0) {
        adj_ts += secs_per_400_years;
    }

    return adj_ts;
}


static int find_extra_rev_index(const struct tz64* restrict tz, int64_t adj_ts)
{
    int i = (adj_ts / avg_secs_per_year) * 2;

    for (; i < 800; i++) {
        if (adj_ts - tz->offsets[tz->offset_map[(i & 1) - 2]].utoff < expand_ts(tz->extra_ts, i)) {
            break;
        }
    }

    return i - 1;
}


struct tm *tz64_ts_to_tm(const struct tz64* restrict tz, int64_t ts, struct tm *restrict tm)
{
    // Don't even bother if we know the year will overflow 32 bits.
    if (ts < min_tm_ts || ts > max_tm_ts) {
        errno = EOVERFLOW;
        return NULL;
    }

    // Figure out how many leap seconds we're dealing wih.
    int32_t lsec = 0, extra = 0;
    if (tz->leap_count != 0) {
        const uint32_t li = find_fwd_index(tz->leap_ts, tz->leap_count, ts);
        extra = (tz->leap_ts[li] - 60 < ts && ts <= tz->leap_ts[li]) ? 1 : 0;
        lsec = tz->leap_secs[li] - extra;
    }

    // Figure out which offset to apply.
    const struct tz_offset *offset;
    if (ts < tz->timestamps[tz->ts_count - 1]) {
        // Do a binary search to find the index of latest timestamp no
        // later than t, and adjust the timestamp.
        const uint32_t i = find_fwd_index(tz->timestamps, tz->ts_count, ts);
        offset = &tz->offsets[tz->offset_map[i]];
    } else if (tz->extra_ts == NULL) {
        offset = &tz->offsets[tz->offset_map[tz->ts_count - 1]];
    } else {
        // Adjust the timestamp to seconds since 2001-01-01 00:00:00.
        int64_t adj_ts = calc_adj_ts(ts);

        // Bisect to find the offset that applies.
        const int i = find_extra_fwd_index(tz->extra_ts, adj_ts);
        offset = &tz->offsets[tz->offset_map[((i + 1) & 1) - 2]];
    }

    // Convert that to broken-down time as if it were UTC.
    int64_t year = ts_to_tm_utc(tm, ts + offset->utoff - lsec - extra);

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

    // Convert the year, month and day to days since the beginning of
    // 1 AD.
    int64_t days = daynum(tm->tm_year + base_year, tm->tm_mon + 1, tm->tm_mday) + overflow;
    days -= 1;

    // Convert days into 400 year blocks and extra.
    int64_t year = 1 + 400 * (days / days_per_400_years);
    days %= days_per_400_years;
    if (days < 0) {
        days += days_per_400_years;
        year -= 1;
    }

    year += populate_ymd(tm, days);
    tm->tm_year = year - base_year;

    return year;
}


int64_t tz64_tm_to_ts(const struct tz64 *tz, struct tm *tm)
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
    int64_t leap_ts = 0;
    int32_t lsec = 0;
    if (tz->leap_count != 0) {
        tm->tm_sec = sec;
        ts += sec;
        recalc = (sec < 0 || sec > 59) ? 1 : 0;

        // Adjust for leap seconds.
        uint32_t li = find_rev_leap(tz, encode_ymdhm(tm));
        lsec = tz->leap_secs[li];
        ts += lsec;
        leap_ts = (li + 1 < tz->leap_count) ? tz->leap_ts[li + 1] : INT64_MAX;
    }

    // Find the latest offset that contains the timestamp.
    const struct tz_offset *offset, *prev_offset, *next_offset;
    int64_t curr_ts, next_ts;
    int64_t curr_trans, next_trans;
    if (ts - tz->offsets[tz->offset_map[tz->ts_count - 1]].utoff < tz->timestamps[tz->ts_count - 1]) {
        uint32_t i = find_rev_index(tz, ts);
        offset = &tz->offsets[tz->offset_map[i]];
        curr_ts = ts;
        curr_trans = tz->timestamps[i];
        prev_offset = (i == 0) ? NULL : &tz->offsets[tz->offset_map[i - 1]];

        if (i + 1 < tz->ts_count) {
            next_offset = &tz->offsets[tz->offset_map[i + 1]];
            next_ts = ts;
            next_trans = tz->timestamps[i + 1];
        } else if (tz->extra_ts == NULL) {
            next_offset = NULL;
            next_ts = 0;
            next_trans = 0;
        } else {
            int64_t adj_ts = calc_adj_ts(ts);
            next_ts = adj_ts;
            int j = find_extra_rev_index(tz, adj_ts);
            next_offset = &tz->offsets[tz->offset_map[(j & 1) - 2]];
            next_trans = expand_ts(tz->extra_ts, j);
        }
    } else if (tz->extra_ts == NULL) {
        uint32_t i = tz->ts_count - 1;
        offset = &tz->offsets[tz->offset_map[i]];
        curr_ts = ts;
        curr_trans = tz->timestamps[i];
        prev_offset = (i == 0) ? NULL : &tz->offsets[tz->offset_map[i - 1]];

        next_ts = 0;
        next_offset = NULL;
        next_trans = 0;
    } else {
        int64_t adj_ts = calc_adj_ts(ts);
        int i = find_extra_rev_index(tz, adj_ts);
        offset = &tz->offsets[tz->offset_map[(i & 1) - 2]];
        curr_ts = adj_ts;
        curr_trans = expand_ts(tz->extra_ts, i);

        next_offset = &tz->offsets[tz->offset_map[((i + 1) & 1) - 2]];
        next_ts = adj_ts;
        next_trans = expand_ts(tz->extra_ts, i + 1);

        // Decide if the previous transition is explicit.
        int64_t diff = curr_trans - expand_ts(tz->extra_ts, i - 1);
        if (tz->ts_count >= 2 && ts - diff < tz->timestamps[tz->ts_count - 1]) {
            prev_offset = &tz->offsets[tz->offset_map[tz->ts_count - 2]];
        } else {
            prev_offset = &tz->offsets[tz->offset_map[((i + 1) & 1) - 2]];
        }
    }

    // Cope with problematic timestamps.
    if (next_offset != NULL && next_ts - offset->utoff >= next_trans) {
        // The time stamp is both after this offset's range and before
        // the next one: it's not a real time.  If the DST indicator
        // matches this one then we assume that something was added to
        // to a valid struct tm to push it into the next; otherwise
        // we'll assume subtraction from the next.
        if (tm->tm_isdst >= 0 && !tm->tm_isdst == !offset->isdst &&
            !tm->tm_isdst != !next_offset->isdst) {
            // Adjust the timestamp by the current offset.
            ts -= offset->utoff;

            // Recompute the broken-down time using the next offset.
            ts_to_tm_utc(tm, ts + next_offset->utoff);
            offset = next_offset;
        } else {
            // Adjust the timestamp by the next offset.
            ts -= next_offset->utoff;

            // Recompute broken-down time using the current offset.
            recalc = 1;
        }
    } else {
        // If the time could belong in either this offset or the
        // previous one then consult the dst indicator and, failing
        // that, the offset from UTC.
        if (tm->tm_isdst >= 0 && prev_offset != NULL && curr_ts - prev_offset->utoff < curr_trans) {
            if (!tm->tm_isdst == !prev_offset->isdst &&
                (!tm->tm_isdst != !offset->isdst || tm->tm_gmtoff == prev_offset->utoff)) {
                offset = prev_offset;
            }
        }

        ts -= offset->utoff;
    }

    if (recalc) {
        int extra = (leap_ts != 0 && tm->tm_sec == 60 && leap_ts - 60 <= ts && ts <= leap_ts) ? 1 : 0;
        ts_to_tm_utc(tm, ts + offset->utoff - lsec - extra);
        tm->tm_sec += extra;
    }

    // We've chosen our offset.  Use it to fill in the remaining
    // parts of broken-down time.
    tm->tm_isdst = offset->isdst;
    tm->tm_gmtoff = offset->utoff;
    tm->tm_zone = tz->desig + offset->desig;
    return ts;
}

////////////////////////////////////////////////////////////////////////
// End of tz64.c
