#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Texture.hpp"
#include <deque>
#include <cairo/cairo.h>
#include <unordered_map>

class CHyprRenderer;

class CHyprMonitorDebugOverlay {
  public:
    int  draw(int offset);

    void renderData(PHLMONITOR pMonitor, float µs);
    void renderDataNoOverlay(PHLMONITOR pMonitor, float µs);
    void frameData(PHLMONITOR pMonitor);

  private:
    std::deque<float>                              m_dLastFrametimes;
    std::deque<float>                              m_dLastRenderTimes;
    std::deque<float>                              m_dLastRenderTimesNoOverlay;
    std::deque<float>                              m_dLastAnimationTicks;
    std::chrono::high_resolution_clock::time_point m_tpLastFrame;
    PHLMONITORREF                                  m_pMonitor;
    CBox                                           m_wbLastDrawnBox;

    friend class CHyprRenderer;
};

class CHyprDebugOverlay {
  public:
    CHyprDebugOverlay();
    void draw();
    void renderData(PHLMONITOR, float µs);
    void renderDataNoOverlay(PHLMONITOR, float µs);
    void frameData(PHLMONITOR);

  private:
    std::unordered_map<PHLMONITOR, CHyprMonitorDebugOverlay> m_mMonitorOverlays;

    cairo_surface_t*                                         m_pCairoSurface = nullptr;
    cairo_t*                                                 m_pCairo        = nullptr;

    SP<CTexture>                                             m_pTexture;

    friend class CHyprMonitorDebugOverlay;
    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprDebugOverlay> g_pDebugOverlay;