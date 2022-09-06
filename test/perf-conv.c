#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <tz64.h>

enum mode {
    MODE_LOCALTIME_R,
    MODE_GMTIME_R,
    MODE_LOCALTIME_RZ
};

static const char *progname;

static void set_progname(const char *arg0)
{
    const char *p = strrchr(arg0, '/');
    progname = (p != NULL) ? p + 1 : arg0;
}


static void usage()
{
    fprintf(stderr, "usage: %s [-t tz] [-n cycles]\n", progname);
}


int main(int argc, char *argv[])
{
    const char *tz_name = NULL;
    unsigned long cycles = 100e6;
    enum mode mode = MODE_LOCALTIME_RZ;

    set_progname(argv[0]);

    char *p;
    int choice;
    while ((choice = getopt(argc, argv, "ct:n:uz")) != -1) {
        switch (choice) {
        case 'c':
            mode = MODE_LOCALTIME_R;
            break;

        case 't':
            tz_name = optarg;
            break;

        case 'n':
            cycles = strtoul(optarg, &p, 0);
            if (p == optarg || *p != '\0') {
                fprintf(stderr, "%s: error: failed to parse %s as an integer\n",
                        progname, optarg);
                exit(1);
            }
            break;

        case 'u':
            mode = MODE_GMTIME_R;
            break;

        case 'z':
            mode = MODE_LOCALTIME_RZ;
            break;

        case '?':
            usage();
            exit(0);

        default:
            abort();
        }
    }

    // Load the time zone.
    struct tz64 *tz = tzalloc(tz_name);
    if (tz == NULL) {
        fprintf(stderr, "%s: error: failed to load time zone %s: %s\n", progname, tz_name, strerror(errno));
        exit(1);
    }

    // Put it in the environment too.
    if (tz_name != NULL) {
        setenv("TZ", tz_name, 1);
        tzset();
    }
    
    int sum = 0;
    clock_t before = clock();
    for (unsigned long i = 0; i < cycles; i++) {
        struct timeval now;
        (void)gettimeofday(&now, NULL);

        struct tm tm;
        switch (mode) {
        case MODE_LOCALTIME_R:
            (void)localtime_r(&now.tv_sec, &tm);
            break;
        case MODE_GMTIME_R:
            (void)gmtime_r(&now.tv_sec, &tm);
            break;
        case MODE_LOCALTIME_RZ:
            (void)localtime_rz(tz, &now.tv_sec, &tm);
            break;
        }
                
        sum += tm.tm_sec + tm.tm_min + tm.tm_hour;
    }
    clock_t after = clock();

    printf("%g (%d)\n", (double)(after - before) / CLOCKS_PER_SEC, sum);

    struct tm tm;

    before = clock();
    for (unsigned long i = 0; i < cycles; i++) {
        switch (mode) {
        case MODE_LOCALTIME_R:
            sum += mktime(&tm);
            break;
        case MODE_GMTIME_R:
            sum += timegm(&tm);
            break;
        case MODE_LOCALTIME_RZ:
            sum += mktime_z(tz, &tm);
            break;
        }
    }
    after = clock();

    printf("%g (%d)\n", (double)(after - before) / CLOCKS_PER_SEC, sum);

    tzfree(tz);
    return 0;
}
