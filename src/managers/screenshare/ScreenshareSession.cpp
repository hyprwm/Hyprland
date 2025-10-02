#include "ScreenshareManager.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

// TODO:
//	m_box = ?
//	m_bufferSize = ?

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor) :
    m_type(SHARE_MONITOR), m_monitor(monitor), m_client(client), m_overlayCursor(overlayCursor) {
    m_listeners.sourceDestroyed = m_monitor->m_events.disconnect.registerListener([this](std::any data) { stop(); });

    m_box          = {{}, m_monitor->m_size};
    const auto POS = m_box.pos() * m_monitor->m_scale;
    m_box.transform(wlTransformToHyprutils(m_monitor->m_transform), m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y).scale(m_monitor->m_scale).round();
    m_box.x = POS.x;
    m_box.y = POS.y;
}

CScreenshareSession::CScreenshareSession(PHLWINDOW window, wl_client* client, bool overlayCursor) :
    m_type(SHARE_WINDOW), m_monitor(window->m_monitor), m_window(window), m_client(client), m_overlayCursor(overlayCursor) {
    ;
    // should this be m_events.unmap? or both?
    m_listeners.sourceDestroyed = m_window->m_events.destroy.registerListener([this](std::any data) { stop(); });

    m_box = {{}, m_window->m_realSize->value() * m_monitor->m_scale};
    m_box.transform(wlTransformToHyprutils(m_monitor->m_transform), m_monitor->m_transformedSize.x, m_monitor->m_transformedSize.y).round();
}

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor) :
    m_type(SHARE_REGION), m_monitor(monitor), m_client(client), m_overlayCursor(overlayCursor), m_box(captureRegion) {
    m_listeners.sourceDestroyed = m_monitor->m_events.disconnect.registerListener([this](std::any data) { stop(); });

    const auto POS = m_box.pos() * m_monitor->m_scale;
    m_box.transform(wlTransformToHyprutils(m_monitor->m_transform), m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y).scale(m_monitor->m_scale).round();
    m_box.x = POS.x;
    m_box.y = POS.y;
}

CScreenshareSession::~CScreenshareSession() {
    stop();
}

Vector2D CScreenshareSession::getBufferSize() {
    return m_box.size();
}

void CScreenshareSession::stop() {
    m_stopped = true;
    m_events.stopped.emit();
}

std::vector<uint32_t> CScreenshareSession::allowedFormats() {
    // TODO: support more that just monitor format in the future
    std::vector<uint32_t> formats;
    formats.push_back(m_monitor->m_output->state->state().drmFormat);
    return formats;
}

bool CScreenshareSession::shareNextFrame(SP<IHLBuffer> buffer, FScreenshareCallback callback) {
    if UNLIKELY (m_stopped)
        return false;

    if UNLIKELY (!m_monitor || !g_pCompositor->monitorExists(m_monitor.lock())) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        stop();
        return false;
    }

    if UNLIKELY (m_type == SHARE_WINDOW && (!m_window || !m_window->m_isMapped)) {
        LOGM(ERR, "Client requested sharing of window that is gone or not shareable!");
        stop();
        return false;
    }

    if UNLIKELY (!buffer || buffer->size != getBufferSize()) {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return false;
    }

    auto formats = allowedFormats();
    if (auto attrs = buffer->dmabuf(); attrs.success && std::count(formats.begin(), formats.end(), attrs.format) != 0) {
        LOGM(ERR, "Invalid dmabuf format in {:x}", (uintptr_t)this);
        return false;
    } else if (auto attrs = buffer->shm(); attrs.success && std::count(formats.begin(), formats.end(), attrs.format) != 0) {
        LOGM(ERR, "Invalid shm format in {:x}", (uintptr_t)this);
        return false;
    } else {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return false;
    }

    g_pScreenshareManager->m_frames.emplace_back(m_self, buffer, callback);

    g_pHyprRenderer->m_directScanoutBlocked = true;

    return true;
}
