#pragma once

#include "./math/Math.hpp"

struct SCurrentRenderData;

class CMonitorZoomController {
  public:
    bool m_resetCameraState = true;

    void applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData);

  private:
    void  zoomWithDetachedCamera(CBox& result, const SCurrentRenderData& m_renderData);

    CBox  m_camera;
    float m_lastZoomLevel = 1.0f;
    bool  m_padCamEdges   = true;
};
