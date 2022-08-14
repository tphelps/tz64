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
#include "tzfile.h"

extern const char *progname;


size_t tz_header_data_len(const struct tz_header *header, size_t time_size)
{
    return
        header->timecnt * time_size +
        header->timecnt +
        header->typecnt * (4 + 1 + 1) +
        header->charcnt +
        header->leapcnt * (time_size + time_size) +
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
        fprintf(stderr, "%s: error: %s too short to be a time zone file\n", progname, path);
        exit(1);
    }

    // Bail if the magic is wrong.
    if (memcmp(data, "TZif", 4) != 0) {
        fprintf(stderr, "%s: error: %s is not a time zone file: bad magic\n", progname, path);
        exit(1);
    }
    printf("magic: %.4s\n", data);

    // Bail i fthe version isn't supported.
    char version = data[4];
    if (version == '\0') {
        version = 1;
    }
    if (version < '!') {
        fprintf(stderr, "%s: error: invalid version %d (%c)\n",
                progname, version, (version < ' ' || version > '~') ? '.' : version);
        exit(1);
    }
    printf("version: %c (%s)\n", version, (version < '1' || version > '4') ? "unsupported" : "ok");

    // Make a copy of the header and adjust the byte order.
    struct tz_header header;
    memcpy(&header, data, sizeof(header));
    data += sizeof(struct tz_header);
    tz_header_fix_endian(&header);

    // Make sure the v1 data is present.
    if (end - data < tz_header_data_len(&header, sizeof(int32_t))) {
        fprintf(stderr, "%s: error: time zone file %s appears to be truncated\n", progname, path);
        exit(1);
    }
    
    if (version == '\0') {
        printf("%u transition times\n", header.timecnt);

        const char *p = data + header.timecnt * sizeof(int32_t);
        for (uint32_t i = 0; i < header.timecnt; i++) {
            int32_t ts;
            memcpy(&ts, data, sizeof(ts));
            data += sizeof(ts);

            printf("%d: %d/%d\n", i, be32toh(ts), *p++);
        }
        data = p;

        printf("%u time types\n", header.typecnt);
        p = data + header.typecnt * 6;
        for (uint32_t i = 0; i < header.typecnt; i++) {
            int32_t utoff;
            memcpy(&utoff, data, sizeof(utoff));
            data += 4;
            printf("%d: utoff %d, %s, idx=%s\n",
                   i, be32toh(utoff), data[0] ? "DST" : "std", p + data[1]);
            data += 2;
        }

        data = p + header.charcnt;

        printf("Leap-Second Records\n");

        for (uint32_t i = 0; i < header.leapcnt; i++) {
            uint32_t occur, corr;
            memcpy(&occur, data, sizeof(occur));
            data += sizeof(occur);
            memcpy(&corr, data, sizeof(corr));
            data += sizeof(corr);

            printf("%d: %d/%d\n", i, be32toh(occur), be32toh(corr));
        }

        printf("Standard/Wall Indicators\n");
        for (uint32_t i = 0; i < header.isstdcnt; i++) {
            printf("%d: %s\n", i, *data++ ? "std" : "wall");
        }

        printf("UT/Local Indicators\n");
        for (uint32_t i = 0; i < header.isutcnt; i++) {
            printf("%d: %s\n", i, *data++ ? "ut" : "local");
        }

        return NULL;
    }

    // Skip the v1 header data.
    data += tz_header_data_len(&header, sizeof(int32_t));

    // Make sure there's a v2 header.
    if (end - data < sizeof(struct tz_header)) {
        fprintf(stderr, "%s: error: time zone file %s appears to be truncated\n", progname, path);
        exit(1);
    }

    // Make a copy of the v2 header and adjust the byte order.
    memcpy(&header, data, sizeof(struct tz_header));
    data += sizeof(header);
    tz_header_fix_endian(&header);

    // Make sure the v2 data is present.
    if (end - data < tz_header_data_len(&header, sizeof(int64_t))) {
        fprintf(stderr, "%s: error: time zone file %s appears to be truncated\n", progname, path);
        exit(1);
    }

    // Print the v2 data.
    printf("%u transition times/types\n", header.timecnt);
    

    if (header.timecnt != 0) {
        printf("%u transition times\n", header.timecnt);
    }

    const char *p = data + header.timecnt * sizeof(int64_t);
    for (uint32_t i = 0; i < header.timecnt; i++) {
        int64_t ts;
        memcpy(&ts, data, sizeof(ts));
        data += sizeof(ts);

        printf("%d: %" PRId64 "/%d\n", i, be64toh(ts), *p++);
    }
    data = p;

    printf("%u time types\n", header.typecnt);
    p = data + header.typecnt * 6;
    for (uint32_t i = 0; i < header.typecnt; i++) {
        int32_t utoff;
        memcpy(&utoff, data, sizeof(utoff));
        data += 4;
        printf("%d: utoff %d, %s, idx=%s\n",
               i, be32toh(utoff), data[0] ? "DST" : "std", p + data[1]);
        data += 2;
    }
    data = p + header.charcnt;

    printf("Leap-Second Records\n");
    for (uint32_t i = 0; i < header.leapcnt; i++) {
        uint64_t occur;
        uint32_t corr;

        memcpy(&occur, data, sizeof(occur));
        data += sizeof(occur);
        memcpy(&corr, data, sizeof(corr));
        data += sizeof(corr);

        printf("%d: %" PRId64 "/%d\n", i, be64toh(occur), be32toh(corr));
    }

    printf("Standard/Wall Indicators\n");
    for (uint32_t i = 0; i < header.isstdcnt; i++) {
        printf("%d: %s\n", i, *data++ ? "std" : "wall");
    }

    printf("UT/Local Indicators\n");
    for (uint32_t i = 0; i < header.isutcnt; i++) {
        printf("%d: %s\n", i, *data++ ? "ut" : "local");
    }

    // Read the footer.
    size_t len = end - data;
    if (len != 0) {
        assert(len >= 2);
        printf("TZ: %.*s\n", (int)(len - 2), data + 1);
    }

    return NULL;
}


struct time_zone *tzalloc(const char *path)
{
    // Open the file.
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "%s: error: failed to open time zone file %s: %s\n",
                progname, path, strerror(errno));
        exit(1);
    }

    // Figure out how big it is.
    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0) {
        fprintf(stderr, "%s: error: failed to determine size of time zone file %s: %s\n",
                progname, path, strerror(errno));
        exit(1);
    }

    // Map it into memory.
    char *data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == NULL) {
        fprintf(stderr, "%s: error: failed to mmap time zone file %s: %s\n",
                progname, path, strerror(errno));
        exit(1);
    }

    // Decode the data in the file.
    struct time_zone *tz = process_tzfile(path, data, statbuf.st_size);

    // Clean up.
    if (munmap(data, statbuf.st_size) != 0) {
        fprintf(stderr, "%s: error: failed to munmap of time zone file %s: %s\n",
                progname, path, strerror(errno));
        exit(1);
    }
    
    if (close(fd) != 0) {
        fprintf(stderr, "%s: error: failed to close time zone file %s: %s\n",
                progname, path, strerror(errno));
        exit(1);
    }

    return tz;
}


