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

// SHould I make it static and volatile?
CMOSTime Time;

#define from_bcd(val)  ((val / 16) * 10 + (val & 0xf))

void CMOSDataFetch(void) {
	uint16_t CMOSValues[128];
	uint16_t index;
	for (index = 0; index < 128; ++index) {
		outb(0x70, index);
		CMOSValues[index] = inb(0x71);
	}
	Time.seconds = from_bcd(CMOSValues[0]);
	Time.minutes = from_bcd(CMOSValues[2]);
	Time.hours = from_bcd(CMOSValues[4]);
	Time.weekday = from_bcd(CMOSValues[6]);
	Time.month_day = from_bcd(CMOSValues[7]);
	Time.month = from_bcd(CMOSValues[8]);
	Time.year = from_bcd(CMOSValues[9]);
	Time.century = from_bcd(CMOSValues[50]);
	return;
} 

CMOSTime FetchCurrentCMOSTime(void) {
	CMOSDataFetch();
	return Time;
}