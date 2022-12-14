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

// Time Zone information file reader

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <endian.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "constants.h"
#include "tz64.h"
#include "tz64file.h"

#define MAGIC "TZif"
#define ZONE_DIR "/usr/share/zoneinfo"
#define MAX_TZSTR_SIZE 63

enum rule_type {
    RT_NONE,
    RT_MONTH,
    RT_JULIAN,
    RT_0JULIAN
};

struct rule {
    char desig[8];
    int32_t utoff;

    enum rule_type type;
    uint16_t day;
    uint8_t week;
    uint8_t month;
    int32_t time;

    uint8_t offset_idx;
};


extern const char *progname;

static const int64_t utc_timestamps[1] = { INT64_MIN };
static const struct tz_offset utc_offsets[1] = { { 0, 0, 0 } };
static const uint8_t utc_offset_map[1] = { 0 };

struct tz64 tz_utc = {
    .ts_count = 1,
    .leap_count = 0,
    .timestamps = utc_timestamps,
    .offsets = utc_offsets,
    .offset_map = utc_offset_map,
    .leap_ts = NULL,
    .rev_leap_ts = NULL,
    .leap_secs = NULL,
    .desig = "UTC",
    .extra_ts = NULL,
};


static int parse_desig(char *buffer, size_t buflen, const char *str)
{
    const char *p = str;
    char *out = buffer;
    char *end = buffer + buflen - 1;

    if (isalpha(*p)) {
        do {
            if (out + 1 < end) {
                *out++ = *p++;
            } else {
                return -1;
            }
        } while (isalpha(*p));
    } else if (*p == '<') {
        p++;
        while (isalnum(*p) || *p == '+' || *p == '-') {
            *out++ = *p++;
        }

        if (*p++ != '>') {
            return -1;
        }
    } else {
        return -1;
    }

    *out = '\0';
    if (out - buffer < 3) {
        return -1;
    }
    return p - str;
}


static int parse_time(int32_t *time_out, const char *str)
{
    const char *p = str;

    int sign = 1;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    }

    if (!isdigit(*p)) {
        return -1;
    }

    int hour = *p++ - '0';
    if (isdigit(*p)) {
        hour = hour * 10 + *p++ - '0';
    }
    if (isdigit(*p)) {
        hour = hour * 10 + *p++ - '0';
    }
    int32_t time = hour * 3600;

    if (*p != ':') {
        *time_out = sign * time;
        return p - str;
    }
    p++;

    if (!isdigit(*p)) {
        return -1;
    }

    int min = *p++ - '0';
    if (isdigit(*p)) {
        min = min * 10 + *p++ - '0';
    }
    time += min * 60;

    if (*p != ':') {
        *time_out = sign * time;
        return p - str;
    }
    p++;

    if (!isdigit(*p)) {
        return -1;
    }
    int sec = *p++ - '0';
    if (isdigit(*p)) {
        sec = sec * 10 + *p++ - '0';
    }
    time += sec;

    *time_out = sign * time;
    return p - str;
}


static int parse_month_rule(struct rule *rule, const char *str)
{
    rule->type = RT_MONTH;

    const char *p = str;
    if (!isdigit(*p)) {
        return -1;
    }

    int mon = *p++ - '0';
    if (isdigit(*p)) {
        mon = mon * 10 + *p++ - '0';
    }
    rule->month = mon;

    if (*p++ != '.') {
        return -1;
    }

    if (*p < '1' || '5' < *p) {
        return -1;
    }
    rule->week = *p++ - '0';

    if (*p++ != '.') {
        return -1;
    }

    if (*p < '0' || '6' < *p) {
        return -1;
    }
    rule->day = *p++ - '0';

    if (*p != '/') {
        rule->time = 2 * 3600;
        return p - str;
    }
    p++;

    int len = parse_time(&rule->time, p);
    if (len < 0) {
        return len;
    }

    p += len;
    return p - str;
}


static int parse_jrule(struct rule *rule, const char *str, int min, int max)
{
    const char *p = str;
    int yday = 0;
    for (int i = 0; i < 3 && isdigit(*p); i++) {
        yday = yday * 10 + *p++ - '0';
    }

    if (yday < min || yday > max) {
        return -1;
    }
    rule->day = yday;

    if (*p != '/') {
        rule->time = 2 * 3600;
        return p - str;
    }
    p++;

    int len = parse_time(&rule->time, p);
    if (len < 0) {
        return len;
    }

    p += len;
    return p - str;
}


static int parse_julian_rule(struct rule *rule, const char *str)
{
    rule->type = RT_JULIAN;
    return parse_jrule(rule, str, 1, 365);
}


static int parse_0julian_rule(struct rule *rule, const char *str)
{
    rule->type = RT_0JULIAN;
    return parse_jrule(rule, str, 0, 365);
}


static int parse_rule(struct rule *rule, const char *str)
{
    if (*str == 'M') {
        int len = parse_month_rule(rule, str + 1);
        return (len < 0) ? len : (len + 1);
    } else if (*str == 'J') {
        int len = parse_julian_rule(rule, str + 1);
        return (len < 0) ? len : (len + 1);
    } else if (isdigit(*str)) {
        return parse_0julian_rule(rule, str);
    } else {
        return -1;
    }
}


static int find_offset(const struct tz64 *tz, uint8_t typecnt, const char *desig, int32_t utoff, int isdst)
{
    for (uint8_t i = 0; i < typecnt; i++) {
        if (tz->offsets[i].utoff == utoff &&
            tz->offsets[i].isdst == isdst &&
            strcmp(desig, tz->desig + tz->offsets[i].desig) == 0) {
            return i;
        }
    }

    return -1;
}


static int parse_tz_string(struct rule* rules, const char *s)
{
    // POSIX specifies: stdoffset[dst[offset][,start[/time],end[/time]]]
    // We support: stdoffset[dst[offset],start[/time],end[/time]]
    // (start and end are mandatory if dst exiss)

    // Parse the designation
    memset(rules, 0, sizeof(struct rule) * 2);
    int len = parse_desig(rules[0].desig, sizeof(rules[0].desig), s);
    if (len < 0) {
        return -1;
    }
    s += len;

    // Parse the time.
    len = parse_time(&rules[0].utoff, s);
    if (len < 0) {
        return -1;
    }
    s += len;
    rules[0].utoff = -rules[0].utoff;

    // If no daylight savings time then we're done.
    if (*s == '\0') {
        rules[0].type = RT_NONE;
        rules[1].type = RT_NONE;
        return 0;
    }

    // Parse the daylight savings time designation
    len = parse_desig(rules[1].desig, sizeof(rules[1].desig), s);
    if (len < 0) {
        return -1;
    }
    s += len;

    // And offset, if any.
    rules[1].utoff = rules[0].utoff + 3600;
    if (*s != '\0' && (isdigit(*s) || *s == '+' || *s == '-')) {
        len = parse_time(&rules[1].utoff, s);
        if (len < 0) {
            return -1;
        }
        s += len;
        rules[1].utoff = -rules[1].utoff;
    }

    // If there are no rules then just fail.
    if (*s++ != ',') {
        return -1;
    }

    len = parse_rule(&rules[0], s);
    if (len < 0) {
        return -1;
    }
    s += len;

    if (*s++ != ',') {
        return -1;
    }

    len = parse_rule(&rules[1], s);
    if (len < 0) {
        return -1;
    }
    s += len;

    // Check for trailing garbage.
    if (*s != '\0') {
        return -1;
    }

    return 0;
}


static bool is_always_dst(const struct rule *rules)
{
    // As per tzfile(5): "DST is in effect all year if it starts
    // January 1 at 00:00 and ends December 31 at 24:00 plus the
    // difference between daylight savings and standard time."
    return
        ((rules[0].type == RT_JULIAN && rules[0].day == 1) ||
         (rules[0].type == RT_0JULIAN && rules[0].day == 0)) &&
        rules[0].time == 0 &&
        rules[1].type == RT_JULIAN &&
        rules[1].day == days_per_nyear &&
        rules[1].time == secs_per_day + rules[1].utoff - rules[0].utoff;
}


static int find_offset_for_ts(const struct tz64 *tz, int64_t ts)
{
    // Find the offset in the 400 year block.
    int64_t adj_ts = (ts - alt_ref_ts) % secs_per_400_years;
    if (adj_ts < 0) {
        adj_ts += secs_per_400_years;
    }

    // Figure out which extra transition that corresponds to.
    int i = adj_ts / avg_secs_per_year * 2;
    for (; i < 800; i++) {
        if (adj_ts < tz64_year_starts[i / 2] + tz->extra_ts[tz64_year_types[i / 2] * 2 + (i & 1)]) {
            break;
        }
    }

    return tz->offset_map[((i + 1) & 1) - 2];
}


