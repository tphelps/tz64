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
