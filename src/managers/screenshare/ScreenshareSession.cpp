#include "ScreenshareManager.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../EventManager.hpp"
#include "../eventLoop/EventLoopManager.hpp"
#include "../../event/EventBus.hpp"
#include <hyprgraphics/egl/Egl.hpp>

using namespace Hyprgraphics::Egl;
using namespace Screenshare;

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, wl_client* client) : m_type(SHARE_MONITOR), m_monitor(monitor), m_client(client) {
    if UNLIKELY (!m_monitor)
        return;

    init();
}

CScreenshareSession::CScreenshareSession(PHLWINDOW window, wl_client* client) : m_type(SHARE_WINDOW), m_window(window), m_client(client) {
    if UNLIKELY (!m_window)
        return;

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

CScreenshareSession::CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client) :
    m_type(SHARE_REGION), m_monitor(monitor), m_captureBox(captureRegion), m_client(client) {
    if UNLIKELY (!m_monitor)
        return;

    init();
}

CScreenshareSession::~CScreenshareSession() {
    stop();
    uintptr_t ptr = m_type == SHARE_WINDOW && !m_window.expired() ? (uintptr_t)m_window.get() : (m_monitor.expired() ? (uintptr_t)nullptr : (uintptr_t)m_monitor.get());
    LOGM(Log::TRACE, "Destroyed screenshare session for ({}): {}, {:x}", m_type, m_name, ptr);
}

void CScreenshareSession::stop() {
    if (m_stopped)
        return;
    m_stopped = true;

    screenshareEvents(false);
    m_events.stopped.emit();
}

bool CScreenshareSession::isActive() {
    return !m_stopped;
}

void CScreenshareSession::init() {
    uintptr_t ptr = m_type == SHARE_WINDOW && !m_window.expired() ? (uintptr_t)m_window.get() : (m_monitor.expired() ? (uintptr_t)nullptr : (uintptr_t)m_monitor.get());
    LOGM(Log::TRACE, "Created screenshare session for ({}): {}, {:x}", m_type, m_name, ptr);

    m_shareStopTimer = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(500),
        [this](SP<CEventLoopTimer> self, void* data) {
            // if this fires, then it's been half a second since the last frame, so we aren't sharing
            screenshareEvents(false);
        },
        nullptr);

    if (g_pEventLoopManager)
        g_pEventLoopManager->addTimer(m_shareStopTimer);

    // scale capture box since it's in logical coords; round to integer pixel
    // dims so m_bufferSize matches the int32 size we send to the client
    m_captureBox.scale(monitor()->m_scale).round();

    m_listeners.monitorDestroyed   = monitor()->m_events.disconnect.listen([this]() { stop(); });
    m_listeners.monitorModeChanged = monitor()->m_events.modeChanged.listen([this]() {
        calculateConstraints();
        m_events.constraintsChanged.emit();
    });
    m_listeners.configReloaded     = Event::bus()->m_events.config.reloaded.listen([this] {
        static const auto PFORCE8BIT = CConfigValue<Config::INTEGER>("misc:screencopy_force_8b");
        static auto       prev       = *PFORCE8BIT;
        if (prev != *PFORCE8BIT) {
            prev = *PFORCE8BIT;
            calculateConstraints();
        }
    });

    calculateConstraints();
}

void CScreenshareSession::calculateConstraints() {
    const auto PMONITOR = monitor();
    if (!PMONITOR) {
        stop();
        return;
    }

    // TODO: maybe support more that just monitor format in the future?
    m_formats.clear();
    const auto preferredReadFormat = PMONITOR->getPreferredReadFormat();
    auto       format              = getPixelFormatFromDRM(preferredReadFormat);

    if (!format) {
        LOGM(Log::ERR, "Missing support for format, BUG REPORT this");
        return;
    }

    // GL_BGRA_EXT only works with GL_UNSIGNED_BYTE 8bit formats, gles limitation
    // so force another format
    const auto readbackFormat = getReadbackFormat(*format);
    if (readbackFormat == GL_BGRA_EXT && format->glType != GL_UNSIGNED_BYTE) {
        const auto colorDepth = getColorDepth(*format);
        if (colorDepth <= 8)
            format = getPixelFormatFromDRM(DRM_FORMAT_XBGR8888);
        else if (colorDepth == 10)
            format = getPixelFormatFromDRM(DRM_FORMAT_XBGR2101010);
        else // 16bit
            format = getPixelFormatFromDRM(DRM_FORMAT_XBGR16161616);

        if (!format) {
            LOGM(Log::ERR, "Missing support for fallback format, BUG REPORT this");
            return;
        }
    }

    const auto altFormat = format->withAlpha ? format->alphaStripped : format->alphaAdded;
    if (altFormat != DRM_FORMAT_INVALID && altFormat != format->drmFormat)
        m_formats.push_back(altFormat);

    m_formats.push_back(format->drmFormat);

    switch (m_type) {
        case SHARE_MONITOR:
            m_bufferSize = PMONITOR->m_pixelSize;
            m_name       = PMONITOR->m_name;
            break;
        case SHARE_WINDOW:
            m_bufferSize = (m_window->m_realSize->value() * PMONITOR->m_scale).round();
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

void CScreenshareSession::screenshareEvents(bool startSharing) {
    if (startSharing && !m_sharing) {
        m_sharing = true;
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencast", .data = std::format("1,{}", m_type)});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencastv2", .data = std::format("1,{},{}", m_type, m_name)});
        LOGM(Log::INFO, "Started screenshare session for ({}): {}", m_type, m_name);

        Event::bus()->m_events.screenshare.state.emit(true, m_type, m_name);
    } else if (!startSharing && m_sharing) {
        m_sharing = false;
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencast", .data = std::format("0,{}", m_type)});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "screencastv2", .data = std::format("0,{},{}", m_type, m_name)});
        LOGM(Log::INFO, "Stopped screenshare session for ({}): {}", m_type, m_name);

        Event::bus()->m_events.screenshare.state.emit(false, m_type, m_name);
    }
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
    UP<CScreenshareFrame> frame = makeUnique<CScreenshareFrame>(m_self, overlayCursor, !m_sharing);
    frame->m_self               = frame;

    Screenshare::mgr()->m_pendingFrames.emplace_back(frame);

    // there is now a pending frame, so block ds
    g_pHyprRenderer->m_directScanoutBlocked = true;

    return frame;
}
