#include "./headers/window.h"
#include "./headers/cursor.h"

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
    this->onCloseRequested = nullptr;
    this->closeUserdata = nullptr;
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

void Window::setOnCloseRequested(WindowCloseCallback callback, void* userdata) {
    onCloseRequested = callback;
    closeUserdata = userdata;
}

void Window::setTitle(const char* title) {
    if (this->title != NULL) {
        free(this->title);
        this->title = NULL;
    }
    if (title != NULL) {
        size_t titleLength = strlen(title);
        this->title = (char*)malloc(titleLength + 1);
        if (this->title != NULL) { strcpy(this->title, title); }
    }
}

bool Window::handleKeyboard(char key, bool shift, bool meta, unsigned char scancode) {
    if (keyboardDelegate != nullptr) return keyboardDelegate->onKeyboard(key, shift, meta, scancode);
    return false;
}

bool Window::handleMouse(int x, int y, int dx, int dy, unsigned char buttons, unsigned char pressedEdge) {
    // Intercepted before the delegate ever sees it -- a click here should never also register on window content.
    if (titleBar.closeButtonContains(x, y)) {
        set_cursor_id(2);
        if (pressedEdge & 1) {
            // onCloseRequested can delete this Window a few frames down (e.g. wm->remove());
            // return immediately after, don't touch `this` again.
            if (onCloseRequested != nullptr) onCloseRequested(closeUserdata);
            return true;
        }
        return false;
    }
    if (mouseDelegate != nullptr) return mouseDelegate->onMouseEvent(x, y, dx, dy, buttons, pressedEdge);
    return false;
}