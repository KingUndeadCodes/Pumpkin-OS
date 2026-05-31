#include "./tests/explorer/explorer.h"
#include "./tests/message/message.h"
#include "./headers/wingman.h"

static size_t bufferSize = 0;
static WindowManager* wm = nullptr;
static FileManager* fileManager = nullptr;
static MessageBox* messageBox = nullptr;

void keyboardFunctionFileManager(char key, bool shift, bool meta, unsigned char scancode) {
    if (fileManager != nullptr) {
        fileManager->keyboard_callback(key, shift, meta, scancode);
        return;
    };
};

void keyboardFunctionWindowManager(char key, bool shift, bool meta, unsigned char scancode) {
    if (wm != nullptr) {
        wm->keyboard_handler(key, shift, meta, scancode);
        wm->composite();
        color_t* buffer = wm->screen->getBuffer();
        memcpy(0xE0000000, buffer, bufferSize);
        return;
    };
};

void initalizeWindowSystem(void) {
    wm = new WindowManager();
    fileManager = new FileManager();
    messageBox = new MessageBox(DialogBoxError, "chorus: not initialized; call initalize() first.");
    /*
    Window* windowTest = new Window(1024, 768, 0, 0, "Test Window");
    windowTest->surface->clear(rgb(255, 0, 0));
    wm->add(windowTest);
    */
    wm->add(fileManager->window);
    wm->add(messageBox->window);
    wm->composite();
    fileManager->window->assignKeyboardFunction(keyboardFunctionFileManager);
    kb_add_event(keyboardFunctionWindowManager);
    color_t* buffer = wm->screen->getBuffer();
    bufferSize = wm->screen->getWidth() * wm->screen->getHeight() * sizeof(color_t); 
    memcpy(0xE0000000, buffer, bufferSize);
    return;
}