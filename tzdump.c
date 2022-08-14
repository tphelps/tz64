#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "tz.h"
#include "tzfile.h"

const char *progname;
static bool dump_v1_data;

static const char magic[] = "TZif";

static void set_progname(const char *arg0)
{
    const char *p = strrchr(arg0, '/');
    progname = (p != NULL) ? p + 1 : arg0;
}


static void usage()
{
    fprintf(stderr, "usage: %s [-r] [-1] [tzfile]...\n", progname);
    fprintf(stderr, "usage: %s -h\n", progname);
}


static const char *format_utc(time_t t)
{
    static char buffer[32];

    struct tm tm;
    if (gmtime_r(&t, &tm) == NULL) {
        snprintf(buffer, sizeof(buffer), "<out of range>");
    } else {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    }
    return buffer;
}


static const char *format_offset(int32_t offset)
{
    static char buffer[16];

    int32_t abs_offset = (offset < 0) ? -offset : offset;
    snprintf(buffer, sizeof(buffer), "%s%02d:%02d:%02d",
             (offset == 0) ? "": (offset < 0) ? "-" : "+",
             abs_offset / 3600,
             abs_offset / 60 % 60,
             abs_offset % 60);
    return buffer;
}


static bool report_too_short(const char *path)
{
    fprintf(stderr, "%s: error: %s appears to be truncated\n", progname, path);
    return false;
}


static void dump_header(const struct tz_header *header)
{
    printf("-- header --\n");
    printf("magic=%.4s\n", header->magic);
    printf("version=%c\n", (header->version == '\0') ? '1' : header->version);
    printf("isutcnt=%u\n", header->isutcnt);
    printf("isdstcnt=%u\n", header->isstdcnt);
    printf("leapcnt=%u\n", header->leapcnt);
    printf("timecnt=%u\n", header->timecnt);
    printf("typecnt=%u\n", header->typecnt);
    printf("charcnt=%u\n", header->charcnt);
}


static bool dump_ts4(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- timestamps --\n");
    for (uint32_t i = 0; i < count; i++) {
        int32_t ts;
        size_t len = fread(&ts, 1, sizeof(ts), file);
        if (len < sizeof(ts)) {
            return report_too_short(path);
        }

        ts = be32toh(ts);
        printf("%u: %" PRId32 " (%s UTC)\n", i, ts, format_utc(ts));
    }

    return true;
}


static bool dump_ts8(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- timestamps --\n");
    for (uint32_t i = 0; i < count; i++) {
        int64_t ts;
        size_t len = fread(&ts, 1, sizeof(ts), file);
        if (len < sizeof(ts)) {
            return report_too_short(path);
        }

        ts = be64toh(ts);

        printf("%u: %" PRId64 " (%s UTC)\n", i, ts, format_utc(ts));
    }

    return true;
}


static bool dump_ts_map(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- timestamp map --\n");
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v;
        if ((v = fgetc(file)) == EOF) {
            return report_too_short(path);
        }
        printf("%u: %u\n", i, v);
    }

    return true;
}


static bool dump_ttinfo(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }
    
    printf("-- ttinfo --\n");
    for (uint32_t i = 0; i < count; i++) {
        int32_t utoff;
        uint8_t dst, desigidx;
        size_t len;
        if ((len = fread(&utoff, 1, sizeof(utoff), file)) < sizeof(utoff) ||
            (dst = fgetc(file)) == EOF || (desigidx = fgetc(file)) == EOF) {
            return report_too_short(path);
        }

        utoff = be32toh(utoff);
        printf("%u: utoff=%" PRId32 ", %s, desigidx=%u\n", i, utoff, dst ? "DST" : "std", desigidx);
    }

    return true;
}


static bool dump_desig(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- desig --\n");
    bool print_index = true;
    for (uint32_t i = 0; i < count; i++) {
        char ch = fgetc(file);
        if (ch == EOF) {
            return report_too_short(path);
        }

        if (print_index) {
            printf("%u: ", i);
            print_index = false;
        }
    
        putchar((ch == '\0') ? '\n' : ch);
        if (ch == '\0') {
            print_index = true;
        }
    }

    return true;
}


static bool dump_leap4(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- leap --\n");
    for (uint32_t i = 0; i < count; i++) {
        int32_t ts, leaps;
        size_t len;
        if ((len = fread(&ts, 1, sizeof(ts), file)) < sizeof(ts) ||
            (len = fread(&leaps, 1, sizeof(leaps), file)) < sizeof(leaps)) {
            return report_too_short(path);
        }

        ts = be32toh(ts);
        leaps = be32toh(leaps);
        printf("%u: %" PRId32 " (%s): %" PRId32 "\n", i, ts, format_utc(ts), leaps);
    }

    return true;
}


static bool dump_leap8(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- leap --\n");
    for (uint32_t i = 0; i < count; i++) {
        int64_t ts;
        int32_t leaps;
        size_t len;
        if ((len = fread(&ts, 1, sizeof(ts), file)) < sizeof(ts) ||
            (len = fread(&leaps, 1, sizeof(leaps), file)) < sizeof(leaps)) {
            return report_too_short(path);
        }

        ts = be64toh(ts);
        leaps = be32toh(leaps);
        printf("%u: %" PRId64 " (%s): %" PRId32 "\n", i, ts, format_utc(ts), leaps);
    }

    return true;
}


static bool dump_isstd(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- isdst --\n");
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v;
        if ((v = fgetc(file)) == EOF) {
            return report_too_short(path);
        }
        printf("%d: %s\n", i, v ? "std" : "DST");
    }

    return true;
}


static bool dump_isut(const char *path, uint32_t count, FILE *file)
{
    if (count == 0) {
        return true;
    }

    printf("-- isut --\n");
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v;
        if ((v = fgetc(file)) == EOF) {
            return report_too_short(path);
        }

        printf("%u: %s\n", i, v ? "UT" : "local time");
    }

    return true;
}


static bool dump_tz_string(const char *path, FILE *file)
{
    int ch = fgetc(file);
    if (ch == EOF) {
        return report_too_short(path);
    }

    if (ch != '\n') {
        fprintf(stderr, "%s: error: malformed TZ string in %s\n", progname, path);
        return false;
    }

    printf("-- tz string --\n");
    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
        if (ch == '\n') {
            return true;
        }
    }

    return report_too_short(path);
}


static void dump_tzif(const char *path, FILE *file)
{
    // Read the magic.
    struct tz_header header;
    size_t len = fread(&header, 1, sizeof(header.magic), file);
    if (len < 4) {
        fprintf(stderr, "%s: error: %s too short to be a TZif file\n", progname, path);
        return;
    }

    if (memcmp(header.magic, magic, sizeof(header.magic)) != 0) {
        fprintf(stderr, "%s: error: %s is not a TZif file\n", progname, path);
        return;
    }

    // Read rest of the v1 header.
    len = fread(&header.version, 1, sizeof(header) - sizeof(header.magic), file);
    if (len < sizeof(header) - sizeof(header.magic)) {
        report_too_short(path);
        return;
    }

    // Convert the header to host endian.
    tz_header_fix_endian(&header);

    // Maybe print the v1 data.
    if (header.version != '\0' && !dump_v1_data) {
        if (fseek(file, tz_header_data_len(&header, sizeof(int32_t)), SEEK_CUR) < 0) {
            report_too_short(path);
            return;
        }
    } else {
        // Report as much of the file as we can.
        printf("== %s ==\n", path);
        dump_header(&header);
        if (!dump_ts4(path, header.timecnt, file) ||
            !dump_ts_map(path, header.timecnt, file) ||
            !dump_ttinfo(path, header.typecnt, file) ||
            !dump_desig(path, header.charcnt, file) ||
            !dump_leap4(path, header.leapcnt, file) ||
            !dump_isstd(path, header.isstdcnt, file) ||
            !dump_isut(path, header.isutcnt, file)) {
            return;
        }
    }

    if (header.version == '\0') {
        return;
    }

    // Read the v2 header.
    len = fread(&header, 1, sizeof(header), file);
    if (len < sizeof(header)) {
        report_too_short(path);
        return;
    }

    // Check its magic.
    if (memcmp(header.magic, magic, sizeof(header.magic)) != 0) {
        fprintf(stderr, "%s: error: invalid v2 header in %s\n", progname, path);
        return;
    }

    // Convert to host endian.
    tz_header_fix_endian(&header);

    // Print the v2 data.
    if (!dump_v1_data) {
        printf("== %s ==\n", path);
    }

    dump_header(&header);
    if (!dump_ts8(path, header.timecnt, file) ||
        !dump_ts_map(path, header.timecnt, file) ||
        !dump_ttinfo(path, header.typecnt, file) ||
        !dump_desig(path, header.charcnt, file) ||
        !dump_leap8(path, header.leapcnt, file) ||
        !dump_isstd(path, header.isstdcnt, file) ||
        !dump_isut(path, header.isutcnt, file) ||
        !dump_tz_string(path, file)) {
        return;
    }
}


static void dump_raw_file(const char *path)
{
    // Open the file.
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "%s: error: failed to open %s: %s\n", progname, path, strerror(errno));
        return;
    }

    // Dump the contents of the file.
    dump_tzif(path, file);

    if (fclose(file) == EOF) {
        fprintf(stderr, "%s: error: failed to close %s: %s\n", progname, path, strerror(errno));
        return;
    }
}


static void dump_cooked_file(const char *path)
{
    // Load the file.
    struct time_zone *tz = tzalloc(path);
    if (tz == NULL) {
        fprintf(stderr, "%s: error: failed to load TZ file %s: %s\n", progname, path, strerror(errno));
        return;
    }

    // Print out each transition time.
    printf("== %s ==\n", path);
    printf("-- timestamps --\n");
    const struct tz_offset *offset = &tz->offsets[tz->offset_map[0]];
    printf("   :             ( the dawn of time  )/0 (%s %s %s)\n",
           format_offset(offset->utoff),
           tz->desig + offset->desig,
           offset->isdst ? "dst" : "std");

    for (uint32_t i = 1; i < tz->ts_count; i++) {
        offset = &tz->offsets[tz->offset_map[i]];
        printf("%3u: %11" PRId64 " (%s)/%u (%s %s %s)\n",
               i - 1, tz->timestamps[i],
               format_utc(tz->timestamps[i]),
               tz->offset_map[i],
               format_offset(offset->utoff),
               tz->desig + offset->desig,
               offset->isdst ? "dst" : "std");
    }

    printf("-- TZ string --\n");
    printf("%s\n", tz->tz);

    if (tz->leap_count != 0) {
        printf("-- leap seconds --\n");
        for (uint32_t i = 0; i < tz->leap_count; i++) {
            printf("%u: %" PRId64 " (%s): %d\n",
                   i, tz->leaps[i].timestamp,
                   format_utc(tz->leaps[i].timestamp),
                   tz->leaps[i].secs);
        }
    }
    
    tzfree(tz);
}



int main(int argc, char *argv[])
{
    set_progname(argv[0]);

    bool raw_mode = false;
    int choice;
    while ((choice = getopt(argc, argv, "1hr")) != -1) {
        switch (choice) {
        case '1':
            dump_v1_data = true;
            break;

        case 'h':
            usage();
            exit(0);

        case 'r':
            raw_mode = true;
            break;

        case '?':
            usage();
            exit(1);

        default:
            abort();
        }
    }

    while (optind < argc) {
        if (raw_mode) {
            dump_raw_file(argv[optind++]);
        } else {
            dump_cooked_file(argv[optind++]);
        }
    }

    exit(0);
}
