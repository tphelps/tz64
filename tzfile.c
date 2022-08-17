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
#include "tz.h"
#include "tzfile.h"

#define MAGIC "TZif"
#define ZONE_DIR "/usr/share/zoneinfo"

extern const char *progname;
static const size_t max_tz_str_size = 64;

static const int64_t utc_timestamps[1] = { INT64_MIN };
static const struct tz_offset utc_offsets[1] = { { 0, 0, 0 } };
static const uint8_t utc_offset_map[1] = { 0 };

struct time_zone tz_utc = {
    .ts_count = 1,
    .leap_count = 0,
    .timestamps = utc_timestamps,
    .offsets = utc_offsets,
    .offset_map = utc_offset_map,
    .leaps = NULL,
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


static struct time_zone *process_tzfile(const char *path, const char *data, off_t size)
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
        sizeof(struct time_zone) +
        (header.timecnt + 1) * sizeof(int64_t) +
        header.typecnt * sizeof(struct tz_offset) +
        header.leapcnt * sizeof(struct tz_leap) +
        header.timecnt + 1 +
        header.charcnt +
        max_tz_str_size;
    char *block = malloc(block_size);
    if (block == NULL) {
        return NULL;
    }

    struct time_zone *tz = (struct time_zone *)block;
    block += sizeof(struct time_zone);

    // Set up pointers to the various fields.
    int64_t *timestamps = (int64_t *)block;
    block += (header.timecnt + 1) * sizeof(int64_t);
    struct tz_offset *offsets = (struct tz_offset *)block;
    block += header.typecnt * sizeof(struct tz_offset);
    struct tz_leap *leaps = (struct tz_leap *)block;
    block += header.leapcnt * sizeof(struct tz_leap);
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
    for (uint32_t i = 0; i < header.leapcnt; i++) {
        int64_t ts;
        memcpy(&ts, data, sizeof(ts));
        data += sizeof(ts);
        ts = be64toh(ts);

        int32_t secs;
        memcpy(&secs, data, sizeof(secs));
        data += sizeof(secs);
        secs = be32toh(secs);

        leaps[i].timestamp = ts;
        leaps[i].secs = secs;
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

    // Populate and the tz itself.
    tz->ts_count = header.timecnt + 1;
    tz->leap_count = header.leapcnt;
    tz->timestamps = timestamps;
    tz->offset_map = offset_map;
    tz->leaps = leaps;
    tz->offsets = offsets;
    tz->desig = desig;
    tz->tz = tz_str;
    return tz;

err:
    free(tz);
    return NULL;
}


static int load_tz(struct time_zone **tz_out, const char *path)
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


static struct time_zone *make_utc()
{
    struct time_zone *tz = malloc(sizeof(struct time_zone));
    if (tz == NULL) {
        return tz;
    }

    memcpy(tz, &tz_utc, sizeof(struct time_zone));
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


struct time_zone *tzalloc(const char *tz_desc)
{
    char pathbuf[256];
    struct time_zone *tz;

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


void tzfree(struct time_zone *tz)
{
    free(tz);
}
