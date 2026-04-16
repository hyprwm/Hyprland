#include <algorithm>
#include <cmath>
#include <numeric>
#include <pango/pangocairo.h>
#include "HyprNotificationOverlay.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../render/pass/RectPassElement.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../event/EventBus.hpp"

#include "../managers/animation/AnimationManager.hpp"
#include "../render/Renderer.hpp"

static inline auto iconBackendFromLayout(PangoLayout* layout) {
    // preference: Nerd > FontAwesome > text
    auto eIconBackendChecks = std::array<eIconBackend, 2>{ICONS_BACKEND_NF, ICONS_BACKEND_FA};
    for (auto iconID : eIconBackendChecks) {
        auto iconsText = std::ranges::fold_left(ICONS_ARRAY[iconID], std::string(), std::plus<>());
        pango_layout_set_text(layout, iconsText.c_str(), -1);
        if (pango_layout_get_unknown_glyphs_count(layout) == 0)
            return iconID;
    }
    return ICONS_BACKEND_NONE;
}

static constexpr auto ANIM_DURATION_MS   = 600.F;
static constexpr auto ANIM_LAG_MS        = 100.F;
static constexpr auto NOTIF_LEFTBAR_SIZE = 5.F;
static constexpr auto NOTIF_PAD_X        = 20.F;
static constexpr auto NOTIF_PAD_Y        = 10.F;
static constexpr auto NOTIF_OFFSET_Y     = 10.F;
static constexpr auto NOTIF_GAP_Y        = 10.F;
static constexpr auto NOTIF_DAMAGE_PAD_X = 20.F;
static constexpr auto ICON_PAD           = 3.F;
static constexpr auto ICON_SCALE         = 0.9F;

CHyprNotificationOverlay::CHyprNotificationOverlay() {
    static auto P = Event::bus()->m_events.monitor.focused.listen([&](PHLMONITOR mon) {
        if (m_notifications.empty())
            return;

        g_pHyprRenderer->damageBox(m_lastDamage);
    });
}

CHyprNotificationOverlay::~CHyprNotificationOverlay() = default;

eIconBackend CHyprNotificationOverlay::iconBackendForFont(const std::string& fontFamily) {
    if (m_iconBackendValid && m_cachedIconBackendFontFamily == fontFamily)
        return m_cachedIconBackend;

    auto* cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto* cairoCtx     = cairo_create(cairoSurface);
    auto* layout       = pango_cairo_create_layout(cairoCtx);
    auto* pangoFD      = pango_font_description_new();

    pango_font_description_set_family(pangoFD, fontFamily.c_str());
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, pangoFD);

    m_cachedIconBackend           = iconBackendFromLayout(layout);
    m_cachedIconBackendFontFamily = fontFamily;
    m_iconBackendValid            = true;

    pango_font_description_free(pangoFD);
    g_object_unref(layout);
    cairo_destroy(cairoCtx);
    cairo_surface_destroy(cairoSurface);

    return m_cachedIconBackend;
}

void CHyprNotificationOverlay::ensureNotificationCache(SNotification& notif, PHLMONITOR pMonitor, const std::string& fontFamily) {
    const auto iconBackend = iconBackendForFont(fontFamily);
    const auto fontSizePx  = std::clamp(sc<int>(notif.fontSize * ((pMonitor->m_pixelSize.x * pMonitor->m_scale) / 1920.f)), 8, 40);

    const bool cacheValid = notif.cache.monitor == pMonitor && notif.cache.fontFamily == fontFamily && notif.cache.fontSizePx == fontSizePx &&
        notif.cache.iconBackend == iconBackend && notif.cache.textTex && (notif.icon == ICON_NONE || notif.cache.iconTex);

    if (cacheValid)
        return;

    notif.cache             = {};
    notif.cache.monitor     = pMonitor;
    notif.cache.fontFamily  = fontFamily;
    notif.cache.fontSizePx  = fontSizePx;
    notif.cache.iconBackend = iconBackend;

    notif.cache.textTex = g_pHyprRenderer->renderText(notif.text, CHyprColor{1.F, 1.F, 1.F, 1.F}, fontSizePx, false, fontFamily);
    if (notif.cache.textTex)
        notif.cache.textSize = notif.cache.textTex->m_size;

    if (notif.icon != ICON_NONE) {
        const auto iconGlyph  = ICONS_ARRAY[iconBackend][notif.icon];
        const auto iconSizePx = std::max(8, sc<int>(std::round(fontSizePx * ICON_SCALE)));

        notif.cache.iconTex = g_pHyprRenderer->renderText(iconGlyph, CHyprColor{1.F, 1.F, 1.F, 1.F}, iconSizePx, false, fontFamily);
        if (notif.cache.iconTex)
            notif.cache.iconSize = notif.cache.iconTex->m_size;
    }
}

void CHyprNotificationOverlay::addNotification(const std::string& text, const CHyprColor& color, const float timeMs, const eIcons icon, const float fontSize) {
    const auto PNOTIF = m_notifications.emplace_back(makeUnique<SNotification>()).get();

    PNOTIF->text  = text;
    PNOTIF->color = color == CHyprColor(0) ? ICONS_COLORS[icon] : color;
    PNOTIF->started.reset();
    PNOTIF->timeMs   = timeMs;
    PNOTIF->icon     = icon;
    PNOTIF->fontSize = fontSize;

    for (auto const& m : g_pCompositor->m_monitors) {
        g_pCompositor->scheduleFrameForMonitor(m);
    }
}

void CHyprNotificationOverlay::dismissNotifications(const int amount) {
    if (amount == -1)
        m_notifications.clear();
    else {
        const int AMT = std::min(amount, sc<int>(m_notifications.size()));

        for (int i = 0; i < AMT; ++i) {
            m_notifications.erase(m_notifications.begin());
        }
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        g_pCompositor->scheduleFrameForMonitor(m);
    }
}

CBox CHyprNotificationOverlay::drawNotifications(PHLMONITOR pMonitor) {
    float       offsetY  = NOTIF_OFFSET_Y;
    float       maxWidth = 0;

    const auto  MONSIZE = pMonitor->m_transformedSize;

    static auto fontFamily = CConfigValue<std::string>("misc:font_family");
    const auto  PBEZIER    = g_pAnimationManager->getBezier("default");

    for (auto const& notif : m_notifications) {
        ensureNotificationCache(*notif, pMonitor, *fontFamily);

        const auto  ICONPADFORNOTIF = notif->icon == ICON_NONE ? 0.F : ICON_PAD;
        const auto  ICONW           = notif->cache.iconSize.x;
        const auto  ICONH           = notif->cache.iconSize.y;
        const auto  TEXTW           = notif->cache.textSize.x;
        const auto  TEXTH           = notif->cache.textSize.y;
        const auto  NOTIFSIZE       = Vector2D{TEXTW + NOTIF_PAD_X + ICONW + 2 * ICONPADFORNOTIF, std::max(TEXTH, ICONH) + NOTIF_PAD_Y};

        const float elapsed = notif->started.getMillis();
        const float lifeMs  = std::max(notif->timeMs, 1.F);

        // first rect (bg, col)
        const float FIRSTRECTANIMP = std::clamp(
            (elapsed > (ANIM_DURATION_MS - ANIM_LAG_MS) ? (elapsed > lifeMs - (ANIM_DURATION_MS - ANIM_LAG_MS) ? lifeMs - elapsed : (ANIM_DURATION_MS - ANIM_LAG_MS)) : elapsed) /
                (ANIM_DURATION_MS - ANIM_LAG_MS),
            0.F, 1.F);

        const float FIRSTRECTPERC = FIRSTRECTANIMP >= 0.99f ? 1.f : PBEZIER->getYForPoint(FIRSTRECTANIMP);

        // second rect (fg, black)
        const float SECONDRECTANIMP =
            std::clamp((elapsed > ANIM_DURATION_MS ? (elapsed > lifeMs - ANIM_DURATION_MS ? lifeMs - elapsed : ANIM_DURATION_MS) : elapsed) / ANIM_DURATION_MS, 0.F, 1.F);

        const float SECONDRECTPERC = SECONDRECTANIMP >= 0.99f ? 1.f : PBEZIER->getYForPoint(SECONDRECTANIMP);

        // third rect (horiz, col)
        const float                 THIRDRECTPERC = std::clamp(elapsed / lifeMs, 0.F, 1.F);

        const float                 firstRectX = MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC;
        const float                 firstRectW = (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC;

        const float                 secondRectX = MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC;
        const float                 secondRectW = NOTIFSIZE.x * SECONDRECTPERC;

        CRectPassElement::SRectData bgData;
        bgData.box   = {firstRectX, offsetY, firstRectW, NOTIFSIZE.y};
        bgData.color = notif->color;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(bgData));

        CRectPassElement::SRectData fgData;
        fgData.box   = {secondRectX, offsetY, secondRectW, NOTIFSIZE.y};
        fgData.color = CHyprColor{0.F, 0.F, 0.F, 1.F};
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(fgData));

        CRectPassElement::SRectData progressData;
        progressData.box   = {secondRectX + 3, offsetY + NOTIFSIZE.y - 4, THIRDRECTPERC * std::max(0.0, NOTIFSIZE.x - 6.0), 2};
        progressData.color = notif->color;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(progressData));

        if (notif->icon != ICON_NONE && notif->cache.iconTex) {
            CTexPassElement::SRenderData iconData;
            iconData.tex = notif->cache.iconTex;
            iconData.box = {secondRectX + NOTIF_LEFTBAR_SIZE + ICONPADFORNOTIF - 1, offsetY - 2 + std::round((NOTIFSIZE.y - ICONH) / 2.0), ICONW, ICONH};
            iconData.a   = 1.F;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(iconData)));
        }

        if (notif->cache.textTex) {
            CTexPassElement::SRenderData textData;
            textData.tex = notif->cache.textTex;
            textData.box = {secondRectX + NOTIF_LEFTBAR_SIZE + ICONW + 2 * ICONPADFORNOTIF, offsetY - 2 + std::round((NOTIFSIZE.y - TEXTH) / 2.0), TEXTW, TEXTH};
            textData.a   = 1.F;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(textData)));
        }

        // adjust offset and move on
        offsetY += NOTIFSIZE.y + NOTIF_GAP_Y;

        if (maxWidth < NOTIFSIZE.x)
            maxWidth = NOTIFSIZE.x;
    }

    // cleanup notifs
    std::erase_if(m_notifications, [](const auto& notif) { return notif->started.getMillis() > notif->timeMs; });

    return CBox{sc<int>(pMonitor->m_position.x + pMonitor->m_size.x - maxWidth - NOTIF_DAMAGE_PAD_X), sc<int>(pMonitor->m_position.y), sc<int>(maxWidth + NOTIF_DAMAGE_PAD_X),
                sc<int>(offsetY + NOTIF_OFFSET_Y)};
}

void CHyprNotificationOverlay::draw(PHLMONITOR pMonitor) {
    // Draw the notifications
    if (m_notifications.empty()) {
        if (m_lastDamage.width > 0 && m_lastDamage.height > 0)
            g_pHyprRenderer->damageBox(m_lastDamage);
        m_lastDamage = {};
        return;
    }

    CBox damage = drawNotifications(pMonitor);

    g_pHyprRenderer->damageBox(damage);
    g_pHyprRenderer->damageBox(m_lastDamage);

    g_pCompositor->scheduleFrameForMonitor(pMonitor);

    m_lastDamage = damage;
}

bool CHyprNotificationOverlay::hasAny() {
    return !m_notifications.empty();
}
