#include <pango/pangocairo.h>
#include "HyprError.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/animation/AnimationTree.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../render/Renderer.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../event/EventBus.hpp"
#include <hyprutils/utils/ScopeGuard.hpp>
#include <string_view>

using namespace Hyprutils::Animation;
using namespace Hyprutils::Utils;

CHyprError::CHyprError() {
    g_pAnimationManager->createAnimation(0.f, m_fadeOpacity, Config::animationTree()->getAnimationPropertyConfig("fadeIn"), AVARDAMAGE_NONE);

    static auto P = Event::bus()->m_events.monitor.focused.listen([&](PHLMONITOR mon) {
        if (!m_isCreated)
            return;

        g_pHyprRenderer->damageMonitor(Desktop::focusState()->monitor());
        m_monitorChanged = true;
    });

    static auto P2 = Event::bus()->m_events.render.pre.listen([&](PHLMONITOR mon) {
        if (!m_isCreated)
            return;

        if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
            g_pHyprRenderer->damageBox(m_damageBox);
    });
}

void CHyprError::queueCreate(const std::string& message, const CHyprColor& color) {
    m_queued      = message;
    m_queuedColor = color;
}

void CHyprError::queueError(const std::string& err) {
    queueCreate(err + "\nHyprland may not work correctly.", CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
}

void CHyprError::createQueued() {
    if (m_isCreated && m_texture)
        m_texture.reset();

    m_fadeOpacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeIn"));

    m_fadeOpacity->setValueAndWarp(0.f);
    *m_fadeOpacity = 1.f;

    const auto PMONITOR = g_pCompositor->m_monitors.front();
    const auto SCALE    = PMONITOR->m_scale;
    const auto FONTSIZE = std::clamp(sc<int>(10.f * ((PMONITOR->m_pixelSize.x * SCALE) / 1920.f)), 8, 40);

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    // RAII ScopeGuard guarantees C-struct memory is freed upon function exit
    CScopeGuard cairoGuard([&]() {
        cairo_destroy(CAIRO);
        cairo_surface_destroy(CAIROSURFACE);
    });

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
    const double PAD     = 10 * SCALE;

    const double WIDTH  = PMONITOR->m_pixelSize.x - PAD * 2;
    const double HEIGHT = (FONTSIZE + 2 * (FONTSIZE / 10.0)) * (VISLINECOUNT + EXTRALINES) + 3;
    const double RADIUS = PAD > HEIGHT / 2 ? HEIGHT / 2 - 1 : PAD;
    const double X      = PAD;
    const double Y      = TOPBAR ? PAD : PMONITOR->m_pixelSize.y - HEIGHT - PAD;

    m_damageBox = {sc<int>(PMONITOR->m_position.x), sc<int>(PMONITOR->m_position.y + (TOPBAR ? 0 : PMONITOR->m_pixelSize.y - (HEIGHT + PAD * 2))), sc<int>(PMONITOR->m_pixelSize.x),
                   sc<int>(HEIGHT + PAD * 2)};

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

    static auto           fontFamily = CConfigValue<std::string>("misc:font_family");
    PangoLayout*          layoutText = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    // RAII ScopeGuard for Pango cleanup
    CScopeGuard pangoGuard([&]() {
        pango_font_description_free(pangoFD);
        g_object_unref(layoutText);
    });

    pango_font_description_set_family(pangoFD, (*fontFamily).c_str());
    pango_font_description_set_absolute_size(pangoFD, FONTSIZE * PANGO_SCALE);
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layoutText, pangoFD);
    pango_layout_set_width(layoutText, (WIDTH - 2 * (1 + RADIUS)) * PANGO_SCALE);
    pango_layout_set_ellipsize(layoutText, PANGO_ELLIPSIZE_END);

    cairo_set_source_rgba(CAIRO, 0.9, 0.9, 0.9, 1.0);

    float            yoffset     = TOPBAR ? 0 : Y - PAD;
    int              renderedcnt = 0;
    std::string_view queuedView(m_queued);

    // Optimized loop: Use std::string_view to eliminate heap allocations
    while (!queuedView.empty() && renderedcnt < VISLINECOUNT) {
        auto             newlinePos = queuedView.find('\n');
        std::string_view current    = queuedView.substr(0, newlinePos);

        cairo_move_to(CAIRO, PAD + 1 + RADIUS, yoffset + PAD + 1);
        pango_layout_set_text(layoutText, current.data(), current.length());
        pango_cairo_show_layout(CAIRO, layoutText);

        yoffset += FONTSIZE + (FONTSIZE / 10.f);
        renderedcnt++;

        if (newlinePos != std::string_view::npos)
            queuedView = queuedView.substr(newlinePos + 1);
        else
            queuedView = "";
    }

    if (VISLINECOUNT < LINECOUNT) {
        std::string moreString = std::format("({} more...)", LINECOUNT - VISLINECOUNT);
        cairo_move_to(CAIRO, PAD + 1 + RADIUS, yoffset + PAD + 1);
        pango_layout_set_text(layoutText, moreString.data(), moreString.length());
        pango_cairo_show_layout(CAIRO, layoutText);
    }

    m_lastHeight = HEIGHT;
    cairo_surface_flush(CAIROSURFACE);

    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    auto       tex  = texture();
    tex->allocate(PMONITOR->m_pixelSize);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    m_isCreated   = true;
    m_queued      = "";
    m_queuedColor = CHyprColor();

    g_pHyprRenderer->damageMonitor(PMONITOR);

    for (const auto& m : g_pCompositor->m_monitors) {
        m->m_reservedArea.resetType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR);
    }

    const auto RESERVED = (HEIGHT + PAD) / SCALE;
    PMONITOR->m_reservedArea.addType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR, Vector2D{0.0, TOPBAR ? RESERVED : 0.0}, Vector2D{0.0, !TOPBAR ? RESERVED : 0.0});

    for (const auto& m : g_pCompositor->m_monitors) {
        g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
    }
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
                if (m_texture)
                    m_texture.reset();
                m_isCreated = false;
                m_queued    = "";

                for (auto& m : g_pCompositor->m_monitors) {
                    g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
                    m->m_reservedArea.resetType(Desktop::RESERVED_DYNAMIC_TYPE_ERROR_BAR);
                }

                return;
            } else {
                m_fadeOpacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadeOut"));
                *m_fadeOpacity = 0.f;
            }
        }
    }

    const auto  PMONITOR = g_pHyprRenderer->m_renderData.pMonitor;
    CBox        monbox   = {0, 0, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y};

    static auto BAR_POSITION = CConfigValue<Hyprlang::INT>("debug:error_position");
    m_damageBox.x            = sc<int>(PMONITOR->m_position.x);
    m_damageBox.y            = sc<int>(PMONITOR->m_position.y + (*BAR_POSITION == 0 ? 0 : PMONITOR->m_pixelSize.y - m_damageBox.height));

    if (m_fadeOpacity->isBeingAnimated() || m_monitorChanged)
        g_pHyprRenderer->damageBox(m_damageBox);

    m_monitorChanged = false;

    CTexPassElement::SRenderData data;
    data.tex = texture();
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

SP<ITexture> CHyprError::texture() {
    if (!m_texture)
        m_texture = g_pHyprRenderer->createTexture();
    return m_texture;
}
