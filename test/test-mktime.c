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
    struct tz64 *tz_new_york, *tz_ny2, *tz_melbourne, *tz_melb2, *tz_hong_kong, *tz_london;
    tz_new_york = tzalloc("America/New_York");
    assert(tz_new_york != NULL);
    tz_ny2 = tzalloc("EST5EDT,M3.2.0,M11.1.0");
    assert(tz_ny2 != NULL);
    tz_melbourne = tzalloc("Australia/Melbourne");
    assert(tz_melbourne != NULL);
    tz_melb2 = tzalloc("AEST-10AEDT,M10.1.0,M4.1.0/3");
    assert(tz_melb2 != NULL);
    tz_hong_kong = tzalloc("Asia/Hong_Kong");
    assert(tz_hong_kong != NULL);
    tz_london = tzalloc("Europe/London");
    assert(tz_london != NULL);

    // Reverse the zero timestamp in each zone.
    time_t ts = 0;
    struct tm tm;
    init_tm(&tm, 1970, 1, 1, 10, 0, 0, -1);
    assert(mktime_z(tz_melbourne, &tm) == ts);
    assert_tm(ts, 1970, 1, 1, 10, 0, 0, 0, DOW_THU, 1, 10 * 3600, "AEST", &tm);

    init_tm(&tm, 1970, 1, 1, 11, 0, 0, -1);
    assert(mktime_z(tz_melb2, &tm) == ts);
    assert_tm(ts, 1970, 1, 1, 11, 0, 0, 1, DOW_THU, 1, 11 * 3600, "AEDT", &tm);

    init_tm(&tm, 1970, 1, 1, 8, 0, 0, -1);
    assert(mktime_z(tz_hong_kong, &tm) == ts);
    assert_tm(ts, 1970, 1, 1, 8, 0, 0, 0, DOW_THU, 1, 8 * 3600, "HKT", &tm);

    init_tm(&tm, 1970, 1, 1, 1, 0, 0, -1);
    assert(mktime_z(tz_london, &tm) == ts);
    assert_tm(ts, 1970, 1, 1, 1, 0, 0, 0, DOW_THU, 1, 3600, "BST", &tm);

    init_tm(&tm, 1969, 12, 31, 19, 0, 0, -1);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 1969, 12, 31, 19, 0, 0, 0, DOW_WED, 365, -5 * 3600, "EST", &tm);

    init_tm(&tm, 1969, 12, 31, 19, 0, 0, -1);
    assert(mktime_z(tz_ny2, &tm) == ts);
    assert_tm(ts, 1969, 12, 31, 19, 0, 0, 0, DOW_WED, 365, -5 * 3600, "EST", &tm);

    // Try the last second of the millenium.
    ts = 978307200 - 1;
    init_tm(&tm, 2001, 1, 1, 10, 59, 59, -1);
    assert(mktime_z(tz_melbourne, &tm) == ts);
    assert_tm(ts, 2001, 1, 1, 10, 59, 59, 1, DOW_MON, 1, 11 * 3600, "AEDT", &tm);

    init_tm(&tm, 2001, 1, 1, 10, 59, 59, -1);
    assert(mktime_z(tz_melb2, &tm) == ts);
    assert_tm(ts, 2001, 1, 1, 10, 59, 59, 1, DOW_MON, 1, 11 * 3600, "AEDT", &tm);


    init_tm(&tm, 2001, 1, 1, 7, 59, 59, -1);
    assert(mktime_z(tz_hong_kong, &tm) == ts);
    assert_tm(ts, 2001, 1, 1, 7, 59, 59, 0, DOW_MON, 1, 8 * 3600, "HKT", &tm);

    init_tm(&tm, 2000, 12, 31, 23, 59, 59, -1);
    assert(mktime_z(tz_london, &tm) == ts);
    assert_tm(ts, 2000, 12, 31, 23, 59, 59, 0, DOW_SUN, 366, 0, "GMT", &tm);

    init_tm(&tm, 2000, 12, 31, 18, 59, 59, -1);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 2000, 12, 31, 18, 59, 59, 0, DOW_SUN, 366, -5 * 3600, "EST", &tm);

    init_tm(&tm, 2000, 12, 31, 18, 59, 59, -1);
    assert(mktime_z(tz_ny2, &tm) == ts);
    assert_tm(ts, 2000, 12, 31, 18, 59, 59, 0, DOW_SUN, 366, -5 * 3600, "EST", &tm);

    // Try an ambiguous time.  New York fell back on 2012-11-04 at one
    // second after 1:59:59, so let's try 1:30:00 when it was DST
    ts = 1352008800 - 1800;
    init_tm(&tm, 2012, 11, 4, 1, 30, 0, 1);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 2012, 11, 4, 1, 30, 0, 1, DOW_SUN, 309, -4 * 3600, "EDT", &tm);

    init_tm(&tm, 2012, 11, 4, 1, 30, 0, 1);
    assert(mktime_z(tz_ny2, &tm) == ts);
    assert_tm(ts, 2012, 11, 4, 1, 30, 0, 1, DOW_SUN, 309, -4 * 3600, "EDT", &tm);

    // Repeat with standard time.
    ts = 1352008800 + 1800;
    init_tm(&tm, 2012, 11, 4, 1, 30, 0, 0);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 2012, 11, 4, 1, 30, 0, 0, DOW_SUN, 309, -5 * 3600, "EST", &tm);

    init_tm(&tm, 2012, 11, 4, 1, 30, 0, 0);
    assert(mktime_z(tz_ny2, &tm) == ts);
    assert_tm(ts, 2012, 11, 4, 1, 30, 0, 0, DOW_SUN, 309, -5 * 3600, "EST", &tm);

    // Try a non-existent time.  New York sprang forward on 2012-03-11
    // at 1 second after 01:59:59.  Try 02:30:00.  An hour after
    // 01:30:00 EST would be 03:30:00 EDT.
    ts = 1331449200 + 1800;
    init_tm(&tm, 2012, 3, 11, 2, 30, 0, 0);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 2012, 3, 11, 3, 30, 0, 1, DOW_SUN, 71, -4 * 3600, "EDT", &tm);

    // Repeat but with the DST indicator indicating an hour before
    // 03:30:00 EDT.
    ts = 1331449200 - 1800;
    init_tm(&tm, 2012, 3, 11, 2, 30, 0, 1);
    assert(mktime_z(tz_new_york, &tm) == ts);
    assert_tm(ts, 2012, 3, 11, 1, 30, 0, 0, DOW_SUN, 71, -5 * 3600, "EST", &tm);

    // Set up a tm based on seconds.
    ts = 1660912736;
    init_tm(&tm, 1970, 1, 1, 0, 0, 0, -1);
    tm.tm_sec = ts + 10 * 60 * 60;
    assert(mktime_z(tz_melbourne, &tm) == ts);
    assert_tm(ts, 2022, 8, 19, 22, 38, 56, 0, DOW_FRI, 231, 10 * 3600, "AEST", &tm);

    init_tm(&tm, 1970, 1, 1, 0, 0, 0, -1);
    tm.tm_sec = ts + 10 * 60 * 60;
    assert(mktime_z(tz_melb2, &tm) == ts);
    assert_tm(ts, 2022, 8, 19, 22, 38, 56, 0, DOW_FRI, 231, 10 * 3600, "AEST", &tm);
}
