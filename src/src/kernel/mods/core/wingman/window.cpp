#include "./headers/window.h"

Window::Window(int width, int height, int offsetX, int offsetY, const char* title) {
    this->width = width;
    this->height = height;
    this->offsetX = offsetX;
    this->offsetY = offsetY;
    if (title != NULL) {
        size_t titleLength = strlen(title);
        this->title = (char*)malloc(titleLength + 1);
        if (this->title != NULL) { strcpy(this->title, title); }
    } else {
        this->title = NULL;
    }
    this->surface = new Surface(width, height);
    this->fn = nullptr;
};

Window::~Window() {
    if (this->title != NULL) {
        free(this->title);
        this->title = NULL;
    }
    if (this->surface != NULL) {
        delete this->surface;
        this->surface = NULL;
    }
};

void Window::assignKeyboardFunction(GlobalKbCallback function) {
    this->fn = function;
    return;
};

void Window::keyboard_callback(char key, bool shift, bool meta, unsigned char scancode) {
    if (this->fn != nullptr) {
        this->fn(key, shift, meta, scancode);
        return;
    }
};