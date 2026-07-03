#include  "cmos.h"

/*
    while (true) {
        print("\r");
        CMOSTime Time = FetchCurrentCMOSTime();
        printf("%d:%d:%d", Time.hours, Time.minutes, Time.seconds);
        if (Time.seconds == 0) {
            beep(1000, 18);
            continue;
        }
        timer_wait(18);
    }
*/

#define from_bcd(val)  ((val / 16) * 10 + (val & 0xf))

static uint8_t cmos_read(uint8_t index) {
	outb(0x70, index);
	return inb(0x71);
}

// Status Register A, bit 7: set while the RTC is updating its registers.
// Reading time fields during an update can return a torn value (e.g. a
// stale "seconds" paired with a rolled-over "minutes").
static bool cmos_update_in_progress(void) {
	return (cmos_read(0x0A) & 0x80) != 0;
}

static CMOSTime CMOSReadOnce(void) {
	CMOSTime Time;
	Time.seconds   = from_bcd(cmos_read(0));
	Time.minutes   = from_bcd(cmos_read(2));
	Time.hours     = from_bcd(cmos_read(4));
	Time.weekday   = from_bcd(cmos_read(6));
	Time.month_day = from_bcd(cmos_read(7));
	Time.month     = from_bcd(cmos_read(8));
	Time.year      = from_bcd(cmos_read(9));
	Time.century   = from_bcd(cmos_read(50));
	return Time;
}

CMOSTime CMOSDataFetch(void) {
	// Read twice (each preceded by an update-in-progress wait) and retry
	// until two consecutive reads agree, to rule out a torn read.
	while (cmos_update_in_progress()) {}
	CMOSTime prev = CMOSReadOnce();

	for (;;) {
		while (cmos_update_in_progress()) {}
		CMOSTime cur = CMOSReadOnce();
		if (memcmp(&prev, &cur, sizeof(CMOSTime)) == 0) {
			return cur;
		}
		prev = cur;
	}
}

CMOSTime FetchCurrentCMOSTime(void) {
	return CMOSDataFetch();
}