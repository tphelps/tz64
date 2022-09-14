# Copyright 2022 Ted Phelps
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

bin_PROGRAMS = tools/tzdump
check_PROGRAMS = \
	test/test-endpoints \
	test/test-localtime \
	test/test-mktime

noinst_PROGRAMS = \
	test/perf-conv

TESTS = $(check_PROGRAMS)

noinst_HEADERS = lib/constants.h test/utils.h
include_HEADERS = lib/tz64.h lib/tz64compat.h lib/tz64file.h

BUILT_SOURCES = lib/yearinfo.c

lib/yearinfo.c: $(srcdir)/gen-year-info.py
	$(srcdir)/gen-year-info.py $@


lib_LIBRARIES = lib/libtz64.a

lib_libtz64_a_SOURCES = \
	lib/constants.h \
	lib/tz64.h lib/tz64.c \
	lib/tz64file.h lib/tz64file.c \
	lib/yearinfo.c

AM_CPPFLAGS = -I$(srcdir)/lib

tools_tzdump_SOURCES = tools/tzdump.c
tools_tzdump_LDADD = lib/libtz64.a

test_test_localtime_SOURCES = test/test-localtime.c test/utils.c
test_test_localtime_LDADD = lib/libtz64.a

test_test_mktime_SOURCES = test/test-mktime.c test/utils.c
test_test_mktime_LDADD = lib/libtz64.a

test_test_endpoints_SOURCES = test/test-endpoints.c test/utils.c
test_test_endpoints_LDADD = lib/libtz64.a

test_perf_conv_SOURCES = test/perf-conv.c
test_perf_conv_LDADD = lib/libtz64.a

