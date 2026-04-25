#pragma once

#include "./math/Math.hpp"

namespace Render {
    struct SRenderData;
}

class CMonitorZoomController {
  public:
    bool m_resetCameraState = true;

    void applyZoomTransform(CBox& monbox, const Render::SRenderData& m_renderData);

  private:
    void  zoomWithDetachedCamera(CBox& result, const Render::SRenderData& m_renderData);

    CBox  m_camera;
    float m_lastZoomLevel = 1.0f;
    bool  m_padCamEdges   = true;
};
