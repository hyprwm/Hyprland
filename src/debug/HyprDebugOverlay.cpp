#include <pango/pangocairo.h>
#include "HyprDebugOverlay.hpp"
#include "config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/pass/TexPassElement.hpp"
#include "../render/Renderer.hpp"
#include "../managers/AnimationManager.hpp"

CHyprDebugOverlay::CHyprDebugOverlay() {
    m_pTexture = makeShared<CTexture>();
}

void CHyprMonitorDebugOverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_dLastRenderTimes.emplace_back(durationUs / 1000.f);

    if (m_dLastRenderTimes.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastRenderTimes.pop_front();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;
}

void CHyprMonitorDebugOverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_dLastRenderTimesNoOverlay.emplace_back(durationUs / 1000.f);

    if (m_dLastRenderTimesNoOverlay.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastRenderTimesNoOverlay.pop_front();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;
}

void CHyprMonitorDebugOverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_dLastFrametimes.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_tpLastFrame).count() / 1000.f);

    if (m_dLastFrametimes.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastFrametimes.pop_front();

    m_tpLastFrame = std::chrono::high_resolution_clock::now();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;

    // anim data too
    const auto PMONITORFORTICKS = g_pHyprRenderer->m_pMostHzMonitor ? g_pHyprRenderer->m_pMostHzMonitor.lock() : g_pCompositor->m_pLastMonitor.lock();
    if (PMONITORFORTICKS) {
        if (m_dLastAnimationTicks.size() > (long unsigned int)PMONITORFORTICKS->refreshRate)
            m_dLastAnimationTicks.pop_front();

        m_dLastAnimationTicks.push_back(g_pAnimationManager->m_fLastTickTime);
    }
}

int CHyprMonitorDebugOverlay::draw(int offset) {

    if (!m_pMonitor)
        return 0;

    // get avg fps
    float avgFrametime = 0;
    float maxFrametime = 0;
    float minFrametime = 9999;
    for (auto const& ft : m_dLastFrametimes) {
        if (ft > maxFrametime)
            maxFrametime = ft;
        if (ft < minFrametime)
            minFrametime = ft;
        avgFrametime += ft;
    }
    float varFrametime = maxFrametime - minFrametime;
    avgFrametime /= m_dLastFrametimes.size() == 0 ? 1 : m_dLastFrametimes.size();

    float avgRenderTime = 0;
    float maxRenderTime = 0;
    float minRenderTime = 9999;
    for (auto const& rt : m_dLastRenderTimes) {
        if (rt > maxRenderTime)
            maxRenderTime = rt;
        if (rt < minRenderTime)
            minRenderTime = rt;
        avgRenderTime += rt;
    }
    float varRenderTime = maxRenderTime - minRenderTime;
    avgRenderTime /= m_dLastRenderTimes.size() == 0 ? 1 : m_dLastRenderTimes.size();

    float avgRenderTimeNoOverlay = 0;
    float maxRenderTimeNoOverlay = 0;
    float minRenderTimeNoOverlay = 9999;
    for (auto const& rt : m_dLastRenderTimesNoOverlay) {
        if (rt > maxRenderTimeNoOverlay)
            maxRenderTimeNoOverlay = rt;
        if (rt < minRenderTimeNoOverlay)
            minRenderTimeNoOverlay = rt;
        avgRenderTimeNoOverlay += rt;
    }
    float varRenderTimeNoOverlay = maxRenderTimeNoOverlay - minRenderTimeNoOverlay;
    avgRenderTimeNoOverlay /= m_dLastRenderTimes.size() == 0 ? 1 : m_dLastRenderTimes.size();

    float avgAnimMgrTick = 0;
    float maxAnimMgrTick = 0;
    float minAnimMgrTick = 9999;
    for (auto const& at : m_dLastAnimationTicks) {
        if (at > maxAnimMgrTick)
            maxAnimMgrTick = at;
        if (at < minAnimMgrTick)
            minAnimMgrTick = at;
        avgAnimMgrTick += at;
    }
    float varAnimMgrTick = maxAnimMgrTick - minAnimMgrTick;
    avgAnimMgrTick /= m_dLastAnimationTicks.size() == 0 ? 1 : m_dLastAnimationTicks.size();

    const float           FPS      = 1.f / (avgFrametime / 1000.f); // frametimes are in ms
    const float           idealFPS = m_dLastFrametimes.size();

    static auto           fontFamily = CConfigValue<std::string>("misc:font_family");
    PangoLayout*          layoutText = pango_cairo_create_layout(g_pDebugOverlay->m_pCairo);
    PangoFontDescription* pangoFD    = pango_font_description_new();

    pango_font_description_set_family(pangoFD, (*fontFamily).c_str());
    pango_font_description_set_style(pangoFD, PANGO_STYLE_NORMAL);
    pango_font_description_set_weight(pangoFD, PANGO_WEIGHT_NORMAL);

    float maxTextW = 0;
    int   fontSize = 0;
    auto  cr       = g_pDebugOverlay->m_pCairo;

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
    cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 1.f, 1.f);

    std::string text;
    showText(m_pMonitor->szName.c_str(), 10);

    if (FPS > idealFPS * 0.95f)
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 0.2f, 1.f, 0.2f, 1.f);
    else if (FPS > idealFPS * 0.8f)
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 0.2f, 1.f);
    else
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 0.2f, 0.2f, 1.f);

    text = std::format("{} FPS", (int)FPS);
    showText(text.c_str(), 16);

    cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 1.f, 1.f);

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

    g_pHyprRenderer->damageBox(&m_wbLastDrawnBox);
    m_wbLastDrawnBox = {(int)g_pCompositor->m_vMonitors.front()->vecPosition.x + MARGIN_LEFT - 1, (int)g_pCompositor->m_vMonitors.front()->vecPosition.y + offset + MARGIN_TOP - 1,
                        (int)maxTextW + 2, posY - offset - MARGIN_TOP + 2};
    g_pHyprRenderer->damageBox(&m_wbLastDrawnBox);

    return posY - offset;
}

void CHyprDebugOverlay::renderData(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_mMonitorOverlays[pMonitor].renderData(pMonitor, durationUs);
}

void CHyprDebugOverlay::renderDataNoOverlay(PHLMONITOR pMonitor, float durationUs) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_mMonitorOverlays[pMonitor].renderDataNoOverlay(pMonitor, durationUs);
}

void CHyprDebugOverlay::frameData(PHLMONITOR pMonitor) {
    static auto PDEBUGOVERLAY = CConfigValue<Hyprlang::INT>("debug:overlay");

    if (!*PDEBUGOVERLAY)
        return;

    m_mMonitorOverlays[pMonitor].frameData(pMonitor);
}

void CHyprDebugOverlay::draw() {

    const auto PMONITOR = g_pCompositor->m_vMonitors.front();

    if (!m_pCairoSurface || !m_pCairo) {
        m_pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);
        m_pCairo        = cairo_create(m_pCairoSurface);
    }

    // clear the pixmap
    cairo_save(m_pCairo);
    cairo_set_operator(m_pCairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_pCairo);
    cairo_restore(m_pCairo);

    // draw the things
    int offsetY = 0;
    for (auto const& m : g_pCompositor->m_vMonitors) {
        offsetY += m_mMonitorOverlays[m].draw(offsetY);
        offsetY += 5; // for padding between mons
    }

    cairo_surface_flush(m_pCairoSurface);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(m_pCairoSurface);
    m_pTexture->allocate();
    glBindTexture(GL_TEXTURE_2D, m_pTexture->m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    CTexPassElement::SRenderData data;
    data.tex = m_pTexture;
    data.box = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};
    g_pHyprRenderer->m_sRenderPass.add(makeShared<CTexPassElement>(data));
}
