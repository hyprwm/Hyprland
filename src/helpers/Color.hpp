#pragma once

#include "../includes.hpp"

class CColor {
public:
    CColor();
    CColor(float, float, float, float);
    CColor(uint64_t);

    float r = 0, g = 0, b = 0, a = 255;

    uint64_t getAsHex();

    CColor operatorMinus(const CColor& c2) const {
        return CColor(r - c2.r, g - c2.g, b - c2.b, a - c2.a);
    }

    CColor operatorPlus(const CColor& c2) const {
        return CColor(r + c2.r, g + c2.g, b + c2.b, a + c2.a);
    }

    CColor operatorMultiply(const float& v) const {
        return CColor(r * v, g * v, b * v, a * v);
    }
    
    bool operatorEq(const CColor& c2) const {
        return r == c2.r && g == c2.g && b == c2.b && a == c2.a;
    }
};
