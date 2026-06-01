#pragma once
#include "PassElement.hpp"
#include <optional>

class CWLSurfaceResource;
namespace Render {
    class ITexture;
}
class CSyncTimeline;

enum eDiscardMode : uint8_t {
    DISCARD_OPAQUE = 1,
    DISCARD_ALPHA  = 1 << 1
};

enum eWrapMode : uint8_t {
    WRAP_CLAMP_TO_EDGE,
    WRAP_REPEAT,
};

struct SMotionBlurData {
    bool     enabled  = false;
    CBox     previous = {};
    CBox     current  = {};
    CBox     source   = {};
    Vector2D sourceTexSize;
    int      samples = 1;

    CBox     extents() const;
};

class CTexPassElement : public IPassElement {
  public:
    struct SRenderData {
        SP<Render::ITexture>   tex;
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
        bool                   blur           = false;
        bool                   forceBlurBlend = false;
        std::optional<float>   ignoreAlpha;
        std::optional<bool>    blockBlurOptimization;
        bool                   cmBackToSRGB = false;

        bool                   discardActive = false;
        bool                   allowCustomUV = false;
        SP<CWLSurfaceResource> surface       = nullptr;

        uint8_t                wrapX = WRAP_CLAMP_TO_EDGE;
        uint8_t                wrapY = WRAP_CLAMP_TO_EDGE;

        uint8_t                discardMode    = DISCARD_OPAQUE;
        float                  discardOpacity = 0.f;

        CRegion                clipRegion;
        PHLLSREF               currentLS;

        SP<Render::ITexture>   blurredBG;
        SP<Render::ITexture>   blurAlphaMatte;

        SMotionBlurData        motionBlur;
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

    virtual ePassElementType type() {
        return EK_TEXTURE;
    };

    SRenderData m_data;
};