static bool tz_offsets_equal(const struct tz64 *tz, uint8_t a, uint8_t b)
{
    return tz->offsets[a].utoff == tz->offsets[b].utoff &&
        tz->offsets[a].isdst == tz->offsets[b].isdst &&
        strcmp(tz->desig + tz->offsets[a].desig, tz->desig + tz->offsets[b].desig) == 0;
}


static int day_of_week(int year, int month, int day)
{
    month -= 2;
    if (month < 1) {
        month += 12;
        year--;
    }

    return (day + (26 * month - 2) / 10 + year + year / 4 - year / 100 + year / 400) % days_per_week;
}


static int64_t calc_month_trans(const struct tz64 *tz, const struct rule *rule, int year)
{
    // Compute the first day of the target month using Zeller's congruence.
    int wday = day_of_week(year, rule->month, 1);

    // Find the first target day of the month.
    int mday = rule->day - wday + 1;
    if (mday <= 0) {
        mday += days_per_week;
    }

    // Add the necessary number of weeks.
    mday += (rule->week - 1) * days_per_week;

    // Make sure that stays within the month.
    int leap = is_leap(year);
    if (month_starts[leap][rule->month - 1] + mday - 1 >= month_starts[leap][rule->month]) {
        mday -= days_per_week;
    }

    // Convert that to a timestamp in UTC.
    struct tm tm;
    tm.tm_sec = rule->time;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = mday;
    tm.tm_mon = rule->month - 1;
    tm.tm_year = year - base_year;
    tm.tm_isdst = 0;
    return tz64_tm_to_ts(&tz_utc, &tm) - tz->offsets[rule->offset_idx].utoff;
}


static int64_t calc_julian_trans(const struct tz64 *tz, const struct rule *rule, int year)
{
    int day = rule->day - 1;
    int mon = day / 32;
    if (day > month_starts[0][mon + 1]) {
        mon++;
    }
    day -= month_starts[0][mon];

    struct tm tm;
    tm.tm_sec = rule->time;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = day + 1;
    tm.tm_mon = mon;
    tm.tm_year = year - base_year;
    tm.tm_isdst = 0;
    return tz64_tm_to_ts(&tz_utc, &tm) - tz->offsets[rule->offset_idx].utoff;
}


static int64_t calc_0julian_trans(const struct tz64 *tz, const struct rule *rule, int year)
{
    // Look up the first day of the year.
    struct tm tm;
    tm.tm_sec = rule->time;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_year = year - base_year;
    tm.tm_isdst = 0;
    int64_t ts = tz64_tm_to_ts(&tz_utc, &tm);

    // Add to get the day of the year.
    ts += rule->day * secs_per_day;

    // And adjust for the local offset.
    return ts - tz->offsets[rule->offset_idx].utoff;
}


static int64_t calc_trans(const struct tz64 *tz, const struct rule *rule, int year)
{
    switch (rule->type) {
    case RT_MONTH:
        return calc_month_trans(tz, rule, year);

    case RT_JULIAN:
        return calc_julian_trans(tz, rule, year);

    case RT_0JULIAN:
        return calc_0julian_trans(tz, rule, year);

    default:
        abort();
    }
}


static int32_t calc_adj_trans(const struct tz64 *tz, const struct rule *rule, int year)
{
    int64_t ts = calc_trans(tz, rule, year);

    // Assume no leap seconds added after the last translation.
    if (tz->leap_count != 0) {
        ts += tz->leap_secs[tz->leap_count - 1];
    }

    return (ts - alt_ref_ts) % secs_per_400_years - tz64_year_starts[year - 2001];
}


static int populate_extra_ts(int32_t *ts, const struct tz64 *tz, const struct rule *rules)
{
    // Repeat for 13 other carefully chosen years to get the full set
    // of 14.
    const int years[14] = {
        2006, 2001, 2002, 2003, 2009, 2010, 2005,
        2012, 2024, 2008, 2020, 2004, 2016, 2028
    };

    // Calculate the transitions for year 2006.
    int32_t std = calc_adj_trans(tz, &rules[0], years[0]);
    int32_t dst = calc_adj_trans(tz, &rules[1], years[0]);

    // Work out which comes first, and record it.
    int adj = (std < dst) ? 0 : 1;
    ts[adj] = std;
    ts[1 - adj] = dst;

    // Compute the transitions for the remaining 400 years.
    for (int i = 1; i < days_per_week * 2; i++) {
        ts[i * 2] = calc_adj_trans(tz, &rules[adj], years[i]);
        ts[i * 2 + 1] = calc_adj_trans(tz, &rules[1 - adj], years[i]);
    }

    return adj;
}


