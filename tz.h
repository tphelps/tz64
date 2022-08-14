#include <inttypes.h>

struct tz_offset {
    uint32_t isdst:1;
    uint32_t desig:8;
    uint32_t utoff:23;
};


struct time_zone {
    uint32_t count;
    uint32_t current;
    uint64_t *timestamps;
    uint64_t *datestamps;
    struct tz_offset *offsets;
};


struct time_zone *tzalloc(const char *path);
void tzfree(struct time_zone *tz);
