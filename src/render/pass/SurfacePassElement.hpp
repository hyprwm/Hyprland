#pragma once
#include "PassElement.hpp"
#include "../../helpers/time/Time.hpp"

class CWLSurfaceResource;
class CTexture;
class CSyncTimeline;
class CRenderPass;

class CSurfacePassElement : public IPassElement {
  public:
    struct SRenderData {
        PHLMONITORREF          pMonitor;
        Time::steady_tp        when = Time::steadyNow();
        Vector2D               pos, localPos;

        void*                  data        = nullptr;
        SP<CWLSurfaceResource> surface     = nullptr;
        SP<CTexture>           texture     = nullptr;
        bool                   mainSurface = true;
        double                 w = 0, h = 0;
        int                    rounding      = 0;
        bool                   dontRound     = true;
        float                  roundingPower = 2.0F;
        bool                   decorate      = false;
        float                  alpha = 1.F, fadeAlpha = 1.F;
        bool                   blur                  = false;
        bool                   blockBlurOptimization = false;

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

        bool     captureWrites = true;
    };

    CSurfacePassElement(const SRenderData& data);
    virtual ~CSurfacePassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual void                discard();
    CRegion                     visibleRegion(bool& cancel);

    virtual const char*         passName() {
        return "CSurfacePassElement";
    }

  private:
    SRenderData m_data;

    CBox        getTexBox();

    friend class CRenderPass;
};
