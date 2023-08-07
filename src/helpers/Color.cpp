#include "Color.hpp"

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c)   ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c)  ((double)(((c)) & 0xff) / 255.0)

CColor::CColor() {}

CColor::CColor(float r, float g, float b, float a) {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
}

CColor::CColor(uint64_t hex) {
    this->r = RED(hex);
    this->g = GREEN(hex);
    this->b = BLUE(hex);
    this->a = ALPHA(hex);
}

uint64_t CColor::getAsHex() {
    return ((int)a) * 0x1000000 + ((int)r) * 0x10000 + ((int)g) * 0x100 + ((int)b) * 0x1;
}