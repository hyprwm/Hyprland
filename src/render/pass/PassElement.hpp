#pragma once

#include "../../defines.hpp"
#include <optional>

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual void                draw(const CRegion& damage) = 0;
    virtual bool                needsLiveBlur()             = 0;
    virtual bool                needsPrecomputeBlur()       = 0;
    virtual const char*         passName()                  = 0;
    virtual void                discard();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual bool                disableSimplification();
};
