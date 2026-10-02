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

    virtual bool        needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool        needsPrecomputeBlur(Render::CRenderContext& ctx);
    virtual bool        undiscardable(Render::CRenderContext& ctx);

    virtual const char* passName() {
        return "CFramebufferElement";
    }

    virtual ePassElementType type() {
        return EK_FRAMEBUFFER;
    };

    SFramebufferElementData m_data;
};
