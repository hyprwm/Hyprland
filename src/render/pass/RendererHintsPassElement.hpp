#pragma once
#include "PassElement.hpp"
#include "../types.hpp"

class CRendererHintsPassElement : public IPassElement {
  public:
    struct SData {
        std::optional<Render::SRenderModifData> renderModif;
        bool                                    append = false;
        // pop this many modifs from the current renderModif (ignores renderModif/append).
        size_t popCount = 0;
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