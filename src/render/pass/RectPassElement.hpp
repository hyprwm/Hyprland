#pragma once
#include "PassElement.hpp"

class CRectPassElement : public IPassElement {
  public:
    struct SRectData {
        CBox       box;
        CHyprColor color;
        int        round = 0;
        bool       blur = false, xray = false;
        float      blurA = 1.F;
    };

    CRectPassElement(const SRectData& data);
    virtual ~CRectPassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();

    virtual const char*         passName() {
        return "CRectPassElement";
    }

  private:
    SRectData data;
};