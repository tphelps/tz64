#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "tz.h"

static void init_tm(struct tm *tm, int year, int month, int day, int hour, int min, int sec, int isdst)
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

    // Reverse the zero timestamp in each zone.
    time_t ts = 0;
    struct tm tm;
    init_tm(&tm, 1970, 1, 1, 10, 0, 0, -1);
    assert(mktime_z(tz_melbourne, &tm) == ts);
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

    init_tm(&tm, 1970, 1, 1, 8, 0, 0, -1);
    assert(mktime_z(tz_hong_kong, &tm) == ts);
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

    init_tm(&tm, 1970, 1, 1, 1, 0, 0, -1);
    assert(mktime_z(tz_london, &tm) == ts);
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

    init_tm(&tm, 1969, 12, 31, 19, 0, 0, -1);
    assert(mktime_z(tz_new_york, &tm) == ts);
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
    init_tm(&tm, 2001, 1, 1, 10, 59, 59, -1);
    assert(mktime_z(tz_melbourne, &tm) == ts);
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

    init_tm(&tm, 2001, 1, 1, 7, 59, 59, -1);
    assert(mktime_z(tz_hong_kong, &tm) == ts);
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

    init_tm(&tm, 2000, 12, 31, 23, 59, 59, -1);
    assert(mktime_z(tz_london, &tm) == ts);
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

    init_tm(&tm, 2000, 12, 31, 18, 59, 59, -1);
    assert(mktime_z(tz_new_york, &tm) == ts);
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

    // Set up a tm based on seconds.
    ts = 1660912736;
    init_tm(&tm, 1970, 1, 1, 0, 0, 0, -1);
    tm.tm_sec = ts + 10 * 60 * 60;
    assert(mktime_z(tz_melbourne, &tm) == ts);
    assert(tm.tm_sec == 56);
    assert(tm.tm_min == 38);
    assert(tm.tm_hour == 22);
    assert(tm.tm_mday == 19);
    assert(tm.tm_mon == 8 - 1);
    assert(tm.tm_year == 2022 - 1900);
    assert(tm.tm_wday == 5);
    assert(tm.tm_yday == 231 - 1);
    assert(tm.tm_isdst == 0);
    assert(tm.tm_gmtoff == 10 * 60 * 60);
    assert(strcmp(tm.tm_zone, "AEST") == 0);
}
