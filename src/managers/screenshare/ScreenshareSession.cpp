#include "ScreenshareManager.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool managed) : m_managed(managed), m_type(SHARE_MONITOR), m_monitor(monitor), m_client(client) {
    init();
}

CScreenshareSession::CScreenshareSession(PHLWINDOW window, wl_client* client, bool managed) : m_managed(managed), m_type(SHARE_WINDOW), m_window(window), m_client(client) {
    m_listeners.windowDestroyed   = m_window->m_events.unmap.listen([this]() { stop(); });
    m_listeners.windowSizeChanged = m_window->m_events.resize.listen([this]() {
        calculateConstraints();
        m_events.constraintsChanged.emit();
    });
    init();
}

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool managed) :
    m_managed(managed), m_type(SHARE_REGION), m_monitor(monitor), m_captureBox(captureRegion), m_client(client) {
    init();
}

CScreenshareSession::~CScreenshareSession() {
    stop();
    LOGM(TRACE, "Destroyed screenshare session for ({}): {}", m_type, m_name);
}

void CScreenshareSession::stop() {
    if (m_stopped)
        return;
    m_stopped = true;
    m_events.stopped.emit();

    if (!m_managed)
        g_pScreenshareManager->screenshareEvents(m_self, false);
}

void CScreenshareSession::init() {
    m_lastFrame.reset();

    m_listeners.monitorDestroyed   = monitor()->m_events.disconnect.listen([this]() { stop(); });
    m_listeners.monitorModeChanged = monitor()->m_events.modeChanged.listen([this]() {
        calculateConstraints();
        m_events.constraintsChanged.emit();
    });

    calculateConstraints();

    if (!m_managed)
        g_pScreenshareManager->screenshareEvents(m_self, true);
}

void CScreenshareSession::calculateConstraints() {
    const auto PMONITOR = monitor();

    // TODO: maybe support more that just monitor format in the future?
    m_formats.clear();
    m_formats.push_back(g_pHyprOpenGL->getPreferredReadFormat(PMONITOR));

    switch (m_type) {
        case SHARE_MONITOR:
            m_box  = {{}, PMONITOR->m_pixelSize};
            m_name = PMONITOR->m_name;
            break;
        case SHARE_WINDOW:
            m_box  = CBox{m_window->m_realPosition->value(), m_window->m_realSize->value()}.round();
            m_name = m_window->m_title;
            break;
        case SHARE_REGION:
            m_box  = m_captureBox.transform(wlTransformToHyprutils(PMONITOR->m_transform), PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);
            m_box  = m_box.scale(PMONITOR->m_scale).round();
            m_name = PMONITOR->m_name;
            break;
    }

    LOGM(LOG, "constraints changed for {}", m_name);
}

const std::vector<SDRMFormat>& CScreenshareSession::allowedFormats() const {
    return m_formats;
}

Vector2D CScreenshareSession::bufferSize() const {
    return m_box.size();
}

PHLMONITOR CScreenshareSession::monitor() const {
    PHLMONITORREF mon = m_type == SHARE_WINDOW ? m_window->m_monitor : m_monitor;
    return mon.expired() ? nullptr : mon.lock();
}

UP<CScreenshareFrame> CScreenshareSession::nextFrame(bool overlayCursor) {
    UP<CScreenshareFrame> frame = makeUnique<CScreenshareFrame>(m_self, overlayCursor);
    frame->m_self               = frame;

    g_pScreenshareManager->m_frames.emplace_back(frame);

    return frame;
}
