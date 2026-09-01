#pragma once
#include "PassElement.hpp"
#include <hyprutils/math/Region.hpp>
#include <optional>

class CRectPassElement : public IPassElement {
  public:
    struct SRectData {
        CBox                box;
        CHyprColor          color;
        int                 round         = 0;
        float               roundingPower = 2.0f;
        bool                blur = false, xray = false;
        float               blurA = 1.F;
        std::optional<CBox> blurPatternBox;
        PHLWINDOWREF        blurOwner;
        CBox                clipBox;

        // internal
        CBox    modifiedBox;
        float   TOPLEFT[2];
        float   FULLSIZE[2];
        CRegion drawRegion;
    };

    CRectPassElement(const SRectData& data);
    virtual ~CRectPassElement() = default;

    virtual bool                needsLiveBlur(const Render::CRenderingContext&);
    virtual bool                needsPrecomputeBlur(const Render::CRenderingContext&);
    virtual std::optional<CBox> boundingBox(const Render::CRenderingContext& context);
    virtual CRegion             opaqueRegion(const Render::CRenderingContext& context);

    virtual const char*         passName() {
        return "CRectPassElement";
    }

    virtual ePassElementType type() {
        return EK_RECT;
    };

    SRectData m_data;
};
