#include "ScreenshareManager.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor) :
    m_type(SHARE_MONITOR), m_monitor(monitor), m_client(client), m_overlayCursor(overlayCursor) {
    init();
}

CScreenshareSession::CScreenshareSession(PHLWINDOW window, wl_client* client, bool overlayCursor) :
    m_type(SHARE_WINDOW), m_monitor(window->m_monitor), m_window(window), m_client(client), m_overlayCursor(overlayCursor) {
    // TODO: should this be m_events.unmap? or both?
    m_listeners.windowDestroyed = m_window->m_events.destroy.listen([this]() { stop(); });
    // m_listeners.windowSizeChanged  = m_window->m_events.sizeChanged.listen([this]() { ; }); // TODO: why is this not a thing...
    init();
}

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor) :
    m_type(SHARE_REGION), m_monitor(monitor), m_captureRegion(captureRegion), m_client(client), m_overlayCursor(overlayCursor) {
    init();
}

CScreenshareSession::~CScreenshareSession() {
    stop();
    LOGM(LOG, "Destroyed screenshare session for {}", typeAndName());
}

void CScreenshareSession::stop() {
    if (m_stopped)
        return;
    m_stopped = true;
    m_events.stopped.emit();

    LOGM(LOG, "Stopped screenshare session for {}", typeAndName());
}

Vector2D CScreenshareSession::getBufferSize() {
    return m_box.size();
}

const std::vector<SDRMFormat>& CScreenshareSession::allowedFormats() {
    return m_formats;
}

void CScreenshareSession::init() {
    m_listeners.monitorDestroyed   = m_monitor->m_events.disconnect.listen([this]() { stop(); });
    m_listeners.monitorModeChanged = m_monitor->m_events.modeChanged.listen([this]() {
        calculateConstraints();
        m_events.constraintsChanged.emit();
    });

    calculateConstraints();

    LOGM(LOG, "New screenshare session for {}", typeAndName());
}

void CScreenshareSession::calculateConstraints() {
    // TODO: maybe support more that just monitor format in the future?
    m_formats.clear();
    m_formats.push_back(g_pHyprOpenGL->getPreferredReadFormat(m_monitor.lock()));

    switch (m_type) {
        case SHARE_MONITOR: m_box = {{}, {m_monitor->m_pixelSize}}; break;
        case SHARE_WINDOW: m_box = {{m_window->m_realPosition->value()}, {m_window->m_realSize->value()}}; break;
        case SHARE_REGION:
            m_box = m_captureRegion.transform(wlTransformToHyprutils(m_monitor->m_transform), m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y);
            m_box = m_box.scale(m_monitor->m_scale).round();
            break;
    }
}

std::string CScreenshareSession::typeAndName() {
    std::string type, name;
    switch (m_type) {
        case SHARE_MONITOR:
            type = "monitor";
            name = m_monitor->m_name;
            break;
        case SHARE_WINDOW:
            type = "window";
            name = m_window->m_title;
            break;
        case SHARE_REGION:
            type = "region";
            name = m_monitor->m_name;
            break;
    }

    return std::format("({}): \"{}\"", type, name);
}

eScreenshareError CScreenshareSession::shareNextFrame(SP<IHLBuffer> buffer, FScreenshareCallback callback) {
    if UNLIKELY (m_stopped)
        return ERROR_STOPPED;

    if UNLIKELY (!m_monitor || !g_pCompositor->monitorExists(m_monitor.lock())) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        stop();
        return ERROR_MONITOR;
    }

    if UNLIKELY (m_type == SHARE_WINDOW && (!m_window || !m_window->m_isMapped)) {
        LOGM(ERR, "Client requested sharing of window that is gone or not shareable!");
        stop();
        return ERROR_WINDOW;
    }

    if UNLIKELY (!buffer) {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_BUFFER;
    }

    if UNLIKELY (buffer->size != getBufferSize()) {
        LOGM(ERR, "Client requested sharing to an invalid buffer size");
        return ERROR_BUFFER_SIZE;
    }

    uint32_t bufFormat;
    if (buffer->dmabuf().success) {
        bufFormat = buffer->dmabuf().format;
    } else if (buffer->shm().success) {
        bufFormat = buffer->shm().format;
    } else {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_BUFFER;
    }

    if (std::ranges::count_if(allowedFormats(), [&](const SDRMFormat& format) { return format.drmFormat == bufFormat; }) == 0) {
        LOGM(ERR, "Invalid format {} in {:x}", bufFormat, (uintptr_t)this);
        return ERROR_BUFFER_FORMAT;
    }

    g_pScreenshareManager->m_frames.emplace_back(makeUnique<CScreenshareFrame>(m_self, buffer, callback));

    g_pHyprRenderer->m_directScanoutBlocked = true;

    return ERROR_NONE;
}
