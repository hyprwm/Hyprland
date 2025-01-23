#pragma once

#include "../defines.hpp"
#include "../render/Texture.hpp"
#include <cairo/cairo.h>
#include <map>
#include <deque>

class CHyprRenderer;

class CHyprMonitorDebugOverlay {
  public:
    int  draw(int offset);

    void renderData(PHLMONITOR pMonitor, float durationUs);
    void renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs);
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
    void renderData(PHLMONITOR, float durationUs);
    void renderDataNoOverlay(PHLMONITOR, float durationUs);
    void frameData(PHLMONITOR);

  private:
    std::map<PHLMONITORREF, CHyprMonitorDebugOverlay> m_mMonitorOverlays;

    cairo_surface_t*                                  m_pCairoSurface = nullptr;
    cairo_t*                                          m_pCairo        = nullptr;

    SP<CTexture>                                      m_pTexture;

    friend class CHyprMonitorDebugOverlay;
    friend class CHyprRenderer;
};

inline UP<CHyprDebugOverlay> g_pDebugOverlay;
