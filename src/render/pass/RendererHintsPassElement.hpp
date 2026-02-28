#pragma once
#include "PassElement.hpp"
#include <optional>
#include "../OpenGL.hpp"

class CRendererHintsPassElement : public IPassElement {
  public:
    struct SData {
        std::optional<SRenderModifData> renderModif;
    };

    CRendererHintsPassElement(const SData& data);
    virtual ~CRendererHintsPassElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();
    virtual bool        undiscardable();

    virtual const char* passName() {
        return "CRendererHintsPassElement";
    }

    virtual ePassElementKind kind() {
        return EK_HINTS;
    };

    SData m_data;
};