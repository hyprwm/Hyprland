#pragma once

#include "../defines.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Texture.hpp"
#include "../SharedDefs.hpp"

#include <deque>

#include <cairo/cairo.h>

enum eIconBackend
{
    ICONS_BACKEND_NONE = 0,
    ICONS_BACKEND_NF,
    ICONS_BACKEND_FA
};

static const std::array<std::array<std::string, ICON_NONE + 1>, 3 /* backends */> ICONS_ARRAY = {
    std::array<std::string, ICON_NONE + 1>{"[!]", "[i]", "[Hint]", "[Err]", "[?]", "[ok]", ""}, std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", "󰸞", ""},
    std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", ""}};
static const std::array<CColor, ICON_NONE + 1> ICONS_COLORS = {CColor{255.0 / 255.0, 204 / 255.0, 102 / 255.0, 1.0},
                                                               CColor{128 / 255.0, 255 / 255.0, 255 / 255.0, 1.0},
                                                               CColor{179 / 255.0, 255 / 255.0, 204 / 255.0, 1.0},
                                                               CColor{255 / 255.0, 77 / 255.0, 77 / 255.0, 1.0},
                                                               CColor{255 / 255.0, 204 / 255.0, 153 / 255.0, 1.0},
                                                               CColor{128 / 255.0, 255 / 255.0, 128 / 255.0, 1.0},
                                                               CColor{0, 0, 0, 1.0}};

struct SNotification {
    std::string text = "";
    CColor      color;
    CTimer      started;
    float       timeMs = 0;
    eIcons      icon   = ICON_NONE;
};

class CHyprNotificationOverlay {
  public:
    CHyprNotificationOverlay();

    void draw(CMonitor* pMonitor);
    void addNotification(const std::string& text, const CColor& color, const float timeMs, const eIcons icon = ICON_NONE);

  private:
    wlr_box                                    drawNotifications(CMonitor* pMonitor);
    wlr_box                                    m_bLastDamage;

    std::deque<std::unique_ptr<SNotification>> m_dNotifications;

    cairo_surface_t*                           m_pCairoSurface = nullptr;
    cairo_t*                                   m_pCairo        = nullptr;

    CMonitor*                                  m_pLastMonitor = nullptr;

    CTexture                                   m_tTexture;

    eIconBackend                               m_eIconBackend   = ICONS_BACKEND_NONE;
    std::string                                m_szIconFontName = "Sans";
};

inline std::unique_ptr<CHyprNotificationOverlay> g_pHyprNotificationOverlay;