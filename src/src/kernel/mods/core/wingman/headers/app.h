#pragma once

#include <stdlib.h>
#include <stddef.h>
#include "./window.h"
#include "./manager.h"

// Common shape shared by every Wingman app that owns a Window (FileManager,
// MessageBox, WidgetDemo) -- the wm/ref/window members, the close-then-
// delete-self pattern, and the close-callback/delegate/registration wiring
// every one of them used to hand-write independently.
class WingmanApp : public KeyboardDelegate, public MouseDelegate {
    protected:
        WindowManager* wm;
        window_ref_t ref;
        Window* window;
        WingmanApp(WindowManager* wm) : wm(wm), ref(WINGMAN_INVALID_WINDOW), window(nullptr) {}
        // Call once, as the last line of a derived constructor, after `window`
        // is fully built and configured (titleBar, title, etc).
        void registerWindow() {
            this->window->setOnCloseRequested(&WingmanApp::closeTrampoline, this);
            this->window->setKeyboardDelegate(this);
            this->window->setMouseDelegate(this);
            this->ref = WINGMAN_INVALID_WINDOW;
            if (this->wm != NULL) {
                this->ref = this->wm->add(this->window);
                if (this->ref != WINGMAN_INVALID_WINDOW) this->wm->focus(this->ref);
            }
        }
        // Removes the window from the WindowManager, then frees this instance --
        // callers must not touch `this` after (directly, or via closeTrampoline()).
        void close() {
            if (this->wm != NULL && this->ref != WINGMAN_INVALID_WINDOW) this->wm->remove(this->ref);
            this->window = NULL;
            delete this;
        }
        static void closeTrampoline(void* userdata) {
            ((WingmanApp*)userdata)->close();
        }
    public:
        virtual ~WingmanApp() {
            if (this->window != NULL) {
                delete this->window;
                this->window = NULL;
            }
        }
        void* operator new(size_t size) { return malloc(size); }
        void* operator new[](size_t size) { return malloc(size); }
        void operator delete(void* p) { free(p); }
        void operator delete[](void* p) { free(p); }
};
