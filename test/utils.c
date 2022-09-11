#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>
#include "utils.h"

static char *format_tm(char *buffer, size_t buflen, const struct tm *tm)
{
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%a %Y-%m-%d %H:%M:%S %M", tm);
    snprintf(buffer, buflen, "%s yday=%d, dst=%d, offset=%ld", timebuf, tm->tm_yday + 1, tm->tm_isdst, tm->tm_gmtoff);
    return buffer;
}


void report_tm_neq(int64_t ts, const struct tm *expected, const struct tm *actual)
{
    char buffer[128];
    fprintf(stderr, "error: broken-down times do not match at time %" PRId64 ":\n", ts);
    fprintf(stderr, "expected: %s\n", format_tm(buffer, sizeof(buffer), expected));
    fprintf(stderr, "  actual: %s\n", format_tm(buffer, sizeof(buffer), actual));
    abort();
}
