#pragma once
#include "PassElement.hpp"
#include "../types.hpp"

class CRendererHintsPassElement : public IPassElement {
  public:
    struct SData {
        std::optional<Render::SRenderModifData> renderModif;
    };

    CRendererHintsPassElement(const SData& data);
    virtual ~CRendererHintsPassElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();
    virtual bool        undiscardable();

    virtual const char* passName() {
        return "CRendererHintsPassElement";
    }

    virtual ePassElementType type() {
        return EK_HINTS;
    };

    SData m_data;
};