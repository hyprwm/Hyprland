#pragma once
#include "PassElement.hpp"
#include <optional>

class CWLSurfaceResource;
class CTexture;
class CSyncTimeline;

class CTexPassElement : public IPassElement {
  public:
    struct SRenderData {
        SP<CTexture>          tex;
        CBox                  box;
        float                 a     = 1.F;
        float                 blurA = 1.F;
        CRegion               damage;
        int                   round         = 0;
        float                 roundingPower = 2.0f;
        bool                  flipEndFrame  = false;
        std::optional<Mat3x3> replaceProjection;
        CBox                  clipBox;
        bool                  blur = false;
        std::optional<float>  ignoreAlpha;
    };

    CTexPassElement(const SRenderData& data);
    CTexPassElement(SRenderData&& data);
    virtual ~CTexPassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual void                discard();

    virtual const char*         passName() {
        return "CTexPassElement";
    }

  private:
    SRenderData m_data;
};