size_t tz_header_data_len(const struct tz_header *header, size_t time_size)
{
    return
        header->timecnt * time_size +
        header->timecnt +
        header->typecnt * (sizeof(int32_t) + 1 + 1) +
        header->charcnt +
        header->leapcnt * (time_size + sizeof(int32_t)) +
        header->isstdcnt +
        header->isutcnt;
}


void tz_header_fix_endian(struct tz_header *header)
{
    header->isutcnt = be32toh(header->isutcnt);
    header->isstdcnt = be32toh(header->isstdcnt);
    header->leapcnt = be32toh(header->leapcnt);
    header->timecnt = be32toh(header->timecnt);
    header->typecnt = be32toh(header->typecnt);
    header->charcnt = be32toh(header->charcnt);
}


static struct tz64 *process_tzfile(const char *path, const char *data, off_t size)
{
    const char *end = data + size;

    if (data + sizeof(struct tz_header) >= end) {
        errno = EINVAL;
        return NULL;
    }

    // Bail if the magic is wrong.
    if (memcmp(data, MAGIC, strlen(MAGIC)) != 0) {
        errno = EINVAL;
        return NULL;
    }


    // Bail if the version isn't supported.
    char version = data[4];
    if (version < '2' || version > '9') {
        errno = EINVAL;
        return NULL;
    }

    // Make a copy of the header and adjust the byte order.
    struct tz_header header;
    memcpy(&header, data, sizeof(header));
    data += sizeof(struct tz_header);
    tz_header_fix_endian(&header);

    // Make sure the v1 data is present and skip it.
    const size_t v1_size = tz_header_data_len(&header, sizeof(int32_t));
    if (end - data < v1_size) {
        errno = EINVAL;
        return NULL;
    }
    data += v1_size;

    // Make sure there's a v2 header.
    if (end - data < sizeof(struct tz_header)) {
        errno = EINVAL;
        return NULL;
    }

    // Make a copy of the v2 header and adjust the byte order.
    memcpy(&header, data, sizeof(struct tz_header));
    data += sizeof(header);
    tz_header_fix_endian(&header);

