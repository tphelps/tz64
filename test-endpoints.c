#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "tz.h"
#include "tzfile.h"


static const char *progname;
static bool exhaustive;

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
    "right/America/Adak",
    "right/America/Anchorage",
    "right/America/Anguilla",
    "right/America/Chicago",
    "right/America/Denver",
    "right/America/Los_Angeles",
    "right/America/Mexico_City",
    "right/America/New_York",
    "right/Asia/Hong_Kong",
    "right/Asia/Singapore",
    "right/Asia/Taipei",
    "right/Asia/Tehran",
    "right/Asia/Tokyo",
    "right/Australia/Adelaide",
    "right/Australia/Brisbane",
    "right/Australia/Melbourne",
    "right/Australia/Sydney",
    "right/Europe/Amsterdam",
    "right/Europe/Berlin",
    "right/Europe/London",
    "right/Europe/Moscow",
    "right/Europe/Paris",
    "right/Europe/Zurich",
    NULL
};


static void set_progname(const char *arg0)
{
    const char *p = strrchr(arg0, '/');
    progname = (p != NULL) ? p + 1 : arg0;
}


static void usage()
{
    fprintf(stderr, "usage: %s [-t timezone] [-x]\n", progname);
    fprintf(stderr, "    -t timezone    test only timezone\n");
    fprintf(stderr, "    -x             do exhaustive testing\n");
}


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


static int check_ts(const struct tz64 *tz, time_t ts)
{
    // Convert the ts to a struct tm with localtime_r
    struct tm ref_tm;
    memset(&ref_tm, 0, sizeof(ref_tm));
    assert(localtime_r(&ts, &ref_tm) == &ref_tm);

    // And then with tz64
    struct tm test_tm;
    memset(&test_tm, 0, sizeof(test_tm));
    assert(localtime_rz(tz, &ts, &test_tm) == &test_tm);

    // Make sure they produce the same result.
    assert_tm_eq(&ref_tm, &test_tm);
    int year = test_tm.tm_year + 1900;

    // Convert back to a timestamp.
    time_t ref_ts = mktime(&ref_tm);
    time_t test_ts = mktime_z(tz, &test_tm);

    // See if mktime produces the same result
    if (test_ts == ref_ts) {
        assert_tm_eq(&ref_tm, &test_tm);
        return year;
    }

    // And mktime sometimes cheats.  Try pushing it a day later and
    // back.
    struct tm dup_tm = ref_tm;
    ref_tm.tm_mday++;
    (void)mktime(&ref_tm);
    ref_tm = dup_tm;
    ref_ts = mktime(&ref_tm);

    // It had better match now.
    if (test_ts == ref_ts) {
        putchar(':');
        fflush(stdout);
        assert_tm_eq(&ref_tm, &test_tm);
        return year;
    }

    // Try pushing it back a day and forward again.
    test_tm.tm_mday--;
    (void)mktime(&ref_tm);
    ref_tm = dup_tm;
    ref_ts = mktime(&test_tm);

    // It had better match now.
    if (test_ts == ref_ts) {
        putchar('#');
        fflush(stdout);
        assert_tm_eq(&ref_tm, &test_tm);
        return year;
    }

    printf("%" PRId64 " -> %" PRId64 " (%" PRId64 ")\n", ts, test_ts, ref_ts);
    printf("%04d-%02d-%02d %02d:%02d:%02d\n",
           ref_tm.tm_year + 1900,
           ref_tm.tm_mon + 1,
           ref_tm.tm_mday,
           ref_tm.tm_hour,
           ref_tm.tm_min,
           ref_tm.tm_sec);
    abort();
}


static void check_tz(const char *name)
{
    // Set the time zone.
    setenv("TZ", name, 1);
    tzset();

    // Load it the hard way.
    struct tz64 *tz = tzalloc(name);
    assert(tz != NULL);

    if (exhaustive) {
        printf("%s:", name);
        fflush(stdout);

        int prev = 0;
        const int64_t start = -2208988800;
        const int64_t end = 2145916800;
        for (time_t ts = start; ts <= end; ts++) {
            int year = check_ts(tz, ts);
            if (prev != year) {
                printf(" %d", year);
                fflush(stdout);
                prev = year;
            }
        }
    } else {
        // Loop through the time zone transitions and check the second of
        // and the second before each.
        for (uint32_t i = 1; i < tz->ts_count; i++) {
            int64_t ts = tz->timestamps[i];
            check_ts(tz, ts - 1);
            check_ts(tz, ts);
        }

        // Repeat for leap second transitions.
        for (uint32_t i = 1; i < tz->leap_count; i++) {
            int64_t ts = tz->leap_ts[i];
            check_ts(tz, ts - 1);
            check_ts(tz, ts);
            check_ts(tz, ts + 1);
        }
    }

    tzfree(tz);
}


int main(int argc, char *argv[])
{
    set_progname (argv[0]);

    const char *tz = NULL;
    int choice;
    while ((choice = getopt(argc, argv, "t:x")) != -1) {
        switch (choice) {
        case 't':
            tz = optarg;
            break;

        case 'x':
            exhaustive = true;
            break;

        case '?':
            usage();
            exit(1);

        default:
            abort();
        }
    }

    if (tz != NULL) {
        check_tz(tz);
    } else {
        for (int i = 0; tz_names[i] != NULL; i++) {
            check_tz(tz_names[i]);
        }
    }
}
