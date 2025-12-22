#include "../include/logging.h"
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

void Logging::flush() {
    if (Logging::capturing) {
        FILE* file = fopen("/kmsglog", "r");
        if (file) {
            // TODO: free the buffer.
            char* buffer = (char*)malloc(1024 * 2 * sizeof(char));
            fread(buffer, 1, 1024 * 2, file);
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
            // Free the buffer
            char* buffer = (char*)malloc(1024 * 2 * sizeof(char));
            fread(buffer, 1, 1024 * 2, file);
            // Check if the last character is a newline
            if (buffer[strlen(buffer) - 1] != '\n') {
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
