#include "HyprNotificationOverlay.hpp"
#include "../Compositor.hpp"

CHyprNotificationOverlay::CHyprNotificationOverlay() {
    g_pHookSystem->hookDynamic("focusedMon", [&](void* self, std::any param) {
        if (m_dNotifications.size() == 0)
            return;

        g_pHyprRenderer->damageBox(&m_bLastDamage);
    });
}

void CHyprNotificationOverlay::addNotification(const std::string& text, const CColor& color, const float timeMs) {
    const auto PNOTIF = m_dNotifications.emplace_back(std::make_unique<SNotification>()).get();

    PNOTIF->text  = text;
    PNOTIF->color = color;
    PNOTIF->started.reset();
    PNOTIF->timeMs = timeMs;
}

wlr_box CHyprNotificationOverlay::drawNotifications(CMonitor* pMonitor) {
    static constexpr auto ANIM_DURATION_MS   = 600.0;
    static constexpr auto ANIM_LAG_MS        = 100.0;
    static constexpr auto NOTIF_LEFTBAR_SIZE = 5.0;

    float                 offsetY  = 10;
    float                 maxWidth = 0;

    const auto            SCALE    = pMonitor->scale;
    const auto            FONTSIZE = std::clamp((int)(10.f * ((pMonitor->vecPixelSize.x * SCALE) / 1920.f)), 8, 40);

    const auto            MONSIZE = pMonitor->vecPixelSize;

    cairo_select_font_face(m_pCairo, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, FONTSIZE);

    cairo_text_extents_t cairoExtents;

    const auto           PBEZIER = g_pAnimationManager->getBezier("default");

    for (auto& notif : m_dNotifications) {
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
        cairo_text_extents(m_pCairo, notif->text.c_str(), &cairoExtents);

        cairo_set_source_rgba(m_pCairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);

        const auto NOTIFSIZE = Vector2D{cairoExtents.width + 20, cairoExtents.height + 10};

        // draw rects
        cairo_rectangle(m_pCairo, MONSIZE.x - (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, offsetY, (NOTIFSIZE.x + NOTIF_LEFTBAR_SIZE) * FIRSTRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_pCairo);

        cairo_set_source_rgb(m_pCairo, 0.f, 0.f, 0.f);

        cairo_rectangle(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC, offsetY, NOTIFSIZE.x * SECONDRECTPERC, NOTIFSIZE.y);
        cairo_fill(m_pCairo);

        cairo_set_source_rgba(m_pCairo, notif->color.r, notif->color.g, notif->color.b, notif->color.a);

        cairo_rectangle(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + 3, offsetY + NOTIFSIZE.y - 4, THIRDRECTPERC * (NOTIFSIZE.x - 6), 2);
        cairo_fill(m_pCairo);

        // draw text
        cairo_set_source_rgb(m_pCairo, 1.f, 1.f, 1.f);
        cairo_move_to(m_pCairo, MONSIZE.x - NOTIFSIZE.x * SECONDRECTPERC + NOTIF_LEFTBAR_SIZE, offsetY + FONTSIZE + (FONTSIZE / 10.0));
        cairo_show_text(m_pCairo, notif->text.c_str());

        // adjust offset and move on
        offsetY += NOTIFSIZE.y + 10;

        if (maxWidth < NOTIFSIZE.x)
            maxWidth = NOTIFSIZE.x;
    }

    // cleanup notifs
    std::erase_if(m_dNotifications, [](const auto& notif) { return notif->started.getMillis() > notif->timeMs; });

    return wlr_box{(int)(pMonitor->vecPosition.x + pMonitor->vecSize.x - maxWidth - 20), (int)pMonitor->vecPosition.y, (int)maxWidth + 20, (int)offsetY + 10};
}

void CHyprNotificationOverlay::draw(CMonitor* pMonitor) {

    if (m_pLastMonitor != pMonitor || !m_pCairo || !m_pCairoSurface) {

        if (m_pCairo && m_pCairoSurface) {
            cairo_destroy(m_pCairo);
            cairo_surface_destroy(m_pCairoSurface);
        }

        m_pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_pCairo        = cairo_create(m_pCairoSurface);
        m_pLastMonitor  = pMonitor;
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

    wlr_box damage = drawNotifications(pMonitor);

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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    wlr_box pMonBox = {0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y};
    g_pHyprOpenGL->renderTexture(m_tTexture, &pMonBox, 1.f);
}