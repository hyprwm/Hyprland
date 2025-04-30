#include <pango/pangocairo.h>
#include "HyprDebugOverlay.hpp"
#include "config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../render/Renderer.hpp"
#include "../managers/AnimationManager.hpp"

CHyprDebugOverlay::CHyprDebugOverlay() {
    m_texture = makeShared<CTexture>();
}

void CHyprMonitorDebugOverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_lastRenderTimes.emplace_back(durationUs / 1000.f);

    if (m_lastRenderTimes.size() > (long unsigned int)pMonitor->m_refreshRate)
        m_lastRenderTimes.pop_front();

    if (!m_monitor)
        m_monitor = pMonitor;
}

void CHyprMonitorDebugOverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_lastRenderTimesNoOverlay.emplace_back(durationUs / 1000.f);

    if (m_lastRenderTimesNoOverlay.size() > (long unsigned int)pMonitor->m_refreshRate)
        m_lastRenderTimesNoOverlay.pop_front();

    if (!m_monitor)
        m_monitor = pMonitor;
}

void CHyprMonitorDebugOverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_lastFrametimes.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_lastFrame).count() / 1000.f);

    if (m_lastFrametimes.size() > (long unsigned int)pMonitor->m_refreshRate)
        m_lastFrametimes.pop_front();

    m_lastFrame = std::chrono::high_resolution_clock::now();

    if (!m_monitor)
        m_monitor = pMonitor;

    // anim data too
    const auto PMONITORFORTICKS = g_pHyprRenderer->m_pMostHzMonitor ? g_pHyprRenderer->m_pMostHzMonitor.lock() : g_pCompositor->m_lastMonitor.lock();
    if (PMONITORFORTICKS) {
        if (m_lastAnimationTicks.size() > (long unsigned int)PMONITORFORTICKS->m_refreshRate)
            m_lastAnimationTicks.pop_front();

        m_lastAnimationTicks.push_back(g_pAnimationManager->m_fLastTickTime);
    }
}

int CHyprMonitorDebugOverlay::draw(int offset) {

    if (!m_monitor)
        return 0;

    // get avg fps
    float avgFrametime = 0;
    float maxFrametime = 0;
    float minFrametime = 9999;
    for (auto const& ft : m_lastFrametimes) {
        if (ft > maxFrametime)
            maxFrametime = ft;
        if (ft < minFrametime)
            minFrametime = ft;
        avgFrametime += ft;
    }
    float varFrametime = maxFrametime - minFrametime;
    avgFrametime /= m_lastFrametimes.size() == 0 ? 1 : m_lastFrametimes.size();

    float avgRenderTime = 0;
    float maxRenderTime = 0;
    float minRenderTime = 9999;
    for (auto const& rt : m_lastRenderTimes) {
        if (rt > maxRenderTime)
            maxRenderTime = rt;
        if (rt < minRenderTime)
            minRenderTime = rt;
        avgRenderTime += rt;
    }
    float varRenderTime = maxRenderTime - minRenderTime;
    avgRenderTime /= m_lastRenderTimes.size() == 0 ? 1 : m_lastRenderTimes.size();

    float avgRenderTimeNoOverlay = 0;
    float maxRenderTimeNoOverlay = 0;
    float minRenderTimeNoOverlay = 9999;
    for (auto const& rt : m_lastRenderTimesNoOverlay) {
        if (rt > maxRenderTimeNoOverlay)
            maxRenderTimeNoOverlay = rt;
        if (rt < minRenderTimeNoOverlay)
            minRenderTimeNoOverlay = rt;
        avgRenderTimeNoOverlay += rt;
    }
    float varRenderTimeNoOverlay = maxRenderTimeNoOverlay - minRenderTimeNoOverlay;
    avgRenderTimeNoOverlay /= m_lastRenderTimes.size() == 0 ? 1 : m_lastRenderTimes.size();

    float avgAnimMgrTick = 0;
    float maxAnimMgrTick = 0;
    float minAnimMgrTick = 9999;
    for (auto const& at : m_lastAnimationTicks) {
        if (at > maxAnimMgrTick)
            maxAnimMgrTick = at;
        if (at < minAnimMgrTick)
            minAnimMgrTick = at;
        avgAnimMgrTick += at;
    }
    float varAnimMgrTick = maxAnimMgrTick - minAnimMgrTick;
    avgAnimMgrTick /= m_lastAnimationTicks.size() == 0 ? 1 : m_lastAnimationTicks.size();

    const float           FPS      = 1.f / (avgFrametime / 1000.f); // frametimes are in ms
    const float           idealFPS = m_lastFrametimes.size();

    static auto           fontFamily = CConfigValue<std::string>("misc:font_family");
    PangoLayout*          layoutText = pango_cairo_create_layout(g_pDebugOverlay->m_cairo);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family(pangoFD, (*fontFamily).c_str());
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);

    float maxTextW = 0;
    int   fontSize = 0;
    auto  cr       = g_pDebugOverlay->m_cairo;

    auto  showText = [cr, layoutText, pangoFD, &maxTextW, &fontSize](const char* text, int size) {
        if (fontSize != size) {
            pango_font_description_set_absolute_size(pangoFD, size * PANGO_SCALE);
            pango_layout_set_font_description(layoutText, pangoFD);
            fontSize = size;
        }

        pango_layout_set_text(layoutText, text, -1);
        pango_cairo_show_layout(cr, layoutText);

        int textW = 0, textH = 0;
        pango_layout_get_size(layoutText, &textW, &textH);
        textW /= PANGO_SCALE;
        textH /= PANGO_SCALE;
        if (textW > maxTextW)
            maxTextW = textW;

        // move to next line
        cairo_rel_move_to(cr, 0, fontSize + 1);
    };

    const int MARGIN_TOP  = 8;
    const int MARGIN_LEFT = 4;
    cairo_move_to(cr, MARGIN_LEFT, MARGIN_TOP + offset);
    cairo_set_source_rgba(g_pDebugOverlay->m_cairo, 1.f, 1.f, 1.f, 1.f);

    std::string text;
    showText(m_monitor->m_name.c_str(), 10);

    if (FPS > idealFPS * 0.95f)
        cairo_set_source_rgba(g_pDebugOverlay->m_cairo, 0.2f, 1.f, 0.2f, 1.f);
    else if (FPS > idealFPS * 0.8f)
        cairo_set_source_rgba(g_pDebugOverlay->m_cairo, 1.f, 1.f, 0.2f, 1.f);
    else
        cairo_set_source_rgba(g_pDebugOverlay->m_cairo, 1.f, 0.2f, 0.2f, 1.f);

    text = std::format("{} FPS", (int)FPS);
    showText(text.c_str(), 16);

    cairo_set_source_rgba(g_pDebugOverlay->m_cairo, 1.f, 1.f, 1.f, 1.f);

    text = std::format("Avg Frametime: {:.2f}ms (var {:.2f}ms)", avgFrametime, varFrametime);
    showText(text.c_str(), 10);

    text = std::format("Avg Rendertime: {:.2f}ms (var {:.2f}ms)", avgRenderTime, varRenderTime);
    showText(text.c_str(), 10);

    text = std::format("Avg Rendertime (No Overlay): {:.2f}ms (var {:.2f}ms)", avgRenderTimeNoOverlay, varRenderTimeNoOverlay);
    showText(text.c_str(), 10);

    text = std::format("Avg Anim Tick: {:.2f}ms (var {:.2f}ms) ({:.2f} TPS)", avgAnimMgrTick, varAnimMgrTick, 1.0 / (avgAnimMgrTick / 1000.0));
    showText(text.c_str(), 10);

    pango_font_description_free(pangoFD);
    g_object_unref(layoutText);

    double posX = 0, posY = 0;
    cairo_get_current_point(cr, &posX, &posY);

    g_pHyprRenderer->damageBox(m_lastDrawnBox);
    m_lastDrawnBox = {(int)g_pCompositor->m_monitors.front()->m_position.x + MARGIN_LEFT - 1, (int)g_pCompositor->m_monitors.front()->m_position.y + offset + MARGIN_TOP - 1,
                      (int)maxTextW + 2, posY - offset - MARGIN_TOP + 2};
    g_pHyprRenderer->damageBox(m_lastDrawnBox);

    return posY - offset;
}

void CHyprDebugOverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].renderData(pMonitor, durationUs);
}

void CHyprDebugOverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].renderDataNoOverlay(pMonitor, durationUs);
}

void CHyprDebugOverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_monitorOverlays[pMonitor].frameData(pMonitor);
}

void CHyprDebugOverlay::draw() {

    const auto PMONITOR = g_pCompositor->m_monitors.front();

    if (!m_cairoSurface || !m_cairo) {
        m_cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);
        m_cairo        = cairo_create(m_cairoSurface);
    }

    // clear the pixmap
    cairo_save(m_cairo);
    cairo_set_operator(m_cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cairo);
    cairo_restore(m_cairo);

    // draw the things
    int offsetY = 0;
    for (auto const& m : g_pCompositor->m_monitors) {
        offsetY += m_monitorOverlays[m].draw(offsetY);
        offsetY += 5; // for padding between mons
    }

    cairo_surface_flush(m_cairoSurface);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(m_cairoSurface);
    m_texture->allocate();
    glBindTexture(GL_TEXTURE_2D, m_texture->m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    CTexPassElement::SRenderData data;
    data.tex = m_texture;
    data.box = {0, 0, PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y};
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CTexPassElement>(data));
}
