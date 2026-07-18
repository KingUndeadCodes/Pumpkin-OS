#include "logging.h"
#include <stdio.h>

static LogDeviceList* Logging::head;
static bool Logging::capturing;

void Logging::addLogDevice(LogDevice* device) {
    LogDeviceList* newNode = (LogDeviceList*)malloc(sizeof(LogDeviceList));
    newNode->value = device;
    newNode->next = Logging::head;
    newNode->prev = NULL;

    if (head != NULL) {
        head->prev = newNode;
    }
    Logging::head = newNode;
}

void Logging::propagate(const char* message) {
    LogDeviceList* current = Logging::head;
    while (current != NULL) {
        if (current->value && current->value->log) {
            current->value->log(message);
        }
        current = current->next;
    }
}

void Logging::capture() {
    Logging::capturing = true;
    return;
}

// See docs/DOCS.md ("mods/std/logging.cpp" section) for the
// NUL-termination fix shared by flush() and log() below.
void Logging::flush() {
    if (Logging::capturing) {
        FILE* file = fopen("/kmsglog", "r");
        if (file) {
            const size_t bufsize = 1024 * 2;
            char* buffer = (char*)malloc(bufsize + 1);
            size_t bytes_read = fread(buffer, 1, bufsize, file);
            buffer[bytes_read] = '\0';
            Logging::log(buffer);
            fclose(file);
            free(buffer);
        }
    }
    Logging::capturing = false;
    return;
}

void Logging::log(const char* message) {
    if (Logging::capturing == true) {
        FILE* file = fopen("/kmsglog", "a");
        if (file) {
            const size_t bufsize = 1024 * 2;
            char* buffer = (char*)malloc(bufsize + 1);
            size_t bytes_read = fread(buffer, 1, bufsize, file);
            buffer[bytes_read] = '\0';
            // Check if the last character is a newline
            if (bytes_read > 0 && buffer[bytes_read - 1] != '\n') {
                fwrite("\n", 1, 1, file);
            }
            fwrite(message, 1, strlen(message), file);
            free(buffer);
            fclose(file);
        }
    }
    Logging::propagate(message);
}

// Initialize static member
// LogDeviceList* Logging::head = NULL;
