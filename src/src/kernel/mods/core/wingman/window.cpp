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

bool Window::handleKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    if (keyboardDelegate != nullptr) {
        return keyboardDelegate->onKeyboard(key, shift, meta, scancode);
    }
}

bool Window::handleMouse(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    if (mouseDelegate != nullptr) {
        return mouseDelegate->onMouseEvent(x, y, dx, dy, buttons, pressedEdge);
    }
    return false;
}