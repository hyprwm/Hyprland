#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Texture.hpp"
#include <deque>
#include <cairo/cairo.h>
#include <unordered_map>

class CHyprMonitorDebugOverlay {
  public:
    int  draw(int offset);

    void renderData(CMonitor* pMonitor, float µs);
    void renderDataNoOverlay(CMonitor* pMonitor, float µs);
    void frameData(CMonitor* pMonitor);

  private:
    std::deque<float>                              m_dLastFrametimes;
    std::deque<float>                              m_dLastRenderTimes;
    std::deque<float>                              m_dLastRenderTimesNoOverlay;
    std::chrono::high_resolution_clock::time_point m_tpLastFrame;
    CMonitor*                                      m_pMonitor = nullptr;
    wlr_box                                        m_wbLastDrawnBox;
};

class CHyprDebugOverlay {
  public:
    void draw();
    void renderData(CMonitor*, float µs);
    void renderDataNoOverlay(CMonitor*, float µs);
    void frameData(CMonitor*);

  private:
    std::unordered_map<CMonitor*, CHyprMonitorDebugOverlay> m_mMonitorOverlays;

    cairo_surface_t*                                        m_pCairoSurface = nullptr;
    cairo_t*                                                m_pCairo        = nullptr;

    CTexture                                                m_tTexture;

    friend class CHyprMonitorDebugOverlay;
};

inline std::unique_ptr<CHyprDebugOverlay> g_pDebugOverlay;