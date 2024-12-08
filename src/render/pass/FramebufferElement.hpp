#pragma once
#include "PassElement.hpp"

class CFramebufferElement : public IPassElement {
  public:
    struct SFramebufferElementData {
        bool    main          = true;
        uint8_t framebufferID = 0;
    };

    CFramebufferElement(const SFramebufferElementData& data_);
    virtual ~CFramebufferElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CFramebufferElement";
    }

  private:
    SFramebufferElementData data;
};