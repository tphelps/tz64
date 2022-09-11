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
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <tz64.h>
#include <tz64file.h>
#include "utils.h"


static const char *progname;
static bool exhaustive;
static int64_t begin_ts = -2208988800;
static int64_t end_ts = 16725189600 + 86400;

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
    fprintf(stderr, "    -b begin ts    start exhaustive testing from ts\n");
    fprintf(stderr, "    -b end ts      end exhaustive testing at ts\n");
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
    assert_tm_eq(ts, &ref_tm, &test_tm);
    int year = test_tm.tm_year + 1900;

    // Convert back to a timestamp.
    time_t ref_ts = mktime(&ref_tm);
    time_t test_ts = mktime_z(tz, &test_tm);

    // See if mktime produces the same result
    if (test_ts == ref_ts) {
        assert_tm_eq(ts, &ref_tm, &test_tm);
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
        assert_tm_eq(ts, &ref_tm, &test_tm);
        return year;
    }

    // Try pushing it back a day and forward again.
    test_tm.tm_mday--;
    (void)mktime(&ref_tm);
    ref_tm = dup_tm;
    ref_ts = mktime(&ref_tm);

    // It had better match now.
    if (test_ts == ref_ts) {
        putchar('#');
        fflush(stdout);
        assert_tm_eq(ts, &ref_tm, &test_tm);
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
        for (time_t ts = begin_ts; ts <= end_ts; ts++) {
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
    char *p;
    int choice;
    while ((choice = getopt(argc, argv, "b:e:t:x")) != -1) {
        switch (choice) {
        case 'b':
            begin_ts = strtoll(optarg, &p, 0);
            if (p == optarg || *p != '\0') {
                fprintf(stderr, "%s: error: failed to decode begin timestamp: %s\n", progname, optarg);
                exit(1);
            }
            break;

        case 'e':
            end_ts = strtoll(optarg, &p, 0);
            if (p == optarg || *p != '\0') {
                fprintf(stderr, "%s: error: failed to decode end timestamp: %s\n", progname, optarg);
                exit(1);
            }
            break;

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
