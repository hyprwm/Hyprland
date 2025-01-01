#pragma once

#include <cstdint>
#include <hyprgraphics/color/Color.hpp>
#include "../debug/Log.hpp"
#include "../macros.hpp"

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
    CHyprColor                   modifyA(float newa) const;

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

//NOLINTNEXTLINE
namespace Colors {
    static const CHyprColor WHITE      = CHyprColor(1.F, 1.F, 1.F, 1.F);
    static const CHyprColor GREEN      = CHyprColor(0.F, 1.F, 0.F, 1.F);
    static const CHyprColor BLUE       = CHyprColor(0.F, 0.F, 1.F, 1.F);
    static const CHyprColor RED        = CHyprColor(1.F, 0.F, 0.F, 1.F);
    static const CHyprColor ORANGE     = CHyprColor(1.F, 0.5F, 0.F, 1.F);
    static const CHyprColor YELLOW     = CHyprColor(1.F, 1.F, 0.F, 1.F);
    static const CHyprColor MAGENTA    = CHyprColor(1.F, 0.F, 1.F, 1.F);
    static const CHyprColor PURPLE     = CHyprColor(0.5F, 0.F, 0.5F, 1.F);
    static const CHyprColor LIME       = CHyprColor(0.5F, 1.F, 0.1F, 1.F);
    static const CHyprColor LIGHT_BLUE = CHyprColor(0.1F, 1.F, 1.F, 1.F);
    static const CHyprColor BLACK      = CHyprColor(0.F, 0.F, 0.F, 1.F);
};
