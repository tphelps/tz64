#ifndef TZ_H
#define TZ_H

#include <inttypes.h>
#include <time.h>

struct time_zone *tzalloc(const char *path);
void tzfree(struct time_zone *tz);

struct tm *localtime_rz(struct time_zone *restrict tz, time_t const *restrict ts, struct tm *restrict tm);
time_t mktime_z(struct time_zone *tz, struct tm *tm);

#endif // TZ_H