    // Check the second header's magic.
    if (memcmp(header.magic, MAGIC, strlen(MAGIC)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    // Make sure the file is big enough for the v2 data.
    size_t v2_size = tz_header_data_len(&header, sizeof(int64_t));
    if (end - data < v2_size) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate memory for the v2 data.
    size_t block_size =
        sizeof(struct tz64) +
        (header.timecnt + 1) * sizeof(int64_t) +
        header.typecnt * sizeof(struct tz_offset) +
        (header.leapcnt + 1) * sizeof(int64_t) +
        (header.leapcnt + 1) * sizeof(int64_t) +
        (header.leapcnt + 1) * sizeof(int32_t) +
        days_per_week * 2 * 2 * sizeof(int32_t) +
        2 + header.timecnt + 1 +
        header.charcnt;
    char *block = calloc(1, block_size);
    if (block == NULL) {
        return NULL;
    }

    struct tz64 *tz = (struct tz64 *)block;
    block += sizeof(struct tz64);

    // Set up pointers to the various fields.
    int64_t *timestamps = (int64_t *)block;
    block += (header.timecnt + 1) * sizeof(int64_t);
    struct tz_offset *offsets = (struct tz_offset *)block;
    block += header.typecnt * sizeof(struct tz_offset);
    int64_t *leap_ts = NULL;
    int64_t *rev_leap_ts = NULL;
    int32_t *leap_secs = NULL;
    if (header.leapcnt != 0) {
        leap_ts = (int64_t *)block;
        block += (header.leapcnt + 1) * sizeof(int64_t);
        rev_leap_ts = (int64_t *)block;
        block += (header.leapcnt + 1) * sizeof(int64_t);
        leap_secs = (int32_t *)block;
        block += (header.leapcnt + 1) * sizeof(int32_t);
    }
    int32_t *extra_ts = (int32_t *)block;
    block += days_per_week * 2 * 2 * sizeof(int32_t);
    uint8_t *offset_map = (uint8_t *)block + 2;
    block += 2 + header.timecnt + 1;
    char *desig = block;
    block += header.charcnt;

    // Read in the timestamps.
    timestamps[0] = INT64_MIN;
    for (uint32_t i = 0; i < header.timecnt; i++) {
        int64_t ts;
        memcpy(&ts, data, sizeof(ts));
        data += sizeof(ts);
        ts = be64toh(ts);

        if (ts <= timestamps[i]) {
            errno = EINVAL;
            goto err;
        }
        timestamps[i + 1] = ts;
    }

    // Read the timestamp/offset map.
    offset_map[-2] = 0;
    offset_map[-1] = 0;
    offset_map[0] = 0;
    memcpy(offset_map + 1, data, header.timecnt);
    for (uint32_t i = 0; i < header.timecnt; i++) {
        if (offset_map[i] > header.typecnt) {
            errno = EINVAL;
            goto err;
        }
    }
    data += header.timecnt;

    // Read the time types.
    for (uint32_t i = 0; i < header.typecnt; i++) {
        int32_t utoff;
        memcpy(&utoff, data, sizeof(utoff));
        data += sizeof(utoff);
        utoff = be32toh(utoff);

        offsets[i].utoff = utoff;
        offsets[i].isdst = *data++;
        offsets[i].desig = *data++;

        if (offsets[i].desig >= header.charcnt) {
            errno = EINVAL;
            goto err;
        }
    }

    // Read the designations.
    memcpy(desig, data, header.charcnt);
    if (desig[header.charcnt -1] != '\0') {
        errno = EINVAL;
        goto err;
    }
    data += header.charcnt;
    
    // Read the leap second information.
    if (header.leapcnt != 0) {
        leap_ts[0] = INT64_MIN;
        rev_leap_ts[0] = INT64_MIN;
        leap_secs[0] = 0;
        for (uint32_t i = 0; i < header.leapcnt; i++) {
            int64_t ts;
            memcpy(&ts, data, sizeof(ts));
            data += sizeof(ts);
            ts = be64toh(ts);

            if (ts <= leap_ts[i]) {
                errno = EINVAL;
                goto err;
            }
            leap_ts[i + 1] = ts;

            int32_t secs;
            memcpy(&secs, data, sizeof(secs));
            data += sizeof(secs);
            secs = be32toh(secs);

            if (secs <= leap_secs[i]) {
                errno = EINVAL;
                goto err;
            }
            leap_secs[i + 1] = secs;
        }
    }

    // Skip the standard/wall indicators.
    data += header.isstdcnt;

    // Skip the UT/local indicators.
    data += header.isutcnt;

    // Read the footer.
    char tzbuf[MAX_TZSTR_SIZE + 1];
    if (data == end) {
        tzbuf[0] = '\0';
    } else {
        if (*data++ != '\n') {
            errno = EINVAL;
            goto err;
        }

        // Find a newline.
        size_t s = (end - data) < MAX_TZSTR_SIZE ? (end - data) : MAX_TZSTR_SIZE;
        const char *nl = memchr(data, '\n', s);
        if (nl == NULL) {
            errno = EINVAL;
            goto err;
        }

        // Copy the time zone string so we can NUL-terminate it.
        memcpy(tzbuf, data, nl - data);
        tzbuf[nl - data] = '\0';
    }

    // Populate the tz itself, except for the reverse leap seconds.
    tz->ts_count = header.timecnt + 1;
    tz->leap_count = (header.leapcnt == 0) ? 0 : (header.leapcnt + 1);
    tz->timestamps = timestamps;
    tz->offset_map = offset_map;
    tz->leap_ts = leap_ts;
    tz->rev_leap_ts = NULL;
    tz->leap_secs = leap_secs;
    tz->offsets = offsets;
    tz->desig = desig;
    tz->extra_ts = NULL;

    // It looks like glibc's zic provides a duplicate final time entry
    // at INT32_MAX for time zones that don't have daylight savings
    // time, apparently in an effort to work around poor performance
    // of glibc's conversion functions after the last explicit time
    // stamp.  Our performance is better without them, so detect and
    // remove them.
    if (tz->ts_count > 1 && tz->timestamps[tz->ts_count - 1] == INT32_MAX &&
        tz->offset_map[tz->ts_count - 1] == tz->offset_map[tz->ts_count - 2]) {
        tz->ts_count--;
    }

    // Parse the tz string.
    if (tzbuf[0] != '\0') {
        struct rule rules[2];
        if (parse_tz_string(rules, tzbuf) < 0) {
            errno = EINVAL;
            goto err;
        }

        // If there's a DST entry then look up its offset.
        if (rules[1].type != RT_NONE) {
            // Look up the tz_offset.
            int offset = find_offset(tz, header.typecnt, rules[1].desig, rules[1].utoff, 1);
            if (offset < 0) {
                errno = EINVAL;
                goto err;
            }
            rules[1].offset_idx = offset;
        }

        if (is_always_dst(rules)) {
            // It's always DST: make sure the tz_offset entry matches
            // the last transition.
            if (!tz_offsets_equal(tz, rules[1].offset_idx, tz->offset_map[tz->ts_count - 1])) {
                errno = EINVAL;
                goto err;
            }
        } else {
            // It's standard time at least some of the time.  Make
            // sure its offset is present.
            int offset = find_offset(tz, header.typecnt, rules[0].desig, rules[0].utoff, 0);
            if (offset < 0) {
                errno = EINVAL;
                goto err;
            }
            rules[0].offset_idx = offset;

            // If it's always standard time then make sure it matches
            // the last transition.
            if (rules[1].type == RT_NONE) {
                if (!tz_offsets_equal(tz, rules[0].offset_idx, tz->offset_map[tz->ts_count - 1])) {
                    errno = EINVAL;
                    goto err;
                }
            } else {
                // Otherwise it's also DST some times.  Compute the
                // transition times for 14 representative years.
                int adj = populate_extra_ts(extra_ts, tz, rules);
                tz->extra_ts = extra_ts;
                offset_map[-2] = rules[1 - adj].offset_idx;
                offset_map[-1] = rules[adj].offset_idx;

                // Check that the last explicit transition matches the
                // rule in effect at that moment.
                int i = find_offset_for_ts(tz, tz->timestamps[tz->ts_count - 1]);
                if (!tz_offsets_equal(tz, i, tz->offset_map[tz->ts_count - 1])) {
                    errno = EINVAL;
                    goto err;
                }
            }
        }
    }

    // Compute local time for each leap second.
    for (uint32_t i = 0; i < header.leapcnt; i++) {
        time_t ts = tz->leap_ts[i + 1] + 1;

        struct tm tm;
        if (tz64_ts_to_tm(tz, ts, &tm) == NULL) {
            errno = EINVAL;
            goto err;
        }

        rev_leap_ts[i + 1] = encode_ymdhm(&tm);
    }

    tz->rev_leap_ts = rev_leap_ts;
    return tz;

err:
    free(tz);
    return NULL;
}


static int load_tz(struct tz64 **tz_out, const char *path)
{
    // Open the file.
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return ENOENT;
    }

    // Figure out how big it is.
    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0) {
        int err = errno;
        close(fd);
        return err;
    }

