#pragma once

#include "./math/Math.hpp"

struct SCurrentRenderData;

class CMonitorZoomController {
  public:
    void  applyZoomTransform(CBox& monbox, const SCurrentRenderData& m_renderData);

    CBox  m_camera;
    bool  m_resetCameraState = true;
    float m_lastZoomLevel    = 1.0f;
    bool  m_padCamEdges      = true;

  private:
    void zoomWithDetachedCamera(CBox& result, const SCurrentRenderData& m_renderData);
};
