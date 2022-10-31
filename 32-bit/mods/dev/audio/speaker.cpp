#include "../pit/pit.h"
#include "speaker.h"

static void PlaySound(uint32_t nFrequence) {
    uint32_t x;
    uint8_t y;
    x = 1193180 / nFrequence;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t)(x));
    outb(0x42, (uint8_t)(x >> 8));
    y = inb(0x61);
    if (y != (y | 3)) {
 	    outb(0x61, y | 3);
    }
}

static void Quiet() {
    uint8_t x = inb(0x61) & 0xFC;
    outb(0x61, x);
}

void beep(uint32_t freq = 950, uint32_t time = 18) {
    if (time == 0) {
        print("beep warning: 'time' played is zero.", COLOR_YELLOW);
        return;
    }
    PlaySound(freq);
    timer_wait(time);
    Quiet();
}

void never_gonna(void) {
    // https://create.arduino.cc/projecthub/410027/rickroll-piezo-buzzer-a1cd11?ref=part&ref_id=8233&offset=3
    const int song1_intro_melody[] = {554, 622, 622, 698, 831, 740, 698, 622, 554, 622, -1, 415, 415};
    const int song1_intro_rhythmn[] = {6, 10, 6, 6, 1, 1, 1, 1, 6, 10, 4, 2, 10};
    const int song1_verse1_melody[] = {-1, 277, 277, 277, 277, 311, -1, 261, 233, 208, -1, 233, 233, 261, 277, 208, 415, 415, 311, -1, 233, 233, 261, 277, 233, 277, 311, -1, 261, 233, 233, 208, -1, 233, 233, 261, 277, 208, 208, 311, 311, 311, 349, 311, 277, 311, 349, 277, 311, 311, 311, 349, 311, 208, -1, 233, 261, 277, 208, -1, 311, 349, 311};
    const int song1_verse1_rhythmn[] = {2, 1, 1, 1, 1, 2, 1, 1, 1, 5, 1, 1, 1, 1, 3, 1, 2, 1, 5, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 4, 5, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 3, 1, 1, 1, 3};
    for (int i = 0; i < 13; i += 1) song1_intro_melody[i] == -1 ? timer_wait(song1_intro_rhythmn[i]) : beep((uint32_t)song1_intro_melody[i], (uint32_t)song1_intro_rhythmn[i]);
    for (int i = 0; i / sizeof(int) < sizeof(song1_intro_melody) / sizeof(int); i += 1) song1_verse1_melody[i] == -1 ? timer_wait(song1_verse1_rhythmn[i]) : beep((uint32_t)song1_verse1_melody[i], (uint32_t)song1_verse1_rhythmn[i] * 3);
}