#pragma once

#include "../../defines.hpp"

class IPassElement {
  public:
    virtual ~IPassElement() = default;

    virtual void draw(const CRegion& damage) = 0;
    virtual bool needsLiveBlur()             = 0;
    virtual bool needsPrecomputeBlur()       = 0;
};
