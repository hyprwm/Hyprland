#pragma once

#include <functional>

#include "Framebuffer.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/cm/ColorManagement.hpp"
#include "../protocols/core/Compositor.hpp"
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>

struct SBackdropScope;

namespace Render {
    class CRenderPass;
    class IRenderbuffer;

    const std::vector<const char*> ASSET_PATHS = {
#ifdef DATAROOTDIR
        DATAROOTDIR,
#endif
        "/usr/share",
        "/usr/local/share",
    };

    enum eDamageTrackingModes : int8_t {
        DAMAGE_TRACKING_INVALID = -1,
        DAMAGE_TRACKING_NONE    = 0,
        DAMAGE_TRACKING_MONITOR,
        DAMAGE_TRACKING_FULL,
    };

    enum eRenderPassMode : uint8_t {
        RENDER_PASS_ALL = 0,
        RENDER_PASS_MAIN,
        RENDER_PASS_POPUP
    };

    enum eRenderMode : uint8_t {
        RENDER_MODE_NORMAL              = 0,
        RENDER_MODE_FULL_FAKE           = 1,
        RENDER_MODE_TO_BUFFER           = 2,
        RENDER_MODE_TO_BUFFER_READ_ONLY = 3,
    };

    struct SRenderWorkspaceUntilData {
        PHLLS     ls;
        PHLWINDOW w;
    };

    enum eRenderProjectionType : uint8_t {
        RPT_MONITOR,
        RPT_OUTPUT,
        RPT_FB,
        RPT_EXPORT,
    };

    struct SRenderModifData {
        enum eRenderModifType : uint8_t {
            RMOD_TYPE_SCALE,        /* scale by a float */
            RMOD_TYPE_SCALECENTER,  /* scale by a float from the center */
            RMOD_TYPE_TRANSLATE,    /* translate by a Vector2D */
            RMOD_TYPE_ROTATE,       /* rotate by a float in rad from top left */
            RMOD_TYPE_ROTATECENTER, /* rotate by a float in rad from center */
        };

        std::vector<std::pair<eRenderModifType, std::any>> modifs;

        void                                               applyToBox(Hyprutils::Math::CBox& box);
        void                                               applyToRegion(Hyprutils::Math::CRegion& rg);
        float                                              combinedScale();

        bool                                               enabled = true;
    };

    struct STFRange {
        float min = 0;
        float max = 80;
    };

    struct SCMSettings {
        NColorManagement::eTransferFunction  sourceTF = NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;
        NColorManagement::eTransferFunction  targetTF = NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;
        STFRange                             srcTFRange;
        STFRange                             dstTFRange;
        float                                srcRefLuminance = 80;
        float                                dstRefLuminance = 80;
        std::array<std::array<double, 3>, 3> convertMatrix;

        bool                                 needsTonemap    = false;
        int                                  tonemapMode     = 1; // 1 - default, 2 - clamp, 3 - limited
        float                                maxLuminance    = 80;
        float                                dstMaxLuminance = 80;
        std::array<std::array<double, 3>, 3> dstPrimaries2XYZ;
        bool                                 needsSDRmod             = false;
        float                                sdrSaturation           = 1.0;
        float                                sdrBrightnessMultiplier = 1.0;
    };

    struct SCMSettingsCacheEntry {
        uint64_t    srcDescId = 0, dstDescId = 0;
        void*       surfacePtr      = nullptr; // read-only!!
        bool        modifySDR       = false;
        float       sdrMinLuminance = -1.F;
        int         sdrMaxLuminance = -1;
        SCMSettings settings;
    };

    struct SCMSettingsCache {
        std::vector<SCMSettingsCacheEntry> entries;
    };

    struct SBackdropCapture {
        SP<SBackdropScope> scope;
        SP<IFramebuffer>   framebuffer;
    };

    class CRenderingContext {
      public:
        CRenderingContext(PHLMONITORREF sceneMonitor, CRenderPass& pass, PHLMONITORREF outputMonitor = {});
        CRenderingContext(const CRenderingContext& parent, CRenderPass& pass);
        CRenderingContext(const CRenderingContext& parent, CRenderPass& pass, PHLMONITORREF sceneMonitor);

        CRenderPass&                  renderPass() const;

        Mat3x3                        targetProjection;
        Vector2D                      fbSize = {-1, -1};

        PHLMONITORREF                 sceneMonitor;
        PHLMONITORREF                 outputMonitor;

        eRenderProjectionType         projectionType = RPT_MONITOR;

        SP<IFramebuffer>              currentFB;
        SP<IFramebuffer>              mainFB;
        SP<IFramebuffer>              outFB;

        CRegion                       damage;
        CRegion                       finalDamage;

        SRenderModifData              renderModif;
        float                         mouseZoomFactor    = 1.F;
        bool                          mouseZoomUseMouse  = true;
        bool                          useNearestNeighbor = false;
        bool                          blockScreenShader  = false;

        Vector2D                      primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        Vector2D                      primarySurfaceUVBottomRight = Vector2D(-1, -1);

        CBox                          clipBox;
        PHLWINDOWREF                  currentWindow;
        WP<CWLSurfaceResource>        surface;

        bool                          transformDamage            = true;
        bool                          noSimplify                 = false;
        bool                          renderingTransformedSource = false;

        PHLWORKSPACEREF               isolatedWorkspace;
        bool                          isolatedWorkspaceFullScene = false;
        bool                          blockSurfaceFeedback       = false;
        bool                          renderingSnapshot          = false;
        bool                          precomputeBlur             = false;
        bool                          updatesMonitorBlurState    = true;

        eRenderMode                   renderMode             = RENDER_MODE_NORMAL;
        bool                          fakeFrame              = false;
        bool                          offloadedFramebuffer   = false;
        bool                          applyFinalScreenShader = false;

        SP<Aquamarine::IBuffer>       buffer;
        SP<IRenderbuffer>             renderbuffer;

        SP<SCMSettingsCache>          cmSettingsCache;
        std::vector<SBackdropCapture> backdropCaptures;

      private:
        CRenderingContext(const CRenderingContext&)                             = default;
        CRenderingContext&                  operator=(const CRenderingContext&) = delete;

        std::reference_wrapper<CRenderPass> m_pass;
    };
}
