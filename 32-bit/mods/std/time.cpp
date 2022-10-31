// Work in Progress!!!

#include "include/time.h"

/*
I have no idea what I'm doing. I'll be copying directly from mlibc (https://github.com/managarm/mlibc/blob/master/options/ansi/generic/time-stubs.cpp).
One More Thing: This is REAL C++ code... oh no.
*/

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

clock_t clock(void) {
	struct timespec ts;
	/*
	if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) return -1;
	if(ts.tv_sec > LONG_MAX / 1000000 || ts.tv_nsec / 1000 > LONG_MAX - 1000000 * ts.tv_sec) return -1;
	*/
	CMOSTime T = FetchCurrentCMOSTime();
	ts.tv_sec = T.seconds;
	ts.tv_nsec = 0; // No way of getting nanoseconds from CMOS
	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/*
must add later
time_t time(time_t *tloc);
*/

void civil_from_days(time_t days_since_epoch, int *year, unsigned int *month, unsigned int *day) {
	time_t time = days_since_epoch + 719468;
	int era = (time >= 0 ? time : time - 146096) / 146097;
	unsigned int doe = static_cast<unsigned int>(time - era * 146097);
	unsigned int yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
	int y = static_cast<int>(yoe) + era * 400;
	unsigned int doy = doe - (365*yoe + yoe/4 - yoe/100);
	unsigned int mp = (5*doy + 2)/153;
	unsigned int d = doy - (153*mp+2)/5 + 1;
	unsigned int m = mp + (mp < 10 ? 3 : -9);
	*year = y + (m <= 2);
	*month = m;
	*day = d;
}

void weekday_from_days(time_t days_since_epoch, unsigned int *weekday) {
	*weekday = static_cast<unsigned int>(days_since_epoch >= -4 ? (days_since_epoch+4) % 7 : (days_since_epoch+5) % 7 + 6);
}

void yearday_from_date(unsigned int year, unsigned int month, unsigned int day, unsigned int *yday) {
	unsigned int n1 = 275 * month / 9;
	unsigned int n2 = (month + 9) / 12;
	unsigned int n3 = (1 + (year - 4 * year / 4 + 2) / 3);
	*yday = n1 - (n2 * n3) + day - 30;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    return 0; // Can be implemented later (not important)
};

double difftime(time_t a, time_t b) {
	return a - b;
}

struct tm *gmtime_r(const time_t *unix_gmt, struct tm *res) {
	int year;
	unsigned int month;
	unsigned int day;
	unsigned int weekday;
	unsigned int yday;
	time_t unix_local = *unix_gmt;
	int days_since_epoch = unix_local / (60*60*24);
	civil_from_days(days_since_epoch, &year, &month, &day);
	weekday_from_days(days_since_epoch, &weekday);
	yearday_from_date(year, month, day, &yday);
	res->tm_sec = unix_local % 60;
	res->tm_min = (unix_local / 60) % 60;
	res->tm_hour = (unix_local / (60*60)) % 24;
	res->tm_mday = day;
	res->tm_mon = month - 1;
	res->tm_year = year - 1900;
	res->tm_wday = weekday;
	res->tm_yday = yday - 1;
	res->tm_isdstl = -1;
	res->tm_zone = __utc;
	res->tm_gmtoff = 0;
	return res;
}

/*
Notes: Removed `thread_local` c++ keyword because OS is single threaded.
*/
struct tm *gmtime(const time_t *unix_gmt) {
	static struct tm per_thread_tm;
	return gmtime_r(unix_gmt, &per_thread_tm);
}


struct tm *localtime_r(const time_t *unix_gmt, struct tm *res) {
	int year;
	unsigned int month;
	unsigned int day;
	unsigned int weekday;
	unsigned int yday;
	time_t offset = 0;
	bool dst;
	char *tm_zone;
    /*
    frg::unique_lock<FutexLock> lock(__time_lock);
	// TODO: Set errno if the conversion fails.
	if(unix_local_from_gmt(*unix_gmt, &offset, &dst, &tm_zone)) {
		__ensure(!"Error parsing /etc/localtime");
		__builtin_unreachable();
	}
    */
    // CMOSTime T = FetchCurrentCMOSTime();
    // HARD CODED VALUES
    dst = true;
    tm_zone = "CDT";
    // END HARD CODED VALUES
	time_t unix_local = *unix_gmt + offset;
	int days_since_epoch = unix_local / (60*60*24);
	civil_from_days(days_since_epoch, &year, &month, &day);
	weekday_from_days(days_since_epoch, &weekday);
	yearday_from_date(year, month, day, &yday);
	res->tm_sec = unix_local % 60;
	res->tm_min = (unix_local / 60) % 60;
	res->tm_hour = (unix_local / (60*60)) % 24;
	res->tm_mday = day;
	res->tm_mon = month - 1;
	res->tm_year = year - 1900;
	res->tm_wday = weekday;
	res->tm_yday = yday - 1;
	res->tm_isdstl = dst;
	res->tm_zone = tm_zone;
	res->tm_gmtoff = offset;
	return res;
}

struct tm *localtime(const time_t *unix_gmt) {
	FetchCurrentCMOSTime();
	static thread_local struct tm per_thread_tm;
	return localtime_r(unix_gmt, &per_thread_tm);
}

constexpr static int days_from_civil(int y, unsigned m, unsigned d) noexcept {
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400); // [0, 399]
	const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1; // [0, 365]
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
	return era * 146097 + static_cast<int>(doe) - 719468;
}

time_t mktime(struct tm *tm) {
    time_t year = tm->tm_year + 1900;
	time_t month = tm->tm_mon + 1;
	time_t days = days_from_civil(year, month, tm->tm_mday);
	time_t secs = (days * 86400) + (tm->tm_hour * 60 * 60) + (tm->tm_min * 60) + tm->tm_sec;
	return secs;
}