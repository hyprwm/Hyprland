#include <pango/pangocairo.h>
#include "HyprError.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../managers/AnimationManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/HookSystemManager.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Animation;

CHyprError::CHyprError() {
    g_pAnimationManager->createAnimation(0.f, m_fFadeOpacity, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), AVARDAMAGE_NONE);

    static auto P = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_bIsCreated)
            return;

        g_pHyprRenderer->damageMonitor(g_pCompositor->m_lastMonitor.lock());
        m_bMonitorChanged = true;
    });

    static auto P2 = g_pHookSystem->hookDynamic("preRender", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_bIsCreated)
            return;

        if (m_fFadeOpacity->isBeingAnimated() || m_bMonitorChanged)
            g_pHyprRenderer->damageBox(m_bDamageBox);
    });

    m_pTexture = makeShared<CTexture>();
}

void CHyprError::queueCreate(std::string message, const CHyprColor& color) {
    m_szQueued = message;
    m_cQueued  = color;
}

void CHyprError::createQueued() {
    if (m_bIsCreated)
        m_pTexture->destroyTexture();

    m_fFadeOpacity->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));

    m_fFadeOpacity->setValueAndWarp(0.f);
    *m_fFadeOpacity = 1.f;

    const auto PMONITOR = g_pCompositor->m_monitors.front();

    const auto SCALE = PMONITOR->scale;

    const auto FONTSIZE = std::clamp((int)(10.f * ((PMONITOR->vecPixelSize.x * SCALE) / 1920.f)), 8, 40);

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    const auto CAIRO = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    const auto   LINECOUNT    = Hyprlang::INT{1} + std::count(m_szQueued.begin(), m_szQueued.end(), '\n');
    static auto  LINELIMIT    = CConfigValue<Hyprlang::INT>("debug:error_limit");
    static auto  BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");

    const bool   TOPBAR = *BAR_POSITION == 0;

    const auto   VISLINECOUNT = std::min(LINECOUNT, *LINELIMIT);
    const auto   EXTRALINES   = (VISLINECOUNT < LINECOUNT) ? 1 : 0;

    const double DEGREES = M_PI / 180.0;

    const double PAD = 10 * SCALE;

    const double WIDTH  = PMONITOR->vecPixelSize.x - PAD * 2;
    const double HEIGHT = (FONTSIZE + 2 * (FONTSIZE / 10.0)) * (VISLINECOUNT + EXTRALINES) + 3;
    const double RADIUS = PAD > HEIGHT / 2 ? HEIGHT / 2 - 1 : PAD;
    const double X      = PAD;
    const double Y      = TOPBAR ? PAD : PMONITOR->vecPixelSize.y - HEIGHT - PAD;

    m_bDamageBox = {0, 0, (int)PMONITOR->vecPixelSize.x, (int)HEIGHT + (int)PAD * 2};

    cairo_new_sub_path(CAIRO);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + RADIUS, RADIUS, -90 * DEGREES, 0 * DEGREES);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + HEIGHT - RADIUS, RADIUS, 0 * DEGREES, 90 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + HEIGHT - RADIUS, RADIUS, 90 * DEGREES, 180 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + RADIUS, RADIUS, 180 * DEGREES, 270 * DEGREES);
    cairo_close_path(CAIRO);

    cairo_set_source_rgba(CAIRO, 0.06, 0.06, 0.06, 1.0);
    cairo_fill_preserve(CAIRO);
    cairo_set_source_rgba(CAIRO, m_cQueued.r, m_cQueued.g, m_cQueued.b, m_cQueued.a);
    cairo_set_line_width(CAIRO, 2);
    cairo_stroke(CAIRO);

    // draw the text with a common font
    const CHyprColor textColor = CHyprColor(0.9, 0.9, 0.9, 1.0);
    cairo_set_source_rgba(CAIRO, textColor.r, textColor.g, textColor.b, textColor.a);

    static auto           fontFamily = CConfigValue<std::string>("misc:font_family");
    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family(pangoFD, (*fontFamily).c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layoutText, pangoFD);

    float yoffset     = TOPBAR ? 0 : Y - PAD;
    int   renderedcnt = 0;
    while (!m_szQueued.empty() && renderedcnt < VISLINECOUNT) {
        std::string current = m_szQueued.substr(0, m_szQueued.find('\n'));
        if (const auto NEWLPOS = m_szQueued.find('\n'); NEWLPOS != std::string::npos)
            m_szQueued = m_szQueued.substr(NEWLPOS + 1);
        else
            m_szQueued = "";
        cairo_move_to(CAIRO, PAD + 1 + RADIUS, yoffset + PAD + 1);
        pango_layout_set_text(layoutText, current.c_str(), -1);
        pango_cairo_show_layout(CAIRO, layoutText);
        yoffset += FONTSIZE + (FONTSIZE / 10.f);
        renderedcnt++;
    }
    if (VISLINECOUNT < LINECOUNT) {
        std::string moreString = std::format("({} more...)", LINECOUNT - VISLINECOUNT);
        cairo_move_to(CAIRO, PAD + 1 + RADIUS, yoffset + PAD + 1);
        pango_layout_set_text(layoutText, moreString.c_str(), -1);
        pango_cairo_show_layout(CAIRO, layoutText);
    }
    m_szQueued = "";

    m_fLastHeight = yoffset + PAD + 1 - (TOPBAR ? 0 : Y - PAD);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_pTexture->allocate();
    glBindTexture(GL_TEXTURE_2D, m_pTexture->m_iTexID);
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
    m_cQueued    = CHyprColor();

    g_pHyprRenderer->damageMonitor(PMONITOR);

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
}

void CHyprError::draw() {
    if (!m_bIsCreated || m_szQueued != "") {
        if (m_szQueued != "")
            createQueued();
        return;
    }

    if (m_bQueuedDestroy) {
        if (!m_fFadeOpacity->isBeingAnimated()) {
            if (m_fFadeOpacity->value() == 0.f) {
                m_bQueuedDestroy = false;
                m_pTexture->destroyTexture();
                m_bIsCreated = false;
                m_szQueued   = "";

                for (auto& m : g_pCompositor->m_monitors) {
                    g_pHyprRenderer->arrangeLayersForMonitor(m->ID);
                }

                return;
            } else {
                m_fFadeOpacity->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
                *m_fFadeOpacity = 0.f;
            }
        }
    }

    const auto PMONITOR = g_pHyprOpenGL->m_RenderData.pMonitor;

    CBox       monbox = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};

    m_bDamageBox.x = (int)PMONITOR->vecPosition.x;
    m_bDamageBox.y = (int)PMONITOR->vecPosition.y;

    if (m_fFadeOpacity->isBeingAnimated() || m_bMonitorChanged)
        g_pHyprRenderer->damageBox(m_bDamageBox);

    m_bMonitorChanged = false;

    CTexPassElement::SRenderData data;
    data.tex = m_pTexture;
    data.box = monbox;
    data.a   = m_fFadeOpacity->value();

    g_pHyprRenderer->m_sRenderPass.add(makeShared<CTexPassElement>(data));
}

void CHyprError::destroy() {
    if (m_bIsCreated)
        m_bQueuedDestroy = true;
    else
        m_szQueued = "";
}

bool CHyprError::active() {
    return m_bIsCreated;
}

float CHyprError::height() {
    return m_fLastHeight;
}
