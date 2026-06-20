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
    this->keyboardDelegate = nullptr;
    this->mouseDelegate = nullptr;
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

void Window::setKeyboardDelegate(KeyboardDelegate* delegate) {
    keyboardDelegate = delegate;
}

void Window::setMouseDelegate(MouseDelegate* delegate) {
    mouseDelegate = delegate;
}

void Window::handleKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    if (keyboardDelegate != nullptr) {
        keyboardDelegate->onKeyboard(key, shift, meta, scancode);
    }
}

void Window::handleMouse(int x, int y, int dx, int dy, unsigned char buttons) {
    if (mouseDelegate != nullptr) {
        mouseDelegate->onMouseEvent(x, y, dx, dy, buttons);
    }
}