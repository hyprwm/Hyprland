#include "CursorZoomGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../managers/input/InputManager.hpp"
#include <hyprutils/string/Numeric.hpp>

CCursorZoomTrackpadGesture::CCursorZoomTrackpadGesture(const std::string& first, const std::string& second) {
    if (const auto n = Hyprutils::String::strToNumber<float>(first); n)
        m_zoomValue = n.value();

    if (second == "mult")
        m_mode = MODE_MULT;
    else if (second == "live")
        m_mode = MODE_LIVE;
}

void CCursorZoomTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    if (m_mode == MODE_LIVE) {
        if (!e.pinch)
            return;

        m_monitor = g_pCompositor->getMonitorFromCursor();
        if (!m_monitor)
            return;

        const auto PMONITOR = m_monitor.lock();
        if (!PMONITOR)
            return;

        m_zoomBegin = std::clamp(PMONITOR->m_cursorZoom->value(), 1.0F, 100.0F);
        PMONITOR->m_cursorZoom->setValueAndWarp(m_zoomBegin);
        PMONITOR->m_zoomController.pinAnchor(g_pInputManager->getMouseCoordsInternal() - PMONITOR->m_position);
        return;
    }

    if (m_mode == MODE_TOGGLE)
        m_zoomed = !m_zoomed;

    for (auto const& m : g_pCompositor->m_monitors) {
        switch (m_mode) {
            case MODE_TOGGLE:
                static auto PZOOMFACTOR = CConfigValue<Config::FLOAT>("cursor:zoom_factor");
                *m->m_cursorZoom        = m_zoomed ? m_zoomValue : *PZOOMFACTOR;
                break;
            case MODE_MULT: *m->m_cursorZoom = std::clamp(m->m_cursorZoom->goal() * m_zoomValue, 1.0F, 100.0F); break;
            case MODE_LIVE: break;
        }
    }
}

void CCursorZoomTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (m_mode != MODE_LIVE || !m_monitor || !e.pinch)
        return;

    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR)
        return;

    auto zoom = std::clamp(m_zoomBegin * static_cast<float>(e.pinch->scale), 1.0F, 100.0F);

    if (zoom < 1.05F)
        zoom = 1.0F;

    PMONITOR->m_cursorZoom->setValueAndWarp(zoom);
}

void CCursorZoomTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (m_mode != MODE_LIVE || !m_monitor)
        return;

    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR)
        return;

    PMONITOR->m_zoomController.clearAnchor();
    m_monitor.reset();
}
