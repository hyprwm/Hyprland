#include "ScreenshareManager.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool managed) : m_managed(managed), m_type(SHARE_MONITOR), m_monitor(monitor), m_client(client) {
    init();
}

CScreenshareSession::CScreenshareSession(PHLWINDOW window, wl_client* client, bool managed) : m_managed(managed), m_type(SHARE_WINDOW), m_window(window), m_client(client) {
    m_listeners.windowDestroyed      = m_window->m_events.unmap.listen([this]() { stop(); });
    m_listeners.windowSizeChanged    = m_window->m_events.resize.listen([this]() {
        calculateConstraints();
        m_events.constraintsChanged.emit();
    });
    m_listeners.windowMonitorChanged = m_window->m_events.monitorChanged.listen([this]() {
        m_listeners.monitorDestroyed   = monitor()->m_events.disconnect.listen([this]() { stop(); });
        m_listeners.monitorModeChanged = monitor()->m_events.modeChanged.listen([this]() {
            calculateConstraints();
            m_events.constraintsChanged.emit();
        });

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
    LOGM(Log::TRACE, "Destroyed screenshare session for ({}): {}", m_type, m_name);
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
    // scale capture box since it's in logical coords
    m_captureBox.scale(monitor()->m_scale);

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
    if (!PMONITOR) {
        stop();
        return;
    }

    // TODO: maybe support more that just monitor format in the future?
    m_formats.clear();
    m_formats.push_back(NFormatUtils::alphaFormat(g_pHyprOpenGL->getPreferredReadFormat(PMONITOR)));
    m_formats.push_back(g_pHyprOpenGL->getPreferredReadFormat(PMONITOR)); // some clients don't like alpha formats

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
    for (auto& format : m_formats) {
        if (format == DRM_FORMAT_XRGB2101010 || format == DRM_FORMAT_ARGB2101010)
            format = DRM_FORMAT_XBGR2101010;
    }

    switch (m_type) {
        case SHARE_MONITOR:
            m_bufferSize = PMONITOR->m_pixelSize;
            m_name       = PMONITOR->m_name;
            break;
        case SHARE_WINDOW:
            m_bufferSize = m_window->m_realSize->value().round();
            m_name       = m_window->m_title;
            break;
        case SHARE_REGION:
            m_bufferSize = PMONITOR->m_transform % 2 == 0 ? m_captureBox.size() : Vector2D{m_captureBox.h, m_captureBox.w};
            m_name       = PMONITOR->m_name;
            break;
        case SHARE_NONE:
        default:
            LOGM(Log::ERR, "Invalid share type?? This shouldn't happen");
            stop();
            return;
    }

    LOGM(Log::TRACE, "constraints changed for {}", m_name);
}

const std::vector<DRMFormat>& CScreenshareSession::allowedFormats() const {
    return m_formats;
}

Vector2D CScreenshareSession::bufferSize() const {
    return m_bufferSize;
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
