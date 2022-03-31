#pragma once

#include "../includes.hpp"

class CColor {
public:
    CColor();
    CColor(float, float, float, float);
    CColor(uint64_t);

    float r = 0, g = 0, b = 0, a = 255;

    uint64_t    getAsHex();
    
};