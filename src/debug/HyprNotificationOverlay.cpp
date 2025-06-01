#include <numeric>
#include <pango/pangocairo.h>
#include "HyprNotificationOverlay.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../render/pass/TexPassElement.hpp"

#include "../managers/AnimationManager.hpp"
#include "../managers/HookSystemManager.hpp"
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

CHyprNotificationOverlay::CHyprNotificationOverlay() {
    static auto P = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        if (m_notifications.empty())
            return;

        g_pHyprRenderer->damageBox(m_lastDamage);
    });

    m_texture = makeShared<CTexture>();
}

CHyprNotificationOverlay::~CHyprNotificationOverlay() {
    if (m_cairo)
        cairo_destroy(m_cairo);
    if (m_cairoSurface)
        cairo_surface_destroy(m_cairoSurface);
}

void CHyprNotificationOverlay::addNotification(const std::string& text, const CHyprColor& color, const float timeMs, const eIcons icon, const float fontSize) {
    const auto PNOTIF = m_notifications.emplace_back(makeUnique<SNotification>()).get();

    PNOTIF->text  = icon != eIcons::ICON_NONE ? " " + text /* tiny bit of padding otherwise icon touches text */ : text;
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
        const int AMT = std::min(amount, static_cast<int>(m_notifications.size()));

        for (int i = 0; i < AMT; ++i) {
            m_notifications.erase(m_notifications.begin());
        }
    }
}

CBox CHyprNotificationOverlay::drawNotifications(PHLMONITOR pMonitor) {
    static constexpr auto ANIM_DURATION_MS   = 600.0;
    static constexpr auto ANIM_LAG_MS        = 100.0;
    static constexpr auto NOTIF_LEFTBAR_SIZE = 5.0;
    static constexpr auto ICON_PAD           = 3.0;
    static constexpr auto ICON_SCALE         = 0.9;
    static constexpr auto GRADIENT_SIZE      = 60.0;

    float                 offsetY  = 10;
    float                 maxWidth = 0;

    const auto            SCALE   = pMonitor->m_scale;
    const auto            MONSIZE = pMonitor->m_transformedSize;

    static auto           fontFamily = CConfigValue<std::string>("misc:font_family");

    PangoLayout*          layout  = pango_cairo_create_layout(m_cairo);
    PangoFontDescription* pangoFD = pango_font_description_new();

    pango_font_description_set_family(pangoFD, (*fontFamily).c_str());
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);

    const auto iconBackendID = iconBackendFromLayout(layout);
    const auto PBEZIER       = g_pAnimationManager->getBezier("default");

    for (auto const& notif : m_notifications) {
        const auto ICONPADFORNOTIF = notif->icon == ICON_NONE ? 0 : ICON_PAD;
        const auto FONTSIZE        = std::clamp((int)(notif->fontSize * ((pMonitor->m_pixelSize.x * SCALE) / 1920.f)), 8, 40);

        // first rect (bg, col)
        const float FIRSTRECTANIMP =
            (notif->started.getMillis() > (ANIM_DURATION_MS - ANIM_LAG_MS) ?
                 (notif->started.getMillis() > notif->timeMs - (ANIM_DURATION_MS - ANIM_LAG_MS) ? notif->timeMs - notif->started.getMillis() : (ANIM_DURATION_MS - ANIM_LAG_MS)) :
                 notif->started.getMillis()) /
            (ANIM_DURATION_MS - ANIM_LAG_MS);

        const float FIRSTRECTPERC = FIRSTRECTANIMP >= 0.99f ? 1.f : PBEZIER->getYForPoint(FIRSTRECTANIMP);

        // second rect (fg, black)
        const float SECONDRECTANIMP = (notif->started.getMillis() > ANIM_DURATION_MS ?
                                           (notif->started.getMillis() > notif->timeMs - ANIM_DURATION_MS ? notif->timeMs - notif->started.getMillis() : ANIM_DURATION_MS) :
                                           notif->started.getMillis()) /
            ANIM_DURATION_MS;

        const float SECONDRECTPERC = SECONDRECTANIMP >= 0.99f ? 1.f : PBEZIER->getYForPoint(SECONDRECTANIMP);

        // third rect (horiz, col)
        const float THIRDRECTPERC = notif->started.getMillis() / notif->timeMs;

        // get text size
        const auto ICON      = ICONS_ARRAY[iconBackendID][notif->icon];
        const auto ICONCOLOR = ICONS_COLORS[notif->icon];

        int        iconW = 0, iconH = 0;
        pango_font_description_set_absolute_size(pangoFD, PANGO_SCALE * FONTSIZE * ICON_SCALE);
        pango_layout_set_font_description(layout, pangoFD);
        pango_layout_set_text(layout, ICON.c_str(), -1);
        pango_layout_get_size(layout, &iconW, &iconH);
        iconW /= PANGO_SCALE;
        iconH /= PANGO_SCALE;

        int textW = 0, textH = 0;
        pango_font_description_set_absolute_size(pangoFD, PANGO_SCALE * FONTSIZE);
        pango_layout_set_font_description(layout, pangoFD);
        pango_layout_set_text(layout, notif->text.c_str(), -1);
        pango_layout_get_size(layout, &textW, &textH);
        textW /= PANGO_SCALE;
        textH /= PANGO_SCALE;

        const auto NOTIFSIZE = Vector2D{textW + 20.0 + iconW + 2 * ICONPADFORNOTIF, textH + 10.0};

        // draw rects
        cairo_set_source_rgba(m_cairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);
        cairo_rectangle(m_cairo, MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY, (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_cairo);

        cairo_set_source_rgb(m_cairo, 0.f, 0.f, 0.f);
        cairo_rectangle(m_cairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC, offsetY, NOTIFSIZE.x * SECONDRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_cairo);

        cairo_set_source_rgba(m_cairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);
        cairo_rectangle(m_cairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + 3, offsetY + NOTIFSIZE.y - 4, THIRDRECTPERC * (NOTIFSIZE.x - 6), 2);
        cairo_fill(m_cairo);

        // draw gradient
        if (notif->icon != ICON_NONE) {
            cairo_pattern_t* pattern;
            pattern = cairo_pattern_create_linear(MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY,
                                                  MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC + GRADIENT_SIZE, offsetY);
            cairo_pattern_add_color_stop_rgba(pattern, 0, ICONCOLOR.r, ICONCOLOR.g, ICONCOLOR.b, ICONCOLOR.a / 3.0);
            cairo_pattern_add_color_stop_rgba(pattern, 1, ICONCOLOR.r, ICONCOLOR.g, ICONCOLOR.b, 0);
            cairo_rectangle(m_cairo, MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY, GRADIENT_SIZE, NOTIFSIZE.y);
            cairo_set_source(m_cairo, pattern);
            cairo_fill(m_cairo);
            cairo_pattern_destroy(pattern);

            // draw icon
            cairo_set_source_rgb(m_cairo, 1.f, 1.f, 1.f);
            cairo_move_to(m_cairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + NOTIF_LEFTBAR_SIZE + ICONPADFORNOTIF - 1, offsetY - 2 + std::round((NOTIFSIZE.y - iconH) / 2.0));
            pango_layout_set_text(layout, ICON.c_str(), -1);
            pango_cairo_show_layout(m_cairo, layout);
        }

        // draw text
        cairo_set_source_rgb(m_cairo, 1.f, 1.f, 1.f);
        cairo_move_to(m_cairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + NOTIF_LEFTBAR_SIZE + iconW + 2 * ICONPADFORNOTIF, offsetY - 2 + std::round((NOTIFSIZE.y - textH) / 2.0));
        pango_layout_set_text(layout, notif->text.c_str(), -1);
        pango_cairo_show_layout(m_cairo, layout);

        // adjust offset and move on
        offsetY += NOTIFSIZE.y + 10;

        if (maxWidth < NOTIFSIZE.x)
            maxWidth = NOTIFSIZE.x;
    }

    pango_font_description_free(pangoFD);
    g_object_unref(layout);

    // cleanup notifs
    std::erase_if(m_notifications, [](const auto& notif) { return notif->started.getMillis() > notif->timeMs; });

    return CBox{(int)(pMonitor->m_position.x + pMonitor->m_size.x - maxWidth - 20), (int)pMonitor->m_position.y, (int)maxWidth + 20, (int)offsetY + 10};
}

void CHyprNotificationOverlay::draw(PHLMONITOR pMonitor) {

    const auto MONSIZE = pMonitor->m_transformedSize;

    if (m_lastMonitor != pMonitor || m_lastSize != MONSIZE || !m_cairo || !m_cairoSurface) {

        if (m_cairo && m_cairoSurface) {
            cairo_destroy(m_cairo);
            cairo_surface_destroy(m_cairoSurface);
        }

        m_cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, MONSIZE.x, MONSIZE.y);
        m_cairo        = cairo_create(m_cairoSurface);
        m_lastMonitor  = pMonitor;
        m_lastSize     = MONSIZE;
    }

    // Draw the notifications
    if (m_notifications.empty())
        return;

    // Render to the monitor

    // clear the pixmap
    cairo_save(m_cairo);
    cairo_set_operator(m_cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cairo);
    cairo_restore(m_cairo);

    cairo_surface_flush(m_cairoSurface);

    CBox damage = drawNotifications(pMonitor);

    g_pHyprRenderer->damageBox(damage);
    g_pHyprRenderer->damageBox(m_lastDamage);

    g_pCompositor->scheduleFrameForMonitor(pMonitor);

    m_lastDamage = damage;

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(m_cairoSurface);
    m_texture->allocate();
    glBindTexture(GL_TEXTURE_2D, m_texture->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MONSIZE.x, MONSIZE.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    CTexPassElement::SRenderData data;
    data.tex = m_texture;
    data.box = {0, 0, MONSIZE.x, MONSIZE.y};
    data.a   = 1.F;

    g_pHyprRenderer->m_renderPass.add(makeShared<CTexPassElement>(data));
}

bool CHyprNotificationOverlay::hasAny() {
    return !m_notifications.empty();
}
