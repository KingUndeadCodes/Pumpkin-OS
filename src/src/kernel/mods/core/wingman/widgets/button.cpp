#include "../headers/widgets/button.h"
#include <string.h>

Button::Button(const char* message, uint32_t color, ButtonCallback onClick, void* userdata) {
    this->color = color;
    this->onClick = onClick;
    this->userdata = userdata;
    this->x = this->y = this->width = this->height = 0;
    if (message != NULL) {
        size_t messageLength = strlen(message);
        this->message = (char*)malloc(messageLength + 1);
        if (this->message != NULL) { strcpy(this->message, message); }
    } else {
        this->message = NULL;
    }
};

Button::~Button() {
    free(this->message);
};
