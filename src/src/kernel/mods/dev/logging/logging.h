#ifndef __LOGGING_H
#define __LOGGING_H

#include <stdio.h>
#include <stdlib.h>

typedef struct LogDevice {
    void (*log)(const char* message);
} LogDevice;

typedef struct LogDeviceList {
    struct LogDeviceList* next;
    struct LogDeviceList* prev;
    LogDevice* value;
} LogDeviceList;

class Logging {
    private:
        static void propagate(const char* message);
        static bool capturing;
    public:
        static LogDeviceList* head;
        static void addLogDevice(LogDevice* device);
        static void log(const char* message);
        static void capture();
        static void flush();
};

#endif