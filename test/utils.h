#include <stddef.h>
#include <time.h>
#include <string.h>
#include <assert.h>

void report_tm_neq(int64_t ts, const struct tm *expected, const struct tm *actual);

static inline void assert_tm_eq(int64_t ts, const struct tm *expected, const struct tm *actual)
{
    if (expected->tm_sec != actual->tm_sec ||
        expected->tm_min != actual->tm_min ||
        expected->tm_hour != actual->tm_hour ||
        expected->tm_mday != actual->tm_mday ||
        expected->tm_mon != actual->tm_mon ||
        expected->tm_year != actual->tm_year ||
        expected->tm_wday != actual->tm_wday ||
        expected->tm_yday != actual->tm_yday ||
        expected->tm_isdst != actual->tm_isdst ||
        expected->tm_gmtoff != actual->tm_gmtoff ||
        strcmp(actual->tm_zone, expected->tm_zone) != 0) {
        report_tm_neq(ts, expected, actual);
    }
}
