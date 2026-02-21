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

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();
    virtual bool        undiscardable();

    virtual const char* passName() {
        return "CFramebufferElement";
    }

    virtual ePassElementKind kind() {
        return EK_FRAMEBUFFER;
    };

    SFramebufferElementData m_data;
};