#include "HyprNotificationOverlay.hpp"
#include "../Compositor.hpp"
#include <pango/pangocairo.h>

CHyprNotificationOverlay::CHyprNotificationOverlay() {
    static auto P = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        if (m_dNotifications.size() == 0)
            return;

        g_pHyprRenderer->damageBox(&m_bLastDamage);
    });

    // check for the icon backend
    std::string fonts      = execAndGet("fc-list");
    std::string fontsLower = fonts;
    std::transform(fontsLower.begin(), fontsLower.end(), fontsLower.begin(), [&](char& i) { return std::tolower(i); });

    size_t index = 0;

    if (index = fontsLower.find("nerd"); index != std::string::npos) {
        m_eIconBackend = ICONS_BACKEND_NF;
    } else if (index = fontsLower.find("font awesome"); index != std::string::npos) {
        m_eIconBackend = ICONS_BACKEND_FA;
    } else if (index = fontsLower.find("fontawesome"); index != std::string::npos) {
        m_eIconBackend = ICONS_BACKEND_FA;
    } else {
        return;
    }

    const auto LASTNEWLINE = fonts.find_last_of('\n', index);
    const auto COLON       = fonts.find(':', LASTNEWLINE);
    const auto COMMA       = fonts.find(',', COLON);
    const auto NEWLINE     = fonts.find('\n', COLON);
    const auto LASTCHAR    = COMMA < NEWLINE ? COMMA : NEWLINE;

    m_szIconFontName = fonts.substr(COLON + 2, LASTCHAR - (COLON + 2));
}

CHyprNotificationOverlay::~CHyprNotificationOverlay() {
    if (m_pCairo)
        cairo_destroy(m_pCairo);
    if (m_pCairoSurface)
        cairo_surface_destroy(m_pCairoSurface);
}

void CHyprNotificationOverlay::addNotification(const std::string& text, const CColor& color, const float timeMs, const eIcons icon, const float fontSize) {
    const auto PNOTIF = m_dNotifications.emplace_back(std::make_unique<SNotification>()).get();

    PNOTIF->text  = text;
    PNOTIF->color = color == CColor(0) ? ICONS_COLORS[icon] : color;
    PNOTIF->started.reset();
    PNOTIF->timeMs   = timeMs;
    PNOTIF->icon     = icon;
    PNOTIF->fontSize = fontSize;

    for (auto& m : g_pCompositor->m_vMonitors) {
        g_pCompositor->scheduleFrameForMonitor(m.get());
    }
}

void CHyprNotificationOverlay::dismissNotifications(const int amount) {
    if (amount == -1)
        m_dNotifications.clear();
    else {
        const int AMT = std::min(amount, static_cast<int>(m_dNotifications.size()));

        for (int i = 0; i < AMT; ++i) {
            m_dNotifications.pop_front();
        }
    }
}

CBox CHyprNotificationOverlay::drawNotifications(CMonitor* pMonitor) {
    static constexpr auto ANIM_DURATION_MS   = 600.0;
    static constexpr auto ANIM_LAG_MS        = 100.0;
    static constexpr auto NOTIF_LEFTBAR_SIZE = 5.0;
    static constexpr auto ICON_PAD           = 3.0;
    static constexpr auto ICON_SCALE         = 0.9;
    static constexpr auto GRADIENT_SIZE      = 60.0;

    float                 offsetY  = 10;
    float                 maxWidth = 0;

    const auto            SCALE   = pMonitor->scale;
    const auto            MONSIZE = pMonitor->vecTransformedSize;

    PangoLayout*          layoutIcon = pango_cairo_create_layout(m_pCairo);
    PangoLayout*          layoutText = pango_cairo_create_layout(m_pCairo);
    PangoFontDescription* pangoFD    = pango_font_description_new();
    pango_font_description_set_family_static(pangoFD, "Sans");
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);

    const auto PBEZIER = g_pAnimationManager->getBezier("default");

    for (auto& notif : m_dNotifications) {
        const auto ICONPADFORNOTIF = notif->icon == ICON_NONE ? 0 : ICON_PAD;
        const auto FONTSIZE        = std::clamp((int)(notif->fontSize * ((pMonitor->vecPixelSize.x * SCALE) / 1920.f)), 8, 40);

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
        const auto ICON      = ICONS_ARRAY[m_eIconBackend][notif->icon];
        const auto ICONCOLOR = ICONS_COLORS[notif->icon];

        int        iconW = 0, iconH = 0;
        pango_font_description_set_absolute_size(pangoFD, PANGO_SCALE * FONTSIZE * ICON_SCALE);
        pango_layout_set_font_description(layoutIcon, pangoFD);
        pango_layout_set_text(layoutIcon, ICON.c_str(), -1);
        pango_layout_get_size(layoutIcon, &iconW, &iconH);
        iconW /= PANGO_SCALE;
        iconH /= PANGO_SCALE;

        int textW = 0, textH = 0;
        pango_font_description_set_absolute_size(pangoFD, PANGO_SCALE * FONTSIZE);
        pango_layout_set_font_description(layoutText, pangoFD);
        pango_layout_set_text(layoutText, notif->text.c_str(), -1);
        pango_layout_get_size(layoutText, &textW, &textH);
        textW /= PANGO_SCALE;
        textH /= PANGO_SCALE;

        const auto NOTIFSIZE = Vector2D{textW + 20 + iconW + 2 * ICONPADFORNOTIF, textH + 10};

        // draw rects
        cairo_set_source_rgba(m_pCairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);
        cairo_rectangle(m_pCairo, MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY, (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_pCairo);

        cairo_set_source_rgb(m_pCairo, 0.f, 0.f, 0.f);
        cairo_rectangle(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC, offsetY, NOTIFSIZE.x * SECONDRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_pCairo);

        cairo_set_source_rgba(m_pCairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);
        cairo_rectangle(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + 3, offsetY + NOTIFSIZE.y - 4, THIRDRECTPERC * (NOTIFSIZE.x - 6), 2);
        cairo_fill(m_pCairo);

        // draw gradient
        if (notif->icon != ICON_NONE) {
            cairo_pattern_t* pattern;
            pattern = cairo_pattern_create_linear(MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY,
                                                  MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC + GRADIENT_SIZE, offsetY);
            cairo_pattern_add_color_stop_rgba(pattern, 0, ICONCOLOR.r, ICONCOLOR.g, ICONCOLOR.b, ICONCOLOR.a / 3.0);
            cairo_pattern_add_color_stop_rgba(pattern, 1, ICONCOLOR.r, ICONCOLOR.g, ICONCOLOR.b, 0);
            cairo_rectangle(m_pCairo, MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY, GRADIENT_SIZE, NOTIFSIZE.y);
            cairo_set_source(m_pCairo, pattern);
            cairo_fill(m_pCairo);
            cairo_pattern_destroy(pattern);

            // draw icon
            cairo_set_source_rgb(m_pCairo, 1.f, 1.f, 1.f);
            cairo_move_to(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + NOTIF_LEFTBAR_SIZE + ICONPADFORNOTIF - 1, offsetY + std::round((NOTIFSIZE.y - iconH - 4) / 2.0));
            pango_cairo_show_layout(m_pCairo, layoutIcon);
        }

        // draw text
        cairo_set_source_rgb(m_pCairo, 1.f, 1.f, 1.f);
        cairo_move_to(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + NOTIF_LEFTBAR_SIZE + iconW + 2 * ICONPADFORNOTIF, offsetY + FONTSIZE + (FONTSIZE / 10.0));
        pango_cairo_show_layout(m_pCairo, layoutText);

        // adjust offset and move on
        offsetY += NOTIFSIZE.y + 10;

        if (maxWidth < NOTIFSIZE.x)
            maxWidth = NOTIFSIZE.x;
    }

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);
    g_object_unref(layoutIcon);

    // cleanup notifs
    std::erase_if(m_dNotifications, [](const auto& notif) { return notif->started.getMillis() > notif->timeMs; });

    return CBox{(int)(pMonitor->vecPosition.x + pMonitor->vecSize.x - maxWidth - 20), (int)pMonitor->vecPosition.y, (int)maxWidth + 20, (int)offsetY + 10};
}

void CHyprNotificationOverlay::draw(CMonitor* pMonitor) {

    const auto MONSIZE = pMonitor->vecTransformedSize;

    if (m_pLastMonitor != pMonitor || m_vecLastSize != MONSIZE || !m_pCairo || !m_pCairoSurface) {

        if (m_pCairo && m_pCairoSurface) {
            cairo_destroy(m_pCairo);
            cairo_surface_destroy(m_pCairoSurface);
        }

        m_pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, MONSIZE.x, MONSIZE.y);
        m_pCairo        = cairo_create(m_pCairoSurface);
        m_pLastMonitor  = pMonitor;
        m_vecLastSize   = MONSIZE;
    }

    // Draw the notifications
    if (m_dNotifications.size() == 0)
        return;

    // Render to the monitor

    // clear the pixmap
    cairo_save(m_pCairo);
    cairo_set_operator(m_pCairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_pCairo);
    cairo_restore(m_pCairo);

    cairo_surface_flush(m_pCairoSurface);

    CBox damage = drawNotifications(pMonitor);

    g_pHyprRenderer->damageBox(&damage);
    g_pHyprRenderer->damageBox(&m_bLastDamage);

    g_pCompositor->scheduleFrameForMonitor(pMonitor);

    m_bLastDamage = damage;

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(m_pCairoSurface);
    m_tTexture.allocate();
    glBindTexture(GL_TEXTURE_2D, m_tTexture.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MONSIZE.x, MONSIZE.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    CBox pMonBox = {0, 0, MONSIZE.x, MONSIZE.y};
    g_pHyprOpenGL->renderTexture(m_tTexture, &pMonBox, 1.f);
}

bool CHyprNotificationOverlay::hasAny() {
    return !m_dNotifications.empty();
}
