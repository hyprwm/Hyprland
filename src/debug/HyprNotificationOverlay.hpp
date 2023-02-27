#pragma once

#include "../defines.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Texture.hpp"

#include <deque>

#include <cairo/cairo.h>

struct SNotification {
    std::string text = "";
    CColor      color;
    CTimer      started;
    float       timeMs = 0;
};

class CHyprNotificationOverlay {
  public:
    CHyprNotificationOverlay();

    void draw(CMonitor* pMonitor);
    void addNotification(const std::string& text, const CColor& color, const float timeMs);

  private:
    wlr_box                                    drawNotifications(CMonitor* pMonitor);
    wlr_box                                    m_bLastDamage;

    std::deque<std::unique_ptr<SNotification>> m_dNotifications;

    cairo_surface_t*                           m_pCairoSurface = nullptr;
    cairo_t*                                   m_pCairo        = nullptr;

    CMonitor*                                  m_pLastMonitor = nullptr;

    CTexture                                   m_tTexture;
};

inline std::unique_ptr<CHyprNotificationOverlay> g_pHyprNotificationOverlay;