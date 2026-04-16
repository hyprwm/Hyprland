#pragma once

#include "../defines.hpp"
#include "../helpers/time/Timer.hpp"
#include "../render/Texture.hpp"
#include "../SharedDefs.hpp"

#include <vector>

enum eIconBackend : uint8_t {
    ICONS_BACKEND_NONE = 0,
    ICONS_BACKEND_NF,
    ICONS_BACKEND_FA
};

static const std::array<std::array<std::string, ICON_NONE + 1>, 3 /* backends */> ICONS_ARRAY = {
    std::array<std::string, ICON_NONE + 1>{"[!]", "[i]", "[Hint]", "[Err]", "[?]", "[ok]", ""},
    std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", "󰸞", ""}, std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", ""}};
static const std::array<CHyprColor, ICON_NONE + 1> ICONS_COLORS = {CHyprColor{1.0, 204 / 255.0, 102 / 255.0, 1.0},
                                                                   CHyprColor{128 / 255.0, 255 / 255.0, 255 / 255.0, 1.0},
                                                                   CHyprColor{179 / 255.0, 255 / 255.0, 204 / 255.0, 1.0},
                                                                   CHyprColor{255 / 255.0, 77 / 255.0, 77 / 255.0, 1.0},
                                                                   CHyprColor{255 / 255.0, 204 / 255.0, 153 / 255.0, 1.0},
                                                                   CHyprColor{128 / 255.0, 255 / 255.0, 128 / 255.0, 1.0},
                                                                   CHyprColor{0, 0, 0, 1.0}};

struct SNotification {
    struct SRenderCache {
        SP<Render::ITexture> textTex;
        SP<Render::ITexture> iconTex;

        Vector2D             textSize = {};
        Vector2D             iconSize = {};

        PHLMONITORREF        monitor;
        std::string          fontFamily;
        int                  fontSizePx  = -1;
        eIconBackend         iconBackend = ICONS_BACKEND_NONE;
    } cache;

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
    void addNotification(const std::string& text, const CHyprColor& color, const float timeMs, const eIcons icon = ICON_NONE, const float fontSize = 13.f);
    void dismissNotifications(const int amount);
    bool hasAny();

  private:
    void                           ensureNotificationCache(SNotification& notif, PHLMONITOR pMonitor, const std::string& fontFamily);
    eIconBackend                   iconBackendForFont(const std::string& fontFamily);
    CBox                           drawNotifications(PHLMONITOR pMonitor);
    CBox                           m_lastDamage;

    std::vector<UP<SNotification>> m_notifications;

    std::string                    m_cachedIconBackendFontFamily;
    eIconBackend                   m_cachedIconBackend = ICONS_BACKEND_NONE;
    bool                           m_iconBackendValid  = false;
};

inline UP<CHyprNotificationOverlay> g_pHyprNotificationOverlay;
