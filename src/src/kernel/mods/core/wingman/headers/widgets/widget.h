#pragma once

#include <stdlib.h>
#include <stddef.h>
#include "../surface.h"

// See docs/DOCS.md ("mods/core/wingman/headers/widgets/widget.h" section).
enum WidgetType {
    WidgetTypeButton,
    WidgetTypeTextInput,
    WidgetTypeCheckbox,
};

class Widget {
    public:
        int x;
        int y;
        int width;
        int height;
        WidgetType type;
        Widget(WidgetType type) : x(0), y(0), width(0), height(0), type(type) {}
        virtual ~Widget() {}
        bool contains(int px, int py) const {
            return px >= this->x && px < this->x + this->width &&
                   py >= this->y && py < this->y + this->height;
        }
        virtual void draw(Surface* surface, int thickness) const = 0;
        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};
