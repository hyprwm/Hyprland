#include <pango/pangocairo.h>
#include "HyprError.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/HookSystemManager.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Animation;

CHyprError::CHyprError() {
    g_pAnimationManager->createAnimation(0.f, m_fadeOpacity, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), AVARDAMAGE_NONE);

    static auto P = g_pHookSystem->hookDynamic("focusedMon", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_isCreated)
            return;

        g_pHyprRenderer->damageMonitor(g_pCompositor->m_lastMonitor.lock());
        m_monitorChanged = true;
    });

    static auto P2 = g_pHookSystem->hookDynamic("preRender", [&](void* self, SCallbackInfo& info, std::any param) {
        if (!m_isCreated)
            return;

        if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
            g_pHyprRenderer->damageBox(m_damageBox);
    });

    m_texture = makeShared<CTexture>();
}

void CHyprError::queueCreate(std::string message, const CHyprColor& color) {
    m_queued      = message;
    m_queuedColor = color;
}

void CHyprError::createQueued() {
    if (m_isCreated)
        m_texture->destroyTexture();

    m_fadeOpacity->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));

    m_fadeOpacity->setValueAndWarp(0.f);
    *m_fadeOpacity = 1.f;

    const auto PMONITOR = g_pCompositor->m_monitors.front();

    const auto SCALE = PMONITOR->m_scale;

    const auto FONTSIZE = std::clamp(sc<int>(10.f * ((PMONITOR->m_pixelSize.x * SCALE) / 1920.f)), 8, 40);

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);

    const auto CAIRO = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    const auto   LINECOUNT    = Hyprlang::INT{1} + std::ranges::count(m_queued, '\n');
    static auto  LINELIMIT    = CConfigValue<Hyprlang::INT>("debug:error_limit");
    static auto  BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");

    const bool   TOPBAR = *BAR_POSITION == 0;

    const auto   VISLINECOUNT = std::min(LINECOUNT, *LINELIMIT);
    const auto   EXTRALINES   = (VISLINECOUNT < LINECOUNT) ? 1 : 0;

    const double DEGREES = M_PI / 180.0;

    const double PAD = 10 * SCALE;

    const double WIDTH  = PMONITOR->m_pixelSize.x - PAD * 2;
    const double HEIGHT = (FONTSIZE + 2 * (FONTSIZE / 10.0)) * (VISLINECOUNT + EXTRALINES) + 3;
    const double RADIUS = PAD > HEIGHT / 2 ? HEIGHT / 2 - 1 : PAD;
    const double X      = PAD;
    const double Y      = TOPBAR ? PAD : PMONITOR->m_pixelSize.y - HEIGHT - PAD;

    m_damageBox = {0, 0, sc<int>(PMONITOR->m_pixelSize.x), sc<int>(HEIGHT) + sc<int>(PAD) * 2};

    cairo_new_sub_path(CAIRO);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + RADIUS, RADIUS, -90 * DEGREES, 0 * DEGREES);
    cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + HEIGHT - RADIUS, RADIUS, 0 * DEGREES, 90 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + HEIGHT - RADIUS, RADIUS, 90 * DEGREES, 180 * DEGREES);
    cairo_arc(CAIRO, X + RADIUS, Y + RADIUS, RADIUS, 180 * DEGREES, 270 * DEGREES);
    cairo_close_path(CAIRO);

    cairo_set_source_rgba(CAIRO, 0.06, 0.06, 0.06, 1.0);
    cairo_fill_preserve(CAIRO);
    cairo_set_source_rgba(CAIRO, m_queuedColor.r, m_queuedColor.g, m_queuedColor.b, m_queuedColor.a);
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
    while (!m_queued.empty() && renderedcnt < VISLINECOUNT) {
        std::string current = m_queued.substr(0, m_queued.find('\n'));
        if (const auto NEWLPOS = m_queued.find('\n'); NEWLPOS != std::string::npos)
            m_queued = m_queued.substr(NEWLPOS + 1);
        else
            m_queued = "";
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
    m_queued = "";

    m_lastHeight = yoffset + PAD + 1 - (TOPBAR ? 0 : Y - PAD);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_texture->allocate();
    m_texture->bind();
    m_texture->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_texture->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_texture->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    m_texture->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);

    m_isCreated   = true;
    m_queued      = "";
    m_queuedColor = CHyprColor();

    g_pHyprRenderer->damageMonitor(PMONITOR);

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->m_id);
}

void CHyprError::draw() {
    if (!m_isCreated || !m_queued.empty()) {
        if (!m_queued.empty())
            createQueued();
        return;
    }

    if (m_queuedDestroy) {
        if (!m_fadeOpacity->isBeingAnimated()) {
            if (m_fadeOpacity->value() == 0.f) {
                m_queuedDestroy = false;
                m_texture->destroyTexture();
                m_isCreated = false;
                m_queued    = "";

                for (auto& m : g_pCompositor->m_monitors) {
                    g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
                }

                return;
            } else {
                m_fadeOpacity->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));
                *m_fadeOpacity = 0.f;
            }
        }
    }

    const auto PMONITOR = g_pHyprOpenGL->m_renderData.pMonitor;

    CBox       monbox = {0, 0, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y};

    m_damageBox.x = sc<int>(PMONITOR->m_position.x);
    m_damageBox.y = sc<int>(PMONITOR->m_position.y);

    if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
        g_pHyprRenderer->damageBox(m_damageBox);

    m_monitorChanged = false;

    CTexPassElement::SRenderData data;
    data.tex = m_texture;
    data.box = monbox;
    data.a   = m_fadeOpacity->value();

    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

void CHyprError::destroy() {
    if (m_isCreated)
        m_queuedDestroy = true;
    else
        m_queued = "";
}

bool CHyprError::active() {
    return m_isCreated;
}

float CHyprError::height() {
    return m_lastHeight;
}
