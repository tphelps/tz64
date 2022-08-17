#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "tz.h"
#include "tzfile.h"

static const char *tz_names[] = {
    "America/Adak",
    "America/Anchorage",
    "America/Anguilla",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Mexico_City",
    "America/New_York",
    "Asia/Hong_Kong",
    "Asia/Singapore",
    "Asia/Taipei",
    "Asia/Tehran",
    "Asia/Tokyo",
    "Australia/Adelaide",
    "Australia/Brisbane",
    "Australia/Melbourne",
    "Australia/Sydney",
    "Europe/Amsterdam",
    "Europe/Berlin",
    "Europe/London",
    "Europe/Moscow",
    "Europe/Paris",
    "Europe/Zurich",
    NULL
};


static void assert_tm_eq(const struct tm *expected, const struct tm *actual)
{
    assert(actual->tm_sec == expected->tm_sec);
    assert(actual->tm_min == expected->tm_min);
    assert(actual->tm_hour == expected->tm_hour);
    assert(actual->tm_mday == expected->tm_mday);
    assert(actual->tm_mon == expected->tm_mon);
    assert(actual->tm_year == expected->tm_year);
    assert(actual->tm_wday == expected->tm_wday);
    assert(actual->tm_yday == expected->tm_yday);
    assert(actual->tm_isdst == expected->tm_isdst);
    assert(actual->tm_gmtoff == expected->tm_gmtoff);
    assert(strcmp(actual->tm_zone, expected->tm_zone) == 0);
}


static void check_ts(const struct time_zone *tz, time_t ts)
{
    struct tm ref_tm;
    memset(&ref_tm, 0, sizeof(ref_tm));
    assert(localtime_r(&ts, &ref_tm) == &ref_tm);

    struct tm test_tm;
    memset(&test_tm, 0, sizeof(test_tm));
    assert(localtime_rz(tz, &ts, &test_tm) == &test_tm);

    assert_tm_eq(&ref_tm, &test_tm);
}


static void check_tz(const char *name)
{
    // Set the time zone.
    setenv("TZ", name, 1);
    tzset();

    // Load it the hard way.
    struct time_zone *tz = tzalloc(name);
    assert(tz != NULL);

    // Loop through the time zone transitions and check the second of
    // and the second before each.
    for (uint32_t i = 1; i < tz->ts_count; i++) {
        int64_t ts = tz->timestamps[i];
        check_ts(tz, ts - 1);
        check_ts(tz, ts);
    }
}


int main(int argc, char *argv[])
{
    for (int i = 0; tz_names[i] != NULL; i++) {
        check_tz(tz_names[i]);
    }
}
