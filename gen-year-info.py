#!/usr/bin/env python3

import argparse
import sys

SECS_PER_DAY = 24 * 60 * 60
DAYS_PER_NYEAR = 365
SECS_PER_NYEAR = SECS_PER_DAY * DAYS_PER_NYEAR
DAYS_PER_400_YEARS = DAYS_PER_NYEAR * 400 + 400 // 4 - 4 + 1
SECS_PER_400_YEARS = DAYS_PER_400_YEARS * SECS_PER_DAY

def day_of_week(year, mon, day):
    mon -= 2
    if mon < 1:
        mon += 12
        year -= 1
    return (day + (26 * mon - 2) // 10 + year + year // 4 - year // 100 + year // 400) % 7;


def is_leap(year):
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)


def year_start_utc(year):
    assert year > 2000
    year -= 2001
    block = year // 400
    year %= 400

    return SECS_PER_400_YEARS * block + SECS_PER_NYEAR * year + SECS_PER_DAY * (year // 4 - year // 100)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output')
    args = parser.parse_args()

    with open(args.output, 'w') as f:
        f.write('#include <inttypes.h>\n')
        f.write('#include "constants.h"\n')
        f.write('\n')

        f.write('const int64_t tz64_year_starts[400] = {\n')
        for i in range(0, 400):
            f.write('    INT64_C({}),\n'.format(year_start_utc(i + 2001)))
        f.write('};\n')
        f.write('\n')

        f.write('const uint8_t tz64_year_types[400] = {\n')
        for i in range(0, 400, 4):
            f.write('   ')
            for j in range(0, 4):
                year = 2001 + i + j
                f.write(' {},'.format(is_leap(year) * 7 + day_of_week(year, 1, 1)))
            f.write('\n')
            
        f.write('};\n')
                    

if __name__ == '__main__':
    main()