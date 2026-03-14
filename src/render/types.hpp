#pragma once

#include "Framebuffer.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/cm/ColorManagement.hpp"
#include "../protocols/core/Compositor.hpp"
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>

namespace Render {
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
        RPT_MIRROR,
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

    struct SRenderData {
        // can be private
        Hyprutils::Math::Mat3x3 targetProjection;

        // ----------------------

        // used by public
        Hyprutils::Math::Vector2D fbSize = {-1, -1};
        PHLMONITORREF             pMonitor;

        eRenderProjectionType     projectionType = RPT_MONITOR;

        SP<IFramebuffer>          currentFB = nullptr; // current rendering to
        SP<IFramebuffer>          mainFB    = nullptr; // main to render to
        SP<IFramebuffer>          outFB     = nullptr; // out to render to (if offloaded, etc)
        SP<IFramebuffer>          prevFB    = nullptr; // out to render to (if offloaded, etc)

        CRegion                   damage;
        CRegion                   finalDamage; // damage used for funal off -> main

        SRenderModifData          renderModif;
        float                     mouseZoomFactor    = 1.f;
        bool                      mouseZoomUseMouse  = true; // true by default
        bool                      useNearestNeighbor = false;
        bool                      blockScreenShader  = false;

        Vector2D                  primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        Vector2D                  primarySurfaceUVBottomRight = Vector2D(-1, -1);

        // TODO remove and pass directly
        CBox                   clipBox = {}; // scaled coordinates
        PHLWINDOWREF           currentWindow;
        WP<CWLSurfaceResource> surface;

        bool                   transformDamage = true;
        bool                   noSimplify      = false;
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
        float                                maxLuminance    = 80;
        float                                dstMaxLuminance = 80;
        std::array<std::array<double, 3>, 3> dstPrimaries2XYZ;
        bool                                 needsSDRmod             = false;
        float                                sdrSaturation           = 1.0;
        float                                sdrBrightnessMultiplier = 1.0;
    };
}