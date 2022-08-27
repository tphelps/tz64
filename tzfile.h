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


struct tz64 {
    uint32_t ts_count;
    uint32_t leap_count;
    const int64_t *timestamps;
    const uint8_t *offset_map;
    const struct tz_offset *offsets;
    const int64_t *leap_ts;
    const int64_t *rev_leap_ts;
    const int32_t *leap_secs;
    const char *desig;
    const char *tz;
};


void tz_header_fix_endian(struct tz_header *header);
size_t tz_header_data_len(const struct tz_header *header, size_t time_size);

static inline int64_t encode_ymdhm(const struct tm *tm)
{
    return (((uint64_t)tm->tm_year) << 32) |
        (((uint32_t)tm->tm_mon) << 24) |
        (((uint32_t)tm->tm_mday) << 16) |
        (((uint32_t)tm->tm_hour) << 8) |
        (uint32_t)tm->tm_min;
}
