#include "Color.hpp"

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c)   ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c)  ((double)(((c)) & 0xff) / 255.0)

CHyprColor::CHyprColor() = default;

CHyprColor::CHyprColor(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {
    okLab = Hyprgraphics::CColor(Hyprgraphics::CColor::SSRGB{r, g, b}).asOkLab();
}

CHyprColor::CHyprColor(uint64_t hex) : r(RED(hex)), g(GREEN(hex)), b(BLUE(hex)), a(ALPHA(hex)) {
    okLab = Hyprgraphics::CColor(Hyprgraphics::CColor::SSRGB{r, g, b}).asOkLab();
}

CHyprColor::CHyprColor(const Hyprgraphics::CColor& color, float a_) : a(a_) {
    const auto SRGB = color.asRgb();
    r               = SRGB.r;
    g               = SRGB.g;
    b               = SRGB.b;

    okLab = color.asOkLab();
}

uint32_t CHyprColor::getAsHex() const {
    return (uint32_t)(a * 255.f) * 0x1000000 + (uint32_t)(r * 255.f) * 0x10000 + (uint32_t)(g * 255.f) * 0x100 + (uint32_t)(b * 255.f) * 0x1;
}

Hyprgraphics::CColor::SSRGB CHyprColor::asRGB() const {
    return {r, g, b};
}

Hyprgraphics::CColor::SOkLab CHyprColor::asOkLab() const {
    return okLab;
}

Hyprgraphics::CColor::SHSL CHyprColor::asHSL() const {
    return Hyprgraphics::CColor(okLab).asHSL();
}

CHyprColor CHyprColor::stripA() const {
    return {r, g, b, 1.F};
}
