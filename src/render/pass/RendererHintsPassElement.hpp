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

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CRendererHintsPassElement";
    }

  private:
    SData data;
};