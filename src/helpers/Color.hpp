#pragma once

#include <cstdint>
#include <hyprgraphics/color/Color.hpp>

class CHyprColor {
  public:
    CHyprColor();
    CHyprColor(float r, float g, float b, float a);
    CHyprColor(const Hyprgraphics::CColor& col, float a);
    CHyprColor(uint64_t);

    // AR32
    uint32_t                     getAsHex() const;
    Hyprgraphics::CColor::SSRGB  asRGB() const;
    Hyprgraphics::CColor::SOkLab asOkLab() const;
    Hyprgraphics::CColor::SHSL   asHSL() const;
    CHyprColor                   stripA() const;

    //
    bool operator==(const CHyprColor& c2) const {
        return c2.r == r && c2.g == g && c2.b == b && c2.a == a;
    }

    // stubs for the AnimationMgr
    CHyprColor operator-(const CHyprColor& c2) const {
        RASSERT(false, "CHyprColor: - is a STUB");
        return {};
    }

    CHyprColor operator+(const CHyprColor& c2) const {
        RASSERT(false, "CHyprColor: + is a STUB");
        return {};
    }

    CHyprColor operator*(const float& c2) const {
        RASSERT(false, "CHyprColor: * is a STUB");
        return {};
    }

    double r = 0, g = 0, b = 0, a = 0;

  private:
    Hyprgraphics::CColor::SOkLab okLab; // cache for the OkLab representation
};
