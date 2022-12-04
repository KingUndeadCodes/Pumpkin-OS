#include "../pit/pit.h"
#include <tasking.h>
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

/*
void never_gonna(void) {
    // https://create.arduino.cc/projecthub/410027/rickroll-piezo-buzzer-a1cd11?ref=part&ref_id=8233&offset=3
    const int song1_intro_melody[] = {554, 622, 622, 698, 831, 740, 698, 622, 554, 622, -1, 415, 415};
    const int song1_intro_rhythmn[] = {6, 10, 6, 6, 1, 1, 1, 1, 6, 10, 4, 2, 10};
    const int song1_verse1_melody[] = {-1, 277, 277, 277, 277, 311, -1, 261, 233, 208, -1, 233, 233, 261, 277, 208, 415, 415, 311, -1, 233, 233, 261, 277, 233, 277, 311, -1, 261, 233, 233, 208, -1, 233, 233, 261, 277, 208, 208, 311, 311, 311, 349, 311, 277, 311, 349, 277, 311, 311, 311, 349, 311, 208, -1, 233, 261, 277, 208, -1, 311, 349, 311};
    const int song1_verse1_rhythmn[] = {2, 1, 1, 1, 1, 2, 1, 1, 1, 5, 1, 1, 1, 1, 3, 1, 2, 1, 5, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 4, 5, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 3, 1, 1, 1, 3};
    for (int i = 0; i < 13; i += 1) song1_intro_melody[i] == -1 ? timer_wait(song1_intro_rhythmn[i]) : beep((uint32_t)song1_intro_melody[i], (uint32_t)song1_intro_rhythmn[i]);
    for (int i = 0; i / sizeof(int) < sizeof(song1_intro_melody) / sizeof(int); i += 1) song1_verse1_melody[i] == -1 ? timer_wait(song1_verse1_rhythmn[i]) : beep((uint32_t)song1_verse1_melody[i], (uint32_t)song1_verse1_rhythmn[i] * 3);
}



#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_C5  523
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D5  587
#define NOTE_AS5 932
#define NOTE_CS5 554
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988


const short melody[] = {
    NOTE_AS4,-2,  NOTE_F4,8,  NOTE_F4,8,  NOTE_AS4,8,//1
    NOTE_GS4,16,  NOTE_FS4,16,  NOTE_GS4,-2,
    NOTE_AS4,-2,  NOTE_FS4,8,  NOTE_FS4,8,  NOTE_AS4,8,
    NOTE_A4,16,  NOTE_G4,16,  NOTE_A4,-2,
    1, 
    NOTE_AS4,4,  NOTE_F4,-4,  NOTE_AS4,8,  NOTE_AS4,16,  NOTE_C5,16, NOTE_D5,16, NOTE_DS5,16,//7
    NOTE_F5,2,  NOTE_F5,8,  NOTE_F5,8,  NOTE_F5,8,  NOTE_FS5,16, NOTE_GS5,16,
    NOTE_AS5,-2,  NOTE_AS5,8,  NOTE_AS5,8,  NOTE_GS5,8,  NOTE_FS5,16,
    NOTE_GS5,-8,  NOTE_FS5,16,  NOTE_F5,2,  NOTE_F5,4, 
    NOTE_DS5,-8, NOTE_F5,16, NOTE_FS5,2, NOTE_F5,8, NOTE_DS5,8, //11
    NOTE_CS5,-8, NOTE_DS5,16, NOTE_F5,2, NOTE_DS5,8, NOTE_CS5,8,
    NOTE_C5,-8, NOTE_D5,16, NOTE_E5,2, NOTE_G5,8, 
    NOTE_F5,16, NOTE_F4,16, NOTE_F4,16, NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,8, NOTE_F4,16,NOTE_F4,8,
    NOTE_AS4,4,  NOTE_F4,-4,  NOTE_AS4,8,  NOTE_AS4,16,  NOTE_C5,16, NOTE_D5,16, NOTE_DS5,16,//15
    NOTE_F5,2,  NOTE_F5,8,  NOTE_F5,8,  NOTE_F5,8,  NOTE_FS5,16, NOTE_GS5,16,
    NOTE_AS5,-2, NOTE_CS6,4,
    NOTE_C6,4, NOTE_A5,2, NOTE_F5,4,
    NOTE_FS5,-2, NOTE_AS5,4,
    NOTE_A5,4, NOTE_F5,2, NOTE_F5,4,
    NOTE_FS5,-2, NOTE_AS5,4,
    NOTE_A5,4, NOTE_F5,2, NOTE_D5,4,
    NOTE_DS5,-2, NOTE_FS5,4,
    NOTE_F5,4, NOTE_CS5,2, NOTE_AS4,4,
    NOTE_C5,-8, NOTE_D5,16, NOTE_E5,2, NOTE_G5,8, 
    NOTE_F5,16, NOTE_F4,16, NOTE_F4,16, NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,8, NOTE_F4,16,NOTE_F4,8
};

void Zelda(void) {
    for (int i = 0; i < 219; i++) {
        PlaySound(melody[i]);
        // timer_wait(wholenote / melody[i]);
        timer_wait(2);
        Quiet();
    }
}
*/