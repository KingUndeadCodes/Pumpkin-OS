#ifndef _SPEAKER_CPP
#define _SPEAKER_CPP
#include "../port.cpp"
#include <stdint.h>

static void PlaySound(uint32_t nFrequence);
static void Quiet();
void beep(uint32_t freq, uint32_t time = 18);

// Audio Examples
// void never_gonna(void);
//  void Zelda(void);
#endif
