#pragma once

#include "../helpers/math/Math.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace Render {
    class CRenderingContext;
}

namespace Monitor {
    class CMonitorZoomController {
      public:
        bool m_resetCameraState = true;

        void pinAnchor(const Vector2D& anchor);
        void clearAnchor();

        void applyZoomTransform(CBox& monbox, const Render::CRenderingContext& context);
        bool shouldDamageEntire(float zoomLevel);

      private:
        void     zoomWithDetachedCamera(CBox& result, const Render::CRenderingContext& context);
        Vector2D getAnchor(const PHLMONITORREF& monitor);

        CBox     m_camera;
        Vector2D m_pinnedAnchor          = {};
        float    m_lastZoomLevel         = 1.0f;
        float    m_lastRenderedZoomLevel = 1.0f;
        bool     m_padCamEdges           = true;
        bool     m_anchorPinned          = false;
    };
}
