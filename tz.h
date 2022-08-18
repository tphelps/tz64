#ifndef TZ_H
#define TZ_H

#include <inttypes.h>
#include <time.h>

struct tz64 *tzalloc(const char *path);
void tzfree(struct tz64 *tz);

struct tm *localtime_rz(const struct tz64 *restrict tz, time_t const *restrict ts, struct tm *restrict tm);
time_t mktime_z(const struct tz64 *tz, struct tm *tm);

#endif // TZ_H