    // Map it into memory.
    char *data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == NULL) {
        int err = errno;
        (void)close(fd);
        return err;
    }

    // Decode the data in the file.
    *tz_out = process_tzfile(path, data, statbuf.st_size);

    // Clean up.
    int res = munmap(data, statbuf.st_size);
    assert(res == 0);
    
    res = close(fd);
    assert(res == 0);

    return 0;
}


static struct tz64 *make_tz_from_one_rule(const struct rule *rule, int isdst)
{
    size_t len = sizeof(struct tz64) + sizeof(struct tz_offset) + 1 + strlen(rule->desig);
    char *block = calloc(1, len);
    struct tz64 *tz = (struct tz64 *)block;
    block += sizeof(struct tz64);
    struct tz_offset *offsets = (struct tz_offset *)block;
    block += sizeof(struct tz_offset);
    uint8_t *offset_map = (uint8_t *)block++;
    char *desig = block;

    offsets[0].utoff = rule->utoff;
    offsets[0].isdst = isdst;
    offsets[0].desig = 0;
    offset_map[0] = 0;
    strcpy(desig, rule->desig);

    tz->ts_count = 1;
    tz->timestamps = utc_timestamps;
    tz->offset_map = offset_map;
    tz->offsets = offsets;
    tz->desig = desig;
    return tz;
}


static struct tz64 *make_tz_from_two_rules(const struct rule *rules)
{
    // Deal with the case of two rules.
    size_t len =
        sizeof(struct tz64) +
        sizeof(struct tz_offset) * 2 + 4 +
        sizeof(int32_t) * days_per_week * 2 * 2 +
        strlen(rules[0].desig) + 1 + strlen(rules[1].desig) + 1;

    char *block = calloc(1, len);
    struct tz64 *tz = (struct tz64 *)block;
    block += sizeof(struct tz64);
    struct tz_offset *offsets = (struct tz_offset *)block;
    block += sizeof(struct tz_offset) * 2;
    tz->extra_ts = (int32_t *)block;
    int32_t *extra_ts = (int32_t *)block;
    block += sizeof(int32_t) * days_per_week * 2 * 2;
    uint8_t *offset_map = (uint8_t *)block + 2;
    block += 4;
    char *desig = block;

    offsets[0].utoff = rules[0].utoff;
    offsets[0].isdst = 0;
    offsets[0].desig = 0;
    size_t dlen = strlen(rules[0].desig);
    memcpy(desig, rules[0].desig, dlen + 1);
    offset_map[0] = 0;

    offsets[1].utoff = rules[1].utoff;
    offsets[1].isdst = 1;
    offsets[1].desig = dlen + 1;
    strcpy(desig + dlen + 1, rules[1].desig);
    offset_map[1] = 1;

    tz->ts_count = 1;
    tz->timestamps = utc_timestamps;
    tz->offset_map = offset_map;
    tz->offsets = offsets;
    tz->desig = desig;
    tz->extra_ts = extra_ts;

    int adj = populate_extra_ts(extra_ts, tz, rules);
    offset_map[-2] = rules[1 - adj].offset_idx;
    offset_map[-1] = rules[adj].offset_idx;
    return tz;
}


static struct tz64 *make_tz_from_string(const char *str)
{
    // Try to parse the string.
    struct rule rules[2];
    if (parse_tz_string(rules, str) < 0) {
        return NULL;
    }

    if (is_always_dst(rules)) {
        return make_tz_from_one_rule(&rules[1], 1);
    } else if (rules[1].type == RT_NONE) {
        return make_tz_from_one_rule(&rules[0], 0);
    } else {
        rules[0].offset_idx = 0;
        rules[1].offset_idx = 1;
        return make_tz_from_two_rules(rules);
    }
}


static struct tz64 *make_utc()
{
    struct tz64 *tz = malloc(sizeof(struct tz64));
    if (tz == NULL) {
        return tz;
    }

    memcpy(tz, &tz_utc, sizeof(struct tz64));
    return tz;
}


static char *mkpath(char *buffer, size_t buflen, const char *rest)
{
    size_t baselen = strlen(ZONE_DIR);
    size_t restlen = strlen(rest);
    size_t len = baselen + 1 + restlen + 1;
    if (len > buflen) {
        return NULL;
    }

    char *p = buffer;
    memcpy(p, ZONE_DIR, baselen);
    p += baselen;
    *p++ = '/';
    memcpy(p, rest, restlen);
    p += restlen;
    *p++ = '\0';

    return buffer;
}


struct tz64 *tz64_alloc(const char *tz_desc)
{
    char pathbuf[256];
    struct tz64 *tz;

    // NULL indicates localtime.
    if (tz_desc == NULL) {
        // Try /usr/share/zoneinfo/localtime
        int res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), "localtime"));
        if (res == 0) {
            return tz;
        }

        // Failing that, try /etc/localtime
        res = load_tz(&tz, "/etc/localtime");
        if (res == 0) {
            return tz;
        }
    }

    // An empty string means UTC (if no localtime then use UTC).
    if (tz_desc == NULL || *tz_desc == '\0') {
        // Then try UTC.
        int res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), "UTC"));
        if (res == 0) {
            return tz;
        }

        // Create UTC.
        return make_utc();
    }

    // If the description begins with a colon then treat it as a path.
    if (*tz_desc == ':') {
        const char *path = tz_desc + 1;
        if (*path == '/') {
            errno = load_tz(&tz, path);
        } else {
            errno = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), path));
        }
        return tz;
    }

    // No colon, but try reading as a path anyway.
    int res;
    if (*tz_desc == '/') {
        res = load_tz(&tz, tz_desc);
    } else {
        res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), tz_desc));
    }
    if (res != ENOENT) {
        errno = res;
        return tz;
    }

    // Try using the string as a POSIX TZ expression.
    return make_tz_from_string(tz_desc);
}


void tz64_free(struct tz64 *tz)
{
    free(tz);
}

////////////////////////////////////////////////////////////////////////
// End of tz64file.c
