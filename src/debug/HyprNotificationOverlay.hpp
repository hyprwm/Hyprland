#pragma once

#include "../defines.hpp"
#include "../helpers/Timer.hpp"
#include "../render/Texture.hpp"
#include "../SharedDefs.hpp"

#include <vector>

#include <cairo/cairo.h>

enum eIconBackend : uint8_t {
    ICONS_BACKEND_NONE = 0,
    ICONS_BACKEND_NF,
    ICONS_BACKEND_FA
};

static const std::array<std::array<std::string, ICON_NONE + 1>, 3 /* backends */> ICONS_ARRAY = {
    std::array<std::string, ICON_NONE + 1>{"[!]", "[i]", "[Hint]", "[Err]", "[?]", "[ok]", ""},
    std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", "󰸞", ""}, std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", ""}};
static const std::array<CHyprColor, ICON_NONE + 1> ICONS_COLORS = {CHyprColor{255.0 / 255.0, 204 / 255.0, 102 / 255.0, 1.0},
                                                                   CHyprColor{128 / 255.0, 255 / 255.0, 255 / 255.0, 1.0},
                                                                   CHyprColor{179 / 255.0, 255 / 255.0, 204 / 255.0, 1.0},
                                                                   CHyprColor{255 / 255.0, 77 / 255.0, 77 / 255.0, 1.0},
                                                                   CHyprColor{255 / 255.0, 204 / 255.0, 153 / 255.0, 1.0},
                                                                   CHyprColor{128 / 255.0, 255 / 255.0, 128 / 255.0, 1.0},
                                                                   CHyprColor{0, 0, 0, 1.0}};

struct SNotification {
    std::string text = "";
    CHyprColor  color;
    CTimer      started;
    float       timeMs   = 0;
    eIcons      icon     = ICON_NONE;
    float       fontSize = 13.f;
};

class CHyprNotificationOverlay {
  public:
    CHyprNotificationOverlay();
    ~CHyprNotificationOverlay();

    void draw(PHLMONITOR pMonitor);
    void addNotification(const std::string& text, const CHyprColor& color, const float TIMEMS, const eIcons ICON = ICON_NONE, const float FONTSIZE = 13.f);
    void dismissNotifications(const int AMOUNT);
    bool hasAny();

  private:
    CBox                           drawNotifications(PHLMONITOR pMonitor);
    CBox                           m_bLastDamage;

    std::vector<UP<SNotification>> m_vNotifications;

    cairo_surface_t*               m_pCairoSurface = nullptr;
    cairo_t*                       m_pCairo        = nullptr;

    PHLMONITORREF                  m_pLastMonitor;
    Vector2D                       m_vecLastSize = Vector2D(-1, -1);

    SP<CTexture>                   m_pTexture;
};

inline UP<CHyprNotificationOverlay> g_pHyprNotificationOverlay;
