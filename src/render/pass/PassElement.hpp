#pragma once

#include "../../defines.hpp"
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

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual bool                needsLiveBlur()       = 0;
    virtual bool                needsPrecomputeBlur() = 0;
    virtual const char*         passName()            = 0;
    virtual ePassElementKind    kind()                = 0;
    virtual void                discard();
    virtual bool                undiscardable();
    virtual std::optional<CBox> boundingBox();  // in monitor-local logical coordinates
    virtual CRegion             opaqueRegion(); // in monitor-local logical coordinates
    virtual bool                disableSimplification();
};
