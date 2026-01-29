#include "CursorZoomGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../helpers/Monitor.hpp"

CCursorZoomTrackpadGesture::CCursorZoomTrackpadGesture(const std::string& first, const std::string& second) {
    try {
        m_zoomValue = std::stof(first);
    } catch (...) { ; }

    if (second == "mult")
        m_mode = MODE_MULT;
}

void CCursorZoomTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    if (m_mode == MODE_TOGGLE)
        m_zoomed = !m_zoomed;

    for (auto const& m : g_pCompositor->m_monitors) {
        switch (m_mode) {
            case MODE_TOGGLE:
                static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
                *m->m_cursorZoom        = m_zoomed ? m_zoomValue : *PZOOMFACTOR;
                break;
            case MODE_MULT: *m->m_cursorZoom = std::clamp(m->m_cursorZoom->goal() * m_zoomValue, 1.0F, 100.0F); break;
        }
    }
}

void CCursorZoomTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {}
void CCursorZoomTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {}
