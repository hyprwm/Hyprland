#include "Color.hpp"
#include "../defines.hpp"

CColor::CColor() {

}

CColor::CColor(float r, float g, float b, float a) {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
}

CColor::CColor(uint64_t hex) {
    this->r = RED(hex) * 255.f;
    this->g = GREEN(hex) * 255.f;
    this->b = BLUE(hex) * 255.f;
    this->a = ALPHA(hex) * 255.f;
}

uint64_t CColor::getAsHex() {
    return ((int)a) * 0x1000000 + ((int)r) * 0x10000 + ((int)g) * 0x100 + ((int)b) * 0x1;
}