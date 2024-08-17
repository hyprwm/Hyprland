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

uint32_t CColor::getAsHex() const {
    return (uint32_t)(a * 255.f) * 0x1000000 + (uint32_t)(r * 255.f) * 0x10000 + (uint32_t)(g * 255.f) * 0x100 + (uint32_t)(b * 255.f) * 0x1;
}