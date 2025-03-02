#ifndef __IMAGE_STRUCT_H
#define __IMAGE_STRUCT_H

typedef struct IntegerRange {
    uint8_t continueWith;
    uint16_t continueFor; // When the image is used change this back to `uint8_t` below.
    // uint8_t continueFor;    
} IntegerRange;

#endif