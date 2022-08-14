#include <inttypes.h>

struct time_zone *tzalloc(const char *path);
void tzfree(struct time_zone *tz);
