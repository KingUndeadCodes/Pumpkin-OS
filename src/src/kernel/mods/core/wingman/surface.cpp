#include "./headers/surface.h"
#include "../../dev/serial/serial.h"

Surface::Surface(int width, int height) {
    this->width = width;
    this->height = height;
    this->pixels = (color_t*)malloc((size_t)this->width * (size_t)this->height * sizeof(color_t));
    if (this->pixels != NULL) { this->clear(rgba(0, 0, 0, 0)); }
};

Surface::~Surface() {
    if (this->pixels != NULL) {
        free(this->pixels);
        this->pixels = NULL;
    }
}

int Surface::getWidth() { return this->width; }
int Surface::getHeight() { return this->height; }
color_t* Surface::getBuffer() { return this->pixels; }

void Surface::clear(color_t color) {
    if (this->pixels == NULL) return;
    for (int i = 0; i < this->width * this->height; i++) this->pixels[i] = color;
};

void Surface::putPixel(int x, int y, color_t color) {
    if (this->pixels == NULL) return;
    if (x < 0 || y < 0) return;
    if (x >= this->width || y >= this->height) return;
    this->pixels[y * this->width + x] = color;
};

void Surface::putPixelUnsafe(int x, int y, color_t color) {
    this->pixels[y * this->width + x] = color;
};

color_t Surface::getPixel(int x, int y) {
    if (this->pixels == NULL) return rgba(0, 0, 0, 0);
    if (x < 0 || y < 0) return rgba(0, 0, 0, 0);
    if (x >= this->width || y >= this->height) return rgba(0, 0, 0, 0);
    return this->pixels[y * this->width + x];
};