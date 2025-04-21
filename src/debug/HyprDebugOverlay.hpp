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
    std::deque<float>                              m_lastFrametimes;
    std::deque<float>                              m_lastRenderTimes;
    std::deque<float>                              m_lastRenderTimesNoOverlay;
    std::deque<float>                              m_lastAnimationTicks;
    std::chrono::high_resolution_clock::time_point m_lastFrame;
    PHLMONITORREF                                  m_monitor;
    CBox                                           m_lastDrawnBox;

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
    std::map<PHLMONITORREF, CHyprMonitorDebugOverlay> m_monitorOverlays;

    cairo_surface_t*                                  m_cairoSurface = nullptr;
    cairo_t*                                          m_cairo        = nullptr;

    SP<CTexture>                                      m_texture;

    friend class CHyprMonitorDebugOverlay;
    friend class CHyprRenderer;
};

inline UP<CHyprDebugOverlay> g_pDebugOverlay;
