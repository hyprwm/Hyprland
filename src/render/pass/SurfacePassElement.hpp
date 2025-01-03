#pragma once
#include "PassElement.hpp"
#include <optional>

class CWLSurfaceResource;
class CTexture;
class CSyncTimeline;

class CSurfacePassElement : public IPassElement {
  public:
    struct SRenderData {
        PHLMONITORREF pMonitor;
        timespec*     when = nullptr;
        Vector2D      pos, localPos;

        // for iters
        void*                  data        = nullptr;
        SP<CWLSurfaceResource> surface     = nullptr;
        SP<CTexture>           texture     = nullptr;
        bool                   mainSurface = true;
        double                 w = 0, h = 0;

        // for rounding
        bool dontRound = true;

        // for fade
        float fadeAlpha = 1.f;

        // for alpha settings
        float alpha = 1.f;

        // for decorations (border)
        bool decorate = false;

        // for custom round values
        int rounding = -1; // -1 means not set

        // for custom rounding powers
        float roundingPower = 2.0f;

        // for blurring
        bool blur                  = false;
        bool blockBlurOptimization = false;

        // only for windows, not popups
        bool squishOversized = true;

        // for calculating UV
        PHLWINDOW pWindow;
        PHLLS     pLS;

        bool      popup = false;

        // counts how many surfaces this pass has rendered
        int      surfaceCounter = 0;

        CBox     clipBox = {}; // scaled coordinates

        uint32_t discardMode    = 0;
        float    discardOpacity = 0.f;

        bool     useNearestNeighbor = false;

        bool     flipEndFrame = false;
    };

    CSurfacePassElement(const SRenderData& data);
    virtual ~CSurfacePassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual void                discard();

    virtual const char*         passName() {
        return "CSurfacePassElement";
    }

  private:
    SRenderData data;

    CBox        getTexBox();
};
