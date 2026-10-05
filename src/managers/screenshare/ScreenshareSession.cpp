#include "ScreenshareManager.hpp"
#include "WorkspaceCaptureSource.hpp"
#include "../../render/OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../../ipc/s2/S2.hpp"
#include "../eventLoop/EventLoopManager.hpp"
#include "../../event/EventBus.hpp"

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
        const auto PMONITOR = monitor();
        if (!PMONITOR) {
            stop();
            return;
        }

        m_listeners.monitorDestroyed   = PMONITOR->m_events.disconnect.listen([this]() { stop(); });
        m_listeners.monitorModeChanged = PMONITOR->m_events.modeChanged.listen([this]() {
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

CScreenshareSession::CScreenshareSession(SP<CWorkspaceCaptureSource> workspace, wl_client* client) : m_type(SHARE_WORKSPACE), m_workspace(std::move(workspace)), m_client(client) {
    m_listeners.workspaceChanged = m_workspace->m_changed.listen([this]() {
        if (m_stopped)
            return;

        const auto PMONITOR = monitor();
        if (!PMONITOR) {
            stop();
            return;
        }

        m_listeners.monitorDestroyed = PMONITOR->m_events.disconnect.listen([this]() { stop(); });
        const auto OLD_SIZE          = m_bufferSize;
        calculateConstraints();
        if (OLD_SIZE != m_bufferSize)
            m_events.constraintsChanged.emit();

        PMONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
        g_pHyprRenderer->damageMonitor(PMONITOR);
    });

    init();
}

CScreenshareSession::~CScreenshareSession() {
    stop();
    uintptr_t ptr = m_type == SHARE_WINDOW && !m_window.expired() ? (uintptr_t)m_window.get() : (m_monitor.expired() ? (uintptr_t)nullptr : (uintptr_t)m_monitor.get());
    LOG(Log::TRACE, "Destroyed screenshare session for ({}): {}, {:x}", m_type, m_name, ptr);
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

bool CScreenshareSession::isStale() {
    return m_stale;
}

void CScreenshareSession::init() {
    const auto PMONITOR = monitor();
    if (!PMONITOR) {
        stop();
        return;
    }

    uintptr_t ptr = m_type == SHARE_WINDOW && !m_window.expired() ? (uintptr_t)m_window.get() : (m_monitor.expired() ? (uintptr_t)nullptr : (uintptr_t)m_monitor.get());
    LOG(Log::TRACE, "Created screenshare session for ({}): {}, {:x}", m_type, m_name, ptr);

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
    m_captureBox.scale(PMONITOR->m_scale).round();

    m_listeners.monitorDestroyed = PMONITOR->m_events.disconnect.listen([this]() { stop(); });
    if (m_type != SHARE_WORKSPACE)
        m_listeners.monitorModeChanged = PMONITOR->m_events.modeChanged.listen([this]() {
            calculateConstraints();
            m_events.constraintsChanged.emit();
        });

    calculateConstraints();
}

void CScreenshareSession::calculateConstraints() {
    if (m_type == SHARE_WORKSPACE) {
        // Keep alpha available before removal so existing buffers can represent an empty source.
        m_formats = {
            DRM_FORMAT_ARGB8888,
        };
        m_bufferSize = m_workspace->bufferSize();
        m_name       = m_workspace->name();
        if (m_bufferSize.x <= 0 || m_bufferSize.y <= 0)
            stop();
        return;
    }

    const auto PMONITOR = monitor();
    if (!PMONITOR) {
        stop();
        return;
    }

    // TODO: maybe support more that just monitor format in the future?
    m_formats.clear();
    m_formats.push_back(NFormatUtils::alphaFormat(PMONITOR->getPreferredReadFormat()));
    m_formats.push_back(PMONITOR->getPreferredReadFormat()); // some clients don't like alpha formats

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
    for (auto& format : m_formats) {
        if (format == DRM_FORMAT_XRGB2101010 || format == DRM_FORMAT_ARGB2101010)
            format = DRM_FORMAT_XBGR2101010;
    }

    switch (m_type) {
        case SHARE_MONITOR:
            m_bufferSize = PMONITOR->m_transformedSize;
            m_name       = PMONITOR->m_name;
            break;
        case SHARE_WINDOW:
            m_bufferSize = (m_window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) * PMONITOR->m_scale).round();
            m_name       = m_window->metadata().title();
            break;
        case SHARE_REGION:
            m_bufferSize = m_captureBox.size();
            m_name       = PMONITOR->m_name;
            break;
        case SHARE_NONE:
        default:
            LOG(Log::ERR, "Invalid share type?? This shouldn't happen");
            stop();
            return;
    }

    LOG(Log::TRACE, "constraints changed for {}", m_name);
}

void CScreenshareSession::screenshareEvents(bool startSharing) {
    m_stale = !startSharing;
    if (startSharing && !m_sharing) {
        m_sharing = true;
        IPC::Socket2::sock()->postEvent({.event = "screencast", .data = std::format("1,{}", m_type)});
        IPC::Socket2::sock()->postEvent({.event = "screencastv2", .data = std::format("1,{},{}", m_type, m_name)});
        LOG(Log::INFO, "Started screenshare session for ({}): {}", m_type, m_name);

        Event::bus()->m_events.screenshare.state.emit(true, m_type, m_name);
    } else if (!startSharing && m_sharing) {
        m_sharing = false;
        IPC::Socket2::sock()->postEvent({.event = "screencast", .data = std::format("0,{}", m_type)});
        IPC::Socket2::sock()->postEvent({.event = "screencastv2", .data = std::format("0,{},{}", m_type, m_name)});
        LOG(Log::INFO, "Stopped screenshare session for ({}): {}", m_type, m_name);

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
    if (m_type == SHARE_WORKSPACE)
        return m_workspace->monitor();

    if (m_type == SHARE_WINDOW && m_window.expired())
        return nullptr;
    PHLMONITORREF mon = m_type == SHARE_WINDOW ? m_window->m_monitor : m_monitor;
    return mon.expired() ? nullptr : mon.lock();
}

UP<CScreenshareFrame> CScreenshareSession::nextFrame(bool overlayCursor) {
    UP<CScreenshareFrame> frame = makeUnique<CScreenshareFrame>(m_self, overlayCursor, !m_sharing);
    frame->m_self               = frame;

    Screenshare::mgr()->m_pendingFrames.emplace_back(frame);

    return frame;
}
