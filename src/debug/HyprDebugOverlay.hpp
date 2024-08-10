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

    void renderData(CMonitor* pMonitor, float durationUs);
    void renderDataNoOverlay(CMonitor* pMonitor, float durationUs);
    void frameData(CMonitor* pMonitor);

  private:
    std::deque<float>                              m_dLastFrametimes;
    std::deque<float>                              m_dLastRenderTimes;
    std::deque<float>                              m_dLastRenderTimesNoOverlay;
    std::deque<float>                              m_dLastAnimationTicks;
    std::chrono::high_resolution_clock::time_point m_tpLastFrame;
    CMonitor*                                      m_pMonitor = nullptr;
    CBox                                           m_wbLastDrawnBox;

    friend class CHyprRenderer;
};

class CHyprDebugOverlay {
  public:
    CHyprDebugOverlay();
    void draw();
    void renderData(CMonitor*, float durationUs);
    void renderDataNoOverlay(CMonitor*, float durationUs);
    void frameData(CMonitor*);

  private:
    std::unordered_map<CMonitor*, CHyprMonitorDebugOverlay> m_mMonitorOverlays;

    cairo_surface_t*                                        m_pCairoSurface = nullptr;
    cairo_t*                                                m_pCairo        = nullptr;

    SP<CTexture>                                            m_pTexture;

    friend class CHyprMonitorDebugOverlay;
    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprDebugOverlay> g_pDebugOverlay;