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
