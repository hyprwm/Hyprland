#pragma once

#include "../../defines.hpp"
#include <cstdint>
#include <optional>

enum ePassElementKind : uint8_t {
    EK_UNKNOWN = 0,
    EK_BORDER,
    EK_CLEAR,
    EK_FRAMEBUFFER,
    EK_PRE_BLUR,
    EK_RECT,
    EK_HINTS,
    EK_SHADOW,
    EK_SURFACE,
    EK_TEXTURE,
    EK_TEXTURE_MATTE
};

enum ePassStage : uint8_t {
    PS_INIT = 1 << 0,
    PS_MAIN = 1 << 1,
    PS_POST = 1 << 2,
};

const auto PS_ANY = PS_INIT | PS_MAIN | PS_POST;

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual bool                needsLiveBlur()       = 0;
    virtual bool                needsPrecomputeBlur() = 0;
    virtual const char*         passName()            = 0;
    virtual ePassElementKind    kind()                = 0;
    virtual ePassStage          stage();
    virtual void                discard();
    virtual bool                undiscardable();
    virtual std::optional<CBox> boundingBox();  // in monitor-local logical coordinates
    virtual CRegion             opaqueRegion(); // in monitor-local logical coordinates
    virtual bool                disableSimplification();
};
