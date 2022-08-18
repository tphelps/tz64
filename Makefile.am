noinst_LIBRARIES = libtz64.a
bin_PROGRAMS = tzdump
check_PROGRAMS = \
	test-endpoints \
	test-localtime \
	test-mktime

TESTS = $(check_PROGRAMS)

libtz64_a_SOURCES = \
	tz.h tz.c \
	tzfile.h tzfile.c


tzdump_SOURCES = tzdump.c
tzdump_LDADD = libtz64.a

test_localtime_SOURCES = test-localtime.c
test_localtime_LDADD = libtz64.a

test_mktime_SOURCES = test-mktime.c
test_mktime_LDADD = libtz64.a

test_endpoints_SOURCES = test-endpoints.c
test_endpoints_LDADD = libtz64.a
