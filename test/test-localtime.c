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
#include <assert.h>
#include <tz64.h>
#include "utils.h"

int main(int argc, char *argv[])
{
    // Load a few time zones.
    struct tz64 *tz_new_york, *tz_melbourne, *tz_hong_kong, *tz_london;
    tz_new_york = tzalloc("America/New_York");
    assert(tz_new_york != NULL);
    tz_melbourne = tzalloc("Australia/Melbourne");
    assert(tz_melbourne != NULL);
    tz_hong_kong = tzalloc("Asia/Hong_Kong");
    assert(tz_hong_kong != NULL);
    tz_london = tzalloc("Europe/London");
    assert(tz_london != NULL);

    // Look up zero in each time zone.
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    time_t ts = 0;
    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_melbourne, &ts, &tm) == &tm);
    assert_tm(ts, 1970, 1, 1, 10, 0, 0, 0, DOW_THU, 1, 10 * 3600, "AEST", &tm);

    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_hong_kong, &ts, &tm) == &tm);
    assert_tm(ts, 1970, 1, 1, 8, 0, 0, 0, DOW_THU, 1, 8 * 3600, "HKT", &tm);

    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_london, &ts, &tm) == &tm);
    assert_tm(ts, 1970, 1, 1, 1, 0, 0, 0, DOW_THU, 1, 3600, "BST", &tm);

    memset(&tm, 0x5a, sizeof(tm));
    assert(localtime_rz(tz_new_york, &ts, &tm) == &tm);
    assert_tm(ts, 1969, 12, 31, 19, 0, 0, 0, DOW_WED, 365, -5 * 3600, "EST", &tm);

    // Try the last second of the millenium.
    ts = 978307200 - 1;
    memset(&tm, 0xa5, sizeof(tm));
    assert(localtime_rz(tz_melbourne, &ts, &tm) == &tm);
    assert_tm(ts, 2001, 1, 1, 10, 59, 59, 1, DOW_MON, 1, 11 * 3600, "AEDT", &tm);

    memset(&tm, 0xff, sizeof(tm));
    assert(localtime_rz(tz_hong_kong, &ts, &tm) == &tm);
    assert_tm(ts, 2001, 1, 1, 7, 59, 59, 0, DOW_MON, 1, 8 * 3600, "HKT", &tm);

    memset(&tm, 0x1, sizeof(tm));
    assert(localtime_rz(tz_london, &ts, &tm) == &tm);
    assert_tm(ts, 2000, 12, 31, 23, 59, 59, 0, DOW_SUN, 366, 0, "GMT", &tm);

    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_new_york, &ts, &tm) == &tm);
    assert_tm(ts, 2000, 12, 31, 18, 59, 59, 0, DOW_SUN, 366, -5 * 3600, "EST", &tm);

    ts = INT64_C(2171494800);
    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_london, &ts, &tm) == &tm);
    assert_tm(ts, 2038, 10, 24, 2, 0, 0, 1, DOW_SUN, 297, 3600, "BST", &tm);

    ts = INT64_C(13601088000);
    memset(&tm, 0xdd, sizeof(tm));
    assert(localtime_rz(tz_new_york, &ts, &tm) == &tm);
    assert_tm(ts, 2400, 12, 31, 19, 0, 0, 0, DOW_SUN, 366, -5 * 3600, "EST", &tm);
}
