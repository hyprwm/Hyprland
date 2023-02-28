#include "HyprError.hpp"
#include "../Compositor.hpp"

CHyprError::CHyprError() {
    m_fFadeOpacity.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), nullptr, AVARDAMAGE_NONE);
    m_fFadeOpacity.registerVar();

    g_pHookSystem->hookDynamic("focusedMon", [&](void* self, std::any param) {
        if (!m_bIsCreated)
            return;

        g_pHyprRenderer->damageMonitor(g_pCompositor->m_pLastMonitor);
        m_bMonitorChanged = true;
    });

    g_pHookSystem->hookDynamic("preRender", [&](void* self, std::any param) {
        if (!m_bIsCreated)
            return;

        if (m_fFadeOpacity.isBeingAnimated() || m_bMonitorChanged)
            g_pHyprRenderer->damageBox(&m_bDamageBox);
    });
}

CHyprError::~CHyprError() {
    m_fFadeOpacity.unregister();
}

void CHyprError::queueCreate(std::string message, const CColor& color) {
    m_szQueued = message;
    m_cQueued  = color;
}

void CHyprError::createQueued() {
    if (m_bIsCreated) {
        m_bQueuedDestroy = false;
        m_tTexture.destroyTexture();
    }

    m_fFadeOpacity.setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));

    m_fFadeOpacity.setValueAndWarp(0.f);
    m_fFadeOpacity = 1.f;

    const auto PMONITOR = g_pCompositor->m_vMonitors.front().get();

    const auto SCALE = PMONITOR->scale;

    const auto FONTSIZE = std::clamp((int)(10.f * ((PMONITOR->vecPixelSize.x * SCALE) / 1920.f)), 8, 40);

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    const auto CAIRO = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    const auto   LINECOUNT = 1 + std::count(m_szQueued.begin(), m_szQueued.end(), '\n');

    const double DEGREES = M_PI / 180.0;

    const double PAD = 10 * SCALE;

    const double X      = PAD;
    const double Y      = PAD;
    const double WIDTH  = PMONITOR->vecPixelSize.x - PAD * 2;
    const double HEIGHT = (FONTSIZE + 2 * (FONTSIZE / 10.0)) * LINECOUNT + 3;
    const double RADIUS = PAD > HEIGHT / 2 ? HEIGHT / 2 - 1 : PAD;

    m_bDamageBox = {0, 0, (int)PMONITOR->vecPixelSize.x, (int)HEIGHT + (int)PAD * 2};

    cairo_new_sub_path(CAIRO);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + RADIUS, RADIUS, -90 * DEGREES, 0 * DEGREES);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + HEIGHT - RADIUS, RADIUS, 0 * DEGREES, 90 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + HEIGHT - RADIUS, RADIUS, 90 * DEGREES, 180 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + RADIUS, RADIUS, 180 * DEGREES, 270 * DEGREES);
    cairo_close_path(CAIRO);

    cairo_set_source_rgba(CAIRO, m_cQueued.r, m_cQueued.g, m_cQueued.b, m_cQueued.a);
    cairo_fill_preserve(CAIRO);
    cairo_set_source_rgba(CAIRO, 0, 0, 0, 1);
    cairo_set_line_width(CAIRO, 2);
    cairo_stroke(CAIRO);

    // draw the text with a common font
    const CColor textColor = m_cQueued.r + m_cQueued.g + m_cQueued.b < 0.2f ? CColor(1.0, 1.0, 1.0, 1.0) : CColor(0, 0, 0, 1.0);

    cairo_select_font_face(CAIRO, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(CAIRO, FONTSIZE);
    cairo_set_source_rgba(CAIRO, textColor.r, textColor.g, textColor.b, textColor.a);

    float yoffset = FONTSIZE;
    while (m_szQueued != "") {
        std::string current = m_szQueued.substr(0, m_szQueued.find('\n'));
        if (const auto NEWLPOS = m_szQueued.find('\n'); NEWLPOS != std::string::npos)
            m_szQueued = m_szQueued.substr(NEWLPOS + 1);
        else
            m_szQueued = "";
        cairo_move_to(CAIRO, PAD + 1 + RADIUS, yoffset + PAD + 1);
        cairo_show_text(CAIRO, current.c_str());
        yoffset += FONTSIZE + (FONTSIZE / 10.f);
    }

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_tTexture.allocate();
    glBindTexture(GL_TEXTURE_2D, m_tTexture.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    m_bIsCreated = true;
    m_szQueued   = "";
    m_cQueued    = CColor();

    g_pHyprRenderer->damageMonitor(PMONITOR);
}

void CHyprError::draw() {
    if (!m_bIsCreated || m_szQueued != "") {
        if (m_szQueued != "")
            createQueued();
        return;
    }

    if (m_bQueuedDestroy) {
        if (!m_fFadeOpacity.isBeingAnimated()) {
            if (m_fFadeOpacity.fl() == 0.f) {
                m_bQueuedDestroy = false;
                m_tTexture.destroyTexture();
                m_bIsCreated = false;
                m_szQueued   = "";
                return;
            } else {
                m_fFadeOpacity.setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
                m_fFadeOpacity = 0.f;
            }
        }
    }

    const auto PMONITOR = g_pHyprOpenGL->m_RenderData.pMonitor;

    wlr_box    monbox = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};

    m_bDamageBox.x = (int)PMONITOR->vecPosition.x;
    m_bDamageBox.y = (int)PMONITOR->vecPosition.y;

    if (m_fFadeOpacity.isBeingAnimated() || m_bMonitorChanged)
        g_pHyprRenderer->damageBox(&m_bDamageBox);

    m_bMonitorChanged = false;

    g_pHyprOpenGL->renderTexture(m_tTexture, &monbox, m_fFadeOpacity.fl(), 0);
}

void CHyprError::destroy() {
    if (m_bIsCreated)
        m_bQueuedDestroy = true;
}
