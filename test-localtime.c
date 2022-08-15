#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "tz.h"

int main(int argc, char *argv[])
{
    // Load a few time zones.
    struct time_zone *tz_new_york, *tz_melbourne, *tz_hong_kong, *tz_london;
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
    assert(tm.tm_sec == 0);
    assert(tm.tm_min == 0);
    assert(tm.tm_hour == 10);
    assert(tm.tm_mday == 1);
    assert(tm.tm_mon == 1 - 1);
    assert(tm.tm_year == 1970 - 1900);
    assert(tm.tm_wday == 4);
    assert(tm.tm_yday == 1 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 10 * 60 * 60);
    assert(strcmp(tm.tm_zone, "AEST") == 0);

    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_hong_kong, &ts, &tm) == &tm);
    assert(tm.tm_sec == 0);
    assert(tm.tm_min == 0);
    assert(tm.tm_hour == 8);
    assert(tm.tm_mday == 1);
    assert(tm.tm_mon == 1 - 1);
    assert(tm.tm_year == 1970 - 1900);
    assert(tm.tm_wday == 4);
    assert(tm.tm_yday == 1 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 8 * 60 * 60);
    assert(strcmp(tm.tm_zone, "HKT") == 0);

    memset(&tm, 0, sizeof(tm));
    assert(localtime_rz(tz_london, &ts, &tm) == &tm);
    assert(tm.tm_sec == 0);
    assert(tm.tm_min == 0);
    assert(tm.tm_hour == 1);
    assert(tm.tm_mday == 1);
    assert(tm.tm_mon == 1 - 1);
    assert(tm.tm_year == 1970 - 1900);
    assert(tm.tm_wday == 4);
    assert(tm.tm_yday == 1 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 1 * 60 * 60);
    assert(strcmp(tm.tm_zone, "BST") == 0);

    assert(localtime_rz(tz_new_york, &ts, &tm) == &tm);
    assert(tm.tm_sec == 0);
    assert(tm.tm_min == 0);
    assert(tm.tm_hour == 19);
    assert(tm.tm_mday == 31);
    assert(tm.tm_mon == 12 - 1);
    assert(tm.tm_year == 1969 - 1900);
    assert(tm.tm_wday == 3);
    assert(tm.tm_yday == 365 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == -5 * 60 * 60);
    assert(strcmp(tm.tm_zone, "EST") == 0);

    // Try the last second of the millenium.
    ts = 978307200 - 1;
    assert(localtime_rz(tz_melbourne, &ts, &tm) == &tm);
    assert(tm.tm_sec == 59);
    assert(tm.tm_min == 59);
    assert(tm.tm_hour == 10);
    assert(tm.tm_mday == 1);
    assert(tm.tm_mon == 1 - 1);
    assert(tm.tm_year == 2001 - 1900);
    assert(tm.tm_wday == 1);
    assert(tm.tm_yday == 1 - 1);
    assert(tm.tm_isdst == 1);
    assert(tm.tm_gmtoff == 11 * 60 * 60);
    assert(strcmp(tm.tm_zone, "AEDT") == 0);
    
    assert(localtime_rz(tz_hong_kong, &ts, &tm) == &tm);
    assert(tm.tm_sec == 59);
    assert(tm.tm_min == 59);
    assert(tm.tm_hour == 7);
    assert(tm.tm_mday == 1);
    assert(tm.tm_mon == 1 - 1);
    assert(tm.tm_year == 2001 - 1900);
    assert(tm.tm_wday == 1);
    assert(tm.tm_yday == 1 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 8 * 60 * 60);
    assert(strcmp(tm.tm_zone, "HKT") == 0);

    assert(localtime_rz(tz_london, &ts, &tm) == &tm);
    assert(tm.tm_sec == 59);
    assert(tm.tm_min == 59);
    assert(tm.tm_hour == 23);
    assert(tm.tm_mday == 31);
    assert(tm.tm_mon == 12 - 1);
    assert(tm.tm_year == 2000 - 1900);
    assert(tm.tm_wday == 0);
    assert(tm.tm_yday == 366 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 0);
    assert(strcmp(tm.tm_zone, "GMT") == 0);

    assert(localtime_rz(tz_new_york, &ts, &tm) == &tm);
    assert(tm.tm_sec == 59);
    assert(tm.tm_min == 59);
    assert(tm.tm_hour == 18);
    assert(tm.tm_mday == 31);
    assert(tm.tm_mon == 12 - 1);
    assert(tm.tm_year == 2000 - 1900);
    assert(tm.tm_wday == 0);
    assert(tm.tm_yday == 366 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == -5 * 60 * 60);
    assert(strcmp(tm.tm_zone, "EST") == 0);
}