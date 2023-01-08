#ifndef _TIME_H
#define _TIME_H 1

#include "../../dev/cmos/cmos.h"
#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef __utc
#define __utc "UTC"
#endif  

typedef long int clock_t;
typedef long int time_t;

struct tm {
    int tm_sec;    // Seconds [0,60].
    int tm_min;    // Minutes [0,59].
    int tm_hour;   // Hour [0,23].
    int tm_mday;   // Day of month [1,31].
    int tm_mon;    // Month of year [0,11].
    int tm_year;   // Years since 1900.
    int tm_wday;   // Day of week [0,6] (Sunday =0).
    int tm_yday;   // Day of year [0,365].
    int tm_isdstl; // Daylight Savings flag.
    long int tm_gmtoff;
    const char *tm_zone;
}; // __attribute__((packed))

struct timespec {
    time_t  tv_sec;    // Seconds.
    long    tv_nsec;   // Nanoseconds.
};

clock_t clock(void);
double difftime(time_t t1, time_t t2);
struct tm *gmtime(const time_t *time);
struct tm *localtime(const time_t *time);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
time_t time(time_t* t);

#endif