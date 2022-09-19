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

#ifndef TZ64COMPAT_H
#define TZ64COMPAT_H

#include <inttypes.h>
#include <tz64.h>

static inline struct tz64 *tzalloc(const char *desc)
{
    return tz64_alloc(desc);
}


static inline void tzfree(struct tz64 *tz)
{
    return tz64_free(tz);
}


static inline struct tm *localtime_rz(const struct tz64 *restrict tz, time_t const *restrict ts, struct tm *restrict tm)
{
    return tz64_ts_to_tm(tz, *ts, tm);
}


static inline time_t mktime_z(const struct tz64 *tz, struct tm *tm)
{
    int64_t ts = tz64_tm_to_ts(tz, tm);
    time_t res = ts;
    return (res == ts) ? res : -1;
}

#endif // TZ64COMPAT_H
