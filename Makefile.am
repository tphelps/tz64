noinst_LIBRARIES = libtz64.a
bin_PROGRAMS = tzdump
check_PROGRAMS = test-localtime

libtz64_a_SOURCES = \
	tz.h tz.c \
	tzfile.h tzfile.c


tzdump_SOURCES = tzdump.c
tzdump_LDADD = libtz64.a

test_localtime_SOURCES = test-localtime.c
test_localtime_LDADD = libtz64.a
