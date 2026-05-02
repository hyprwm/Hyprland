#pragma once

#include "./math/Math.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace Render {
    struct SRenderData;
}

class CMonitorZoomController {
  public:
    bool m_resetCameraState = true;

    void pinAnchor(const Vector2D& anchor);
    void clearAnchor();

    void applyZoomTransform(CBox& monbox, const Render::SRenderData& m_renderData);

  private:
    void     zoomWithDetachedCamera(CBox& result, const Render::SRenderData& m_renderData);
    Vector2D getAnchor(const PHLMONITORREF& monitor);

    CBox     m_camera;
    Vector2D m_pinnedAnchor  = {};
    float    m_lastZoomLevel = 1.0f;
    bool     m_padCamEdges   = true;
    bool     m_anchorPinned  = false;
};
