#include "Color.hpp"
#include "../defines.hpp"

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