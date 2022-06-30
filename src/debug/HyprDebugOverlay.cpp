#include "HyprDebugOverlay.hpp"
#include "../Compositor.hpp"

void CHyprMonitorDebugOverlay::renderData(SMonitor* pMonitor, float µs) {
    m_dLastRenderTimes.push_back(µs / 1000.f);

    if (m_dLastRenderTimes.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastRenderTimes.pop_front();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;
}

void CHyprMonitorDebugOverlay::renderDataNoOverlay(SMonitor* pMonitor, float µs) {
    m_dLastRenderTimesNoOverlay.push_back(µs / 1000.f);

    if (m_dLastRenderTimesNoOverlay.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastRenderTimesNoOverlay.pop_front();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;
}

void CHyprMonitorDebugOverlay::frameData(SMonitor* pMonitor) {
    m_dLastFrametimes.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_tpLastFrame).count() / 1000.f);

    if (m_dLastFrametimes.size() > (long unsigned int)pMonitor->refreshRate)
        m_dLastFrametimes.pop_front();

    m_tpLastFrame = std::chrono::high_resolution_clock::now();

    if (!m_pMonitor)
        m_pMonitor = pMonitor;
}

int CHyprMonitorDebugOverlay::draw(int offset) {

    if (!m_pMonitor)
        return 0;

    int yOffset = offset;
    cairo_text_extents_t cairoExtents;
    float maxX = 0;
    std::string text = "";

    // get avg fps
    float avgFrametime = 0;
    for (auto& ft : m_dLastFrametimes) {
        avgFrametime += ft;
    }
    avgFrametime /= m_dLastFrametimes.size() == 0 ? 1 : m_dLastFrametimes.size();

    float avgRenderTime = 0;
    for (auto& rt : m_dLastRenderTimes) {
        avgRenderTime += rt;
    }
    avgRenderTime /= m_dLastRenderTimes.size() == 0 ? 1 : m_dLastRenderTimes.size();

    float avgRenderTimeNoOverlay = 0;
    for (auto& rt : m_dLastRenderTimesNoOverlay) {
        avgRenderTimeNoOverlay += rt;
    }
    avgRenderTimeNoOverlay /= m_dLastRenderTimes.size() == 0 ? 1 : m_dLastRenderTimes.size();

    const float FPS = 1.f / (avgFrametime / 1000.f); // frametimes are in ms
    const float idealFPS = m_dLastFrametimes.size();

    cairo_select_font_face(g_pDebugOverlay->m_pCairo, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    cairo_set_font_size(g_pDebugOverlay->m_pCairo, 10);
    cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 1.f, 1.f);

    yOffset += 10;
    cairo_move_to(g_pDebugOverlay->m_pCairo, 0, yOffset);
    text = m_pMonitor->szName;
    cairo_show_text(g_pDebugOverlay->m_pCairo, text.c_str());
    cairo_text_extents(g_pDebugOverlay->m_pCairo, text.c_str(), &cairoExtents);
    if (cairoExtents.width > maxX) maxX = cairoExtents.width;

    cairo_set_font_size(g_pDebugOverlay->m_pCairo, 16);

    if (FPS > idealFPS * 0.95f)
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 0.2f, 1.f, 0.2f, 1.f);
    else if (FPS > idealFPS * 0.8f)
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 0.2f, 1.f);
    else
        cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 0.2f, 0.2f, 1.f);

    yOffset += 17;
    cairo_move_to(g_pDebugOverlay->m_pCairo, 0, yOffset);
    text = std::string(std::to_string((int)FPS) + " FPS");
    cairo_show_text(g_pDebugOverlay->m_pCairo, text.c_str());
    cairo_text_extents(g_pDebugOverlay->m_pCairo, text.c_str(), &cairoExtents);
    if (cairoExtents.width > maxX) maxX = cairoExtents.width;

    cairo_set_font_size(g_pDebugOverlay->m_pCairo, 10);
    cairo_set_source_rgba(g_pDebugOverlay->m_pCairo, 1.f, 1.f, 1.f, 1.f);

    yOffset += 11;
    cairo_move_to(g_pDebugOverlay->m_pCairo, 0, yOffset);
    text = std::string("Avg Frametime: " + std::to_string((int)avgFrametime) + "." + std::to_string((int)(avgFrametime * 10.f) % 10) + "ms");
    cairo_show_text(g_pDebugOverlay->m_pCairo, text.c_str());
    cairo_text_extents(g_pDebugOverlay->m_pCairo, text.c_str(), &cairoExtents);
    if (cairoExtents.width > maxX) maxX = cairoExtents.width;

    yOffset += 11;
    cairo_move_to(g_pDebugOverlay->m_pCairo, 0, yOffset);
    text = std::string("Avg Rendertime: " + std::to_string((int)avgRenderTime) + "." + std::to_string((int)(avgRenderTime * 10.f) % 10) + "ms");
    cairo_show_text(g_pDebugOverlay->m_pCairo, text.c_str());
    cairo_text_extents(g_pDebugOverlay->m_pCairo, text.c_str(), &cairoExtents);
    if (cairoExtents.width > maxX) maxX = cairoExtents.width;

    yOffset += 11;
    cairo_move_to(g_pDebugOverlay->m_pCairo, 0, yOffset);
    text = std::string("Avg Rendertime (no overlay): " + std::to_string((int)avgRenderTimeNoOverlay) + "." + std::to_string((int)(avgRenderTimeNoOverlay * 10.f) % 10) + "ms");
    cairo_show_text(g_pDebugOverlay->m_pCairo, text.c_str());
    cairo_text_extents(g_pDebugOverlay->m_pCairo, text.c_str(), &cairoExtents);
    if (cairoExtents.width > maxX) maxX = cairoExtents.width;

    yOffset += 11;

    g_pHyprRenderer->damageBox(&m_wbLastDrawnBox);
    m_wbLastDrawnBox = {(int)g_pCompositor->m_vMonitors.front()->vecPosition.x, (int)g_pCompositor->m_vMonitors.front()->vecPosition.y + offset - 1, (int)maxX + 2, yOffset - offset + 2};
    g_pHyprRenderer->damageBox(&m_wbLastDrawnBox);

    return yOffset - offset;
}

void CHyprDebugOverlay::renderData(SMonitor* pMonitor, float µs) {
    m_mMonitorOverlays[pMonitor].renderData(pMonitor, µs);
}

void CHyprDebugOverlay::renderDataNoOverlay(SMonitor* pMonitor, float µs) {
    m_mMonitorOverlays[pMonitor].renderDataNoOverlay(pMonitor, µs);
}

void CHyprDebugOverlay::frameData(SMonitor* pMonitor) {
    m_mMonitorOverlays[pMonitor].frameData(pMonitor);
}

void CHyprDebugOverlay::draw() {

    const auto PMONITOR = g_pCompositor->m_vMonitors.front().get();

    if (!m_pCairoSurface || !m_pCairo) {
        m_pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PMONITOR->vecSize.x, PMONITOR->vecSize.y);
        m_pCairo = cairo_create(m_pCairoSurface);
    }

    // clear the pixmap
    cairo_save(m_pCairo);
    cairo_set_operator(m_pCairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_pCairo);
    cairo_restore(m_pCairo);

    // draw the things
    int offsetY = 0;
    for (auto& m : g_pCompositor->m_vMonitors) {
        offsetY += m_mMonitorOverlays[m.get()].draw(offsetY);
        offsetY += 5; // for padding between mons
    }

    cairo_surface_flush(m_pCairoSurface);

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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PMONITOR->vecSize.x, PMONITOR->vecSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    wlr_box pMonBox = {0,0,PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};
    g_pHyprOpenGL->renderTexture(m_tTexture, &pMonBox, 255.f);
}
