#pragma once
#include "PassElement.hpp"
#include <optional>

class CWLSurfaceResource;
class ITexture;
class CSyncTimeline;

enum eDiscardMode : uint8_t {
    DISCARD_OPAQUE = 1,
    DISCARD_ALPHA  = 1 << 1
};

class CTexPassElement : public IPassElement {
  public:
    struct SRenderData {
        SP<ITexture>           tex;
        CBox                   box;
        float                  a        = 1.F;
        float                  blurA    = 1.F;
        float                  overallA = 1.F;
        CRegion                damage;
        int                    round               = 0;
        float                  roundingPower       = 2.0f;
        bool                   flipEndFrame        = false;
        bool                   useMirrorProjection = false;
        CBox                   clipBox;
        bool                   blur = false;
        std::optional<float>   ignoreAlpha;
        std::optional<bool>    blockBlurOptimization;
        bool                   cmBackToSRGB = false;
        SP<CMonitor>           cmBackToSRGBSource;

        bool                   discardActive = false;
        bool                   allowCustomUV = false;
        SP<CWLSurfaceResource> surface       = nullptr;

        uint32_t               discardMode    = DISCARD_OPAQUE;
        float                  discardOpacity = 0.f;

        CRegion                clipRegion;
        PHLLSREF               currentLS;
    };

    CTexPassElement(const SRenderData& data);
    CTexPassElement(SRenderData&& data);
    virtual ~CTexPassElement() = default;

    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual void                discard();

    virtual const char*         passName() {
        return "CTexPassElement";
    }

    virtual ePassElementKind kind() {
        return EK_TEXTURE;
    };

    SRenderData m_data;
};
