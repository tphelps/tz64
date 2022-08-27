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

// Time Zone information file reader

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <endian.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "tz64.h"
#include "tz64file.h"

#define MAGIC "TZif"
#define ZONE_DIR "/usr/share/zoneinfo"

extern const char *progname;
static const size_t max_tz_str_size = 64;

static const int64_t utc_timestamps[1] = { INT64_MIN };
static const struct tz_offset utc_offsets[1] = { { 0, 0, 0 } };
static const uint8_t utc_offset_map[1] = { 0 };

struct tz64 tz_utc = {
    .ts_count = 1,
    .leap_count = 0,
    .timestamps = utc_timestamps,
    .offsets = utc_offsets,
    .offset_map = utc_offset_map,
    .leap_ts = NULL,
    .rev_leap_ts = NULL,
    .leap_secs = NULL,
    .desig = "UTC",
    .tz = "UTC0"
};


size_t tz_header_data_len(const struct tz_header *header, size_t time_size)
{
    return
        header->timecnt * time_size +
        header->timecnt +
        header->typecnt * (sizeof(int32_t) + 1 + 1) +
        header->charcnt +
        header->leapcnt * (time_size + sizeof(int32_t)) +
        header->isstdcnt +
        header->isutcnt;
}

void tz_header_fix_endian(struct tz_header *header)
{
    header->isutcnt = be32toh(header->isutcnt);
    header->isstdcnt = be32toh(header->isstdcnt);
    header->leapcnt = be32toh(header->leapcnt);
    header->timecnt = be32toh(header->timecnt);
    header->typecnt = be32toh(header->typecnt);
    header->charcnt = be32toh(header->charcnt);
}


static struct tz64 *process_tzfile(const char *path, const char *data, off_t size)
{
    const char *end = data + size;

    if (data + sizeof(struct tz_header) >= end) {
        errno = EINVAL;
        return NULL;
    }

    // Bail if the magic is wrong.
    if (memcmp(data, MAGIC, strlen(MAGIC)) != 0) {
        errno = EINVAL;
        return NULL;
    }


    // Bail if the version isn't supported.
    char version = data[4];
    if (version < '2' || version > '9') {
        errno = EINVAL;
        return NULL;
    }

    // Make a copy of the header and adjust the byte order.
    struct tz_header header;
    memcpy(&header, data, sizeof(header));
    data += sizeof(struct tz_header);
    tz_header_fix_endian(&header);

    // Make sure the v1 data is present and skip it.
    const size_t v1_size = tz_header_data_len(&header, sizeof(int32_t));
    if (end - data < v1_size) {
        errno = EINVAL;
        return NULL;
    }
    data += v1_size;

    // Make sure there's a v2 header.
    if (end - data < sizeof(struct tz_header)) {
        errno = EINVAL;
        return NULL;
    }

    // Make a copy of the v2 header and adjust the byte order.
    memcpy(&header, data, sizeof(struct tz_header));
    data += sizeof(header);
    tz_header_fix_endian(&header);

    // Check the second header's magic.
    if (memcmp(header.magic, MAGIC, strlen(MAGIC)) != 0) {
        errno = EINVAL;
        return NULL;
    }

    // Make sure the file is big enough for the v2 data.
    size_t v2_size = tz_header_data_len(&header, sizeof(int64_t));
    if (end - data < v2_size) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate memory for the v2 data.
    size_t block_size =
        sizeof(struct tz64) +
        (header.timecnt + 1) * sizeof(int64_t) +
        header.typecnt * sizeof(struct tz_offset) +
        (header.leapcnt + 1) * sizeof(int64_t) +
        (header.leapcnt + 1) * sizeof(int64_t) +
        header.leapcnt * sizeof(int32_t) +
        header.timecnt + 1 +
        header.charcnt +
        max_tz_str_size;
    char *block = malloc(block_size);
    if (block == NULL) {
        return NULL;
    }

    struct tz64 *tz = (struct tz64 *)block;
    block += sizeof(struct tz64);

    // Set up pointers to the various fields.
    int64_t *timestamps = (int64_t *)block;
    block += (header.timecnt + 1) * sizeof(int64_t);
    struct tz_offset *offsets = (struct tz_offset *)block;
    block += header.typecnt * sizeof(struct tz_offset);
    int64_t *leap_ts = NULL;
    int64_t *rev_leap_ts = NULL;
    int32_t *leap_secs = NULL;
    if (header.leapcnt != 0) {
        leap_ts = (int64_t *)block;
        block += (header.leapcnt + 1) * sizeof(int64_t);
        rev_leap_ts = (int64_t *)block;
        block += (header.leapcnt + 1) * sizeof(int64_t);
        leap_secs = (int32_t *)block;
        block += (header.leapcnt + 1) * sizeof(int32_t);
    }
    uint8_t *offset_map = (uint8_t *)block;
    block += header.timecnt + 1;
    char *desig = block;
    block += header.charcnt;
    char *tz_str = block;

    // Read in the timestamps.
    timestamps[0] = INT64_MIN;
    for (uint32_t i = 0; i < header.timecnt; i++) {
        int64_t ts;
        memcpy(&ts, data, sizeof(ts));
        data += sizeof(ts);
        ts = be64toh(ts);

        if (ts <= timestamps[i]) {
            errno = EINVAL;
            goto err;
        }
        timestamps[i + 1] = ts;
    }

    // Read the timestamp/offset map.
    offset_map[0] = 0;
    memcpy(offset_map + 1, data, header.timecnt);
    for (uint32_t i = 0; i < header.timecnt; i++) {
        if (offset_map[i] > header.typecnt) {
            errno = EINVAL;
            goto err;
        }
    }
    data += header.timecnt;

    // Read the time types.
    for (uint32_t i = 0; i < header.typecnt; i++) {
        int32_t utoff;
        memcpy(&utoff, data, sizeof(utoff));
        data += sizeof(utoff);
        utoff = be32toh(utoff);

        offsets[i].utoff = utoff;
        offsets[i].isdst = *data++;
        offsets[i].desig = *data++;

        if (offsets[i].desig >= header.charcnt) {
            errno = EINVAL;
            goto err;
        }
    }

    // Read the designations.
    memcpy(desig, data, header.charcnt);
    if (desig[header.charcnt -1] != '\0') {
        errno = EINVAL;
        goto err;
    }
    data += header.charcnt;
    
    // Read the leap second information.
    if (header.leapcnt != 0) {
        leap_ts[0] = INT64_MIN;
        rev_leap_ts[0] = INT64_MIN;
        leap_secs[0] = 0;
        for (uint32_t i = 0; i < header.leapcnt; i++) {
            int64_t ts;
            memcpy(&ts, data, sizeof(ts));
            data += sizeof(ts);
            ts = be64toh(ts);

            if (ts <= leap_ts[i]) {
                errno = EINVAL;
                goto err;
            }
            leap_ts[i + 1] = ts;

            int32_t secs;
            memcpy(&secs, data, sizeof(secs));
            data += sizeof(secs);
            secs = be32toh(secs);

            if (secs <= leap_secs[i]) {
                errno = EINVAL;
                goto err;
            }
            leap_secs[i + 1] = secs;
        }
    }

    // Skip the standard/wall indicators.
    data += header.isstdcnt;

    // Skip the UT/local indicators.
    data += header.isutcnt;

    // Read the footer.
    if (data == end) {
        tz->tz = NULL;
    } else {
        if (*data++ != '\n') {
            errno = EINVAL;
            goto err;
        }

        // Find a newline.
        size_t s = (end - data) < max_tz_str_size ? (end - data) : max_tz_str_size;
        const char *nl = memchr(data, '\n', s);
        if (nl == NULL) {
            errno = EINVAL;
            goto err;
        }

        memcpy(tz_str, data, nl - data);
        tz_str[nl - data] = '\0';
    }

    // Populate the tz itself, except for the reverse leap seconds.
    tz->ts_count = header.timecnt + 1;
    tz->leap_count = (header.leapcnt == 0) ? 0 : (header.leapcnt + 1);
    tz->timestamps = timestamps;
    tz->offset_map = offset_map;
    tz->leap_ts = leap_ts;
    tz->rev_leap_ts = NULL;
    tz->leap_secs = leap_secs;
    tz->offsets = offsets;
    tz->desig = desig;
    tz->tz = tz_str;

    // Compute local time for each leap second.
    for (uint32_t i = 0; i < header.leapcnt; i++) {
        time_t ts = tz->leap_ts[i + 1] + 1;

        struct tm tm;
        if (localtime_rz(tz, &ts, &tm) == NULL) {
            errno = EINVAL;
            goto err;
        }

        rev_leap_ts[i + 1] = encode_ymdhm(&tm);
    }

    tz->rev_leap_ts = rev_leap_ts;
    return tz;

err:
    free(tz);
    return NULL;
}


static int load_tz(struct tz64 **tz_out, const char *path)
{
    // Open the file.
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return ENOENT;
    }

    // Figure out how big it is.
    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0) {
        int err = errno;
        close(fd);
        return err;
    }

    // Map it into memory.
    char *data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == NULL) {
        int err = errno;
        (void)close(fd);
        return err;
    }

    // Decode the data in the file.
    *tz_out = process_tzfile(path, data, statbuf.st_size);

    // Clean up.
    int res = munmap(data, statbuf.st_size);
    assert(res == 0);
    
    res = close(fd);
    assert(res == 0);

    return 0;
}


static struct tz64 *make_utc()
{
    struct tz64 *tz = malloc(sizeof(struct tz64));
    if (tz == NULL) {
        return tz;
    }

    memcpy(tz, &tz_utc, sizeof(struct tz64));
    return tz;
}


static char *mkpath(char *buffer, size_t buflen, const char *rest)
{
    size_t baselen = strlen(ZONE_DIR);
    size_t restlen = strlen(rest);
    size_t len = baselen + 1 + restlen + 1;
    if (len > buflen) {
        return NULL;
    }

    char *p = buffer;
    memcpy(p, ZONE_DIR, baselen);
    p += baselen;
    *p++ = '/';
    memcpy(p, rest, restlen);
    p += restlen;
    *p++ = '\0';

    return buffer;
}


struct tz64 *tzalloc(const char *tz_desc)
{
    char pathbuf[256];
    struct tz64 *tz;

    // NULL indicates localtime.
    if (tz_desc == NULL) {
        // Try /usr/share/zoneinfo/localtime
        int res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), "localtime"));
        if (res == 0) {
            return tz;
        }

        // Failing that, try /etc/localtime
        res = load_tz(&tz, "/etc/localtime");
        if (res == 0) {
            return tz;
        }
    }

    // An empty string means UTC (if no localtime then use UTC).
    if (tz_desc == NULL || *tz_desc == '\0') {
        // Then try UTC.
        int res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), "UTC"));
        if (res == 0) {
            return tz;
        }

        // Create UTC.
        return make_utc();
    }

    // If the description begins with a colon then treat it as a path.
    if (*tz_desc == ':') {
        const char *path = tz_desc + 1;
        if (*path == '/') {
            errno = load_tz(&tz, path);
        } else {
            errno = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), path));
        }
        return tz;
    }

    // No colon, but try reading as a path anyway.
    int res;
    if (*tz_desc == '/') {
        res = load_tz(&tz, tz_desc);
    } else {
        res = load_tz(&tz, mkpath(pathbuf, sizeof(pathbuf), tz_desc));
    }
    if (res == 0) {
        return tz;
    }

    // FIXME: Otherwise treat it as a POSIX TZ string.
    return NULL;
}


void tzfree(struct tz64 *tz)
{
    free(tz);
}
