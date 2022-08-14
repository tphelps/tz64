#include <inttypes.h>

// The format of a TZif file header.
struct tz_header {
    char magic[4];
    char version;
    char _reserved[15];
    uint32_t isutcnt;
    uint32_t isstdcnt;
    uint32_t leapcnt;
    uint32_t timecnt;
    uint32_t typecnt;
    uint32_t charcnt;
};


struct tz_offset {
    int32_t utoff:23;
    uint32_t isdst:1;
    uint32_t desig:8;
};


struct time_zone {
    uint32_t ts_count;
    const int64_t *timestamps;
    const uint8_t *offset_map;
    const struct tz_offset *offsets;
    const char *desig;
    const char *tz;
};



void tz_header_fix_endian(struct tz_header *header);
size_t tz_header_data_len(const struct tz_header *header, size_t time_size);

