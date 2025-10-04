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

    if (g_pScreenshareManager && !m_managed)
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

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
    for (auto& format : m_formats) {
        if (format == DRM_FORMAT_XRGB2101010 || format == DRM_FORMAT_ARGB2101010)
            format = DRM_FORMAT_XBGR2101010;
    }

    switch (m_type) {
        case SHARE_MONITOR:
            m_box  = {{}, PMONITOR->m_pixelSize};
            m_name = PMONITOR->m_name;
            break;
        case SHARE_WINDOW:
            m_box = CBox{m_window->m_realPosition->value(), m_window->m_realSize->value()};
            m_box.transform(wlTransformToHyprutils(PMONITOR->m_transform), PMONITOR->m_transformedSize.y, PMONITOR->m_transformedSize.x);
            m_box.scale(PMONITOR->m_scale).round();
            m_name = m_window->m_title;
            break;
        case SHARE_REGION:
            m_box = m_captureBox.transform(wlTransformToHyprutils(PMONITOR->m_transform), PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);
            m_box.scale(PMONITOR->m_scale).round();
            m_name = PMONITOR->m_name;
            break;
    }

    LOGM(LOG, "constraints changed for {}", m_name);

    // schedule a frame so that when a screenshare starts it isn't black until the output is updated
    g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
}

const std::vector<DRMFormat>& CScreenshareSession::allowedFormats() const {
    return m_formats;
}

Vector2D CScreenshareSession::bufferSize() const {
    return m_box.size();
}

PHLMONITOR CScreenshareSession::monitor() const {
    if (m_type == SHARE_WINDOW && m_window.expired())
        return nullptr;
    PHLMONITORREF mon = m_type == SHARE_WINDOW ? m_window->m_monitor : m_monitor;
    return mon.expired() ? nullptr : mon.lock();
}

UP<CScreenshareFrame> CScreenshareSession::nextFrame(bool overlayCursor) {
    UP<CScreenshareFrame> frame = makeUnique<CScreenshareFrame>(m_self, overlayCursor, m_frameCounter == 0);
    frame->m_self               = frame;

    g_pScreenshareManager->m_frames.emplace_back(frame);

    return frame;
}
