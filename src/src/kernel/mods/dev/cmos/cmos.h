#ifndef CMOS_H
#define CMOS_H

#include "../port.cpp"
#include <stdint.h>
#include <string.h>

typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t weekday;
	uint8_t month_day;
	uint8_t month;
	uint8_t year;
	uint8_t century;
} __attribute__((packed)) CMOSTime;

CMOSTime FetchCurrentCMOSTime(void);

#endif