#include "WorkspaceCaptureSource.hpp"
#include "../../output/Monitor.hpp"
#include "../../workspace/HLWorkspace.hpp"

using namespace Screenshare;

CWorkspaceCaptureSource::CWorkspaceCaptureSource(PHLWORKSPACE workspace, uint32_t mode) : m_workspace(workspace), m_removed(!workspace), m_mode(mode) {
    if (!workspace)
        return;

    m_name = workspace->addressableName();
    updateMonitor();

    m_listeners.destroy        = workspace->m_events.destroy.listen([this]() {
        m_removed   = true;
        m_workspace = nullptr;
        m_listeners = {};
        m_changed.emit();
    });
    m_listeners.monitorChanged = workspace->m_events.monitorChanged.listen([this]() {
        updateMonitor();
        m_changed.emit();
    });
    const auto updateName      = [this]() {
        if (const auto WORKSPACE = m_workspace.lock())
            m_name = WORKSPACE->addressableName();
        m_changed.emit();
    };
    m_listeners.renamed   = workspace->m_events.renamed.listen(updateName);
    m_listeners.idChanged = workspace->m_events.idChanged.listen(updateName);
}

PHLWORKSPACE CWorkspaceCaptureSource::workspace() const {
    return m_removed ? nullptr : m_workspace.lock();
}

PHLMONITOR CWorkspaceCaptureSource::monitor() const {
    if (m_removed)
        return m_lastMonitor.lock();

    const auto WORKSPACE = m_workspace.lock();
    return WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
}

Vector2D CWorkspaceCaptureSource::bufferSize() const {
    return m_bufferSize;
}

const std::string& CWorkspaceCaptureSource::name() const {
    return m_name;
}

bool CWorkspaceCaptureSource::removed() const {
    return m_removed;
}

uint32_t CWorkspaceCaptureSource::mode() const {
    return m_mode;
}

void CWorkspaceCaptureSource::updateMonitor() {
    m_listeners.modeChanged.reset();

    const auto MONITOR = monitor();
    if (!MONITOR)
        return;

    m_lastMonitor           = MONITOR;
    m_listeners.modeChanged = MONITOR->m_events.modeChanged.listen([this]() {
        updateSize();
        m_changed.emit();
    });
    updateSize();
}

void CWorkspaceCaptureSource::updateSize() {
    const auto MONITOR = monitor();
    if (!MONITOR || m_removed)
        return;

    const auto SIZE = MONITOR->m_transformedSize;
    if (SIZE.x > 0 && SIZE.y > 0)
        m_bufferSize = SIZE;
}
