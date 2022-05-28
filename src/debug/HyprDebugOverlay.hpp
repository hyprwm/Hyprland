#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Texture.hpp"
#include <deque>
#include <cairo/cairo.h>
#include <unordered_map>

class CHyprMonitorDebugOverlay {
public:
    int draw(int offset);

    void renderData(SMonitor* pMonitor, float µs);
    void renderDataNoOverlay(SMonitor* pMonitor, float µs);
    void frameData(SMonitor* pMonitor);

private:
    std::deque<float> m_dLastFrametimes;
    std::deque<float> m_dLastRenderTimes;
    std::deque<float> m_dLastRenderTimesNoOverlay;
    std::chrono::high_resolution_clock::time_point m_tpLastFrame;
    SMonitor* m_pMonitor = nullptr;
    wlr_box m_wbLastDrawnBox;
};

class CHyprDebugOverlay {
public:

    void draw();
    void renderData(SMonitor*, float µs);
    void renderDataNoOverlay(SMonitor*, float µs);
    void frameData(SMonitor*);

private:

    std::unordered_map<SMonitor*, CHyprMonitorDebugOverlay> m_mMonitorOverlays;

    cairo_surface_t* m_pCairoSurface = nullptr;
    cairo_t* m_pCairo = nullptr;

    CTexture m_tTexture;

    friend class CHyprMonitorDebugOverlay;
};

inline std::unique_ptr<CHyprDebugOverlay> g_pDebugOverlay;