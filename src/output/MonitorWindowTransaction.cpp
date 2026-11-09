#include "MonitorWindowTransaction.hpp"

#include "Monitor.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../protocols/core/Compositor.hpp"

#include <algorithm>
#include <utility>

using namespace Monitor;
using namespace Desktop::View;

CMonitorWindowTransaction::CMonitorWindowTransaction(PHLMONITOR monitor) : m_monitor(monitor) {
    m_timeout = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer>, void*) {
            m_timedOut = true;
            requestApply();
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_timeout);
}

CMonitorWindowTransaction::~CMonitorWindowTransaction() {
    m_timeout->cancel();
    clear();
}

bool CMonitorWindowTransaction::queue(PHLWINDOW window, const CBox& box) {
    if (window->geometricBox(IGeometric::GEOMETRIC_GOAL) == box && (!window->m_pendingGeometry || window->m_geometryTransaction.get() == this))
        return window->m_pendingGeometry.has_value();

    const bool WARP = window->m_pendingGeometry && window->m_pendingGeometry->warp;
    if (window->m_geometryTransaction && window->m_geometryTransaction.get() != this)
        window->m_geometryTransaction->remove(window);

    const bool QUEUED = window->m_geometryTransaction.get() == this;
    window->stageGeometry(box);
    window->m_pendingGeometry->warp = WARP;

    if (!QUEUED) {
        if (m_pending.empty())
            m_timeout->updateTimeout(std::chrono::milliseconds(50));

        m_pending.emplace_back(window);
        window->m_geometryTransaction = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    }

    m_monitor->scheduleFrame();
    return true;
}

void CMonitorWindowTransaction::remove(PHLWINDOW window) {
    std::erase_if(m_pending, [&window](const auto& pending) { return !pending || pending == window; });

    if (window->m_geometryTransaction.get() == this) {
        window->m_geometryTransaction.reset();
        window->discardPendingGeometry();
    }

    if (m_pending.empty()) {
        m_timeout->updateTimeout(std::nullopt);
        m_timedOut = false;
    }
}

void CMonitorWindowTransaction::clear() {
    for (const auto& pending : std::exchange(m_pending, {})) {
        if (!pending || pending->m_geometryTransaction.get() != this)
            continue;

        pending->m_geometryTransaction.reset();
        pending->discardPendingGeometry();
    }

    if (!m_timeout->cancelled())
        m_timeout->updateTimeout(std::nullopt);
    m_timedOut = false;
}

bool CMonitorWindowTransaction::onlyMoves() const {
    return std::ranges::all_of(m_pending, [](const auto& window) { return !window || !window->pendingGeometryResizes(); });
}

bool CMonitorWindowTransaction::ready() const {
    return m_timedOut || std::ranges::any_of(m_pending, [this](const auto& window) {
               return window && window->m_pendingGeometry && window->m_pendingGeometry->ready && matchesPendingResize(window.lock());
           });
}

void CMonitorWindowTransaction::onPreRender() {
    m_rendering = true;

    if (ready() || onlyMoves())
        apply();
}

void CMonitorWindowTransaction::onPostRender() {
    m_rendering = false;

    if (ready())
        apply();
}

void CMonitorWindowTransaction::onWindowCommit(PHLWINDOW window) {
    if (window->m_geometryTransaction.get() != this || !window->m_pendingGeometry)
        return;

    const auto SURFACE = window->backend().surface();
    if (!SURFACE || !SURFACE->m_current.updated.bits.buffer)
        return;

    window->m_pendingGeometry->ready = SURFACE->m_current.buffer && matchesPendingResize(window);
    if (window->m_pendingGeometry->ready)
        requestApply();
}

bool CMonitorWindowTransaction::matchesPendingResize(PHLWINDOW window) const {
    if (window->m_geometryTransaction.get() != this || !window->pendingGeometryResizes())
        return false;

    const auto SURFACE = window->backend().surface();
    if (!SURFACE || !SURFACE->m_current.texture)
        return false;

    const auto EXPECTED = window->size(IGeometric::GEOMETRIC_GOAL).floor();
    if (window->backend().isX11())
        return window->backend().clientToLogical(CBox{{}, SURFACE->m_current.size}, m_monitor.lock()).size().round() == EXPECTED;

    const auto EXTENTS   = SURFACE->extends();
    const auto GEOMETRY  = window->backend().geometry().box;
    const auto EFFECTIVE = GEOMETRY.empty() ? EXTENTS : GEOMETRY.intersection(EXTENTS);
    return EFFECTIVE.size().round() == EXPECTED && window->backend().reportedSize() == EXPECTED;
}

void CMonitorWindowTransaction::requestApply() {
    if (!m_rendering && !m_applying)
        apply();
}

void CMonitorWindowTransaction::apply() {
    if (m_pending.empty() || m_applying)
        return;

    const auto SELF = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    m_applying      = true;
    m_timedOut      = false;
    m_timeout->updateTimeout(std::nullopt);

    auto pending = std::exchange(m_pending, {});

    // Detach the entire batch before animation/decorations callbacks can queue another one.
    // Keep the staged goals visible until each window is applied.
    for (const auto& window : pending) {
        if (window && window->m_geometryTransaction.get() == this)
            window->m_geometryTransaction.reset();
    }

    for (const auto& window : pending) {
        if (!window || window->m_geometryTransaction)
            continue;

        if (window->m_pendingGeometry && window->m_monitor && window->m_monitor != m_monitor && window->m_monitor->m_windowTransaction) {
            window->m_monitor->m_windowTransaction->queue(window.lock(), window->m_pendingGeometry->box);
            continue;
        }

        window->applyPendingGeometry();

        // Applying geometry can invoke hooks which disconnect/reconfigure the monitor.
        if (!SELF) {
            for (const auto& remaining : pending) {
                if (remaining && !remaining->m_geometryTransaction)
                    remaining->discardPendingGeometry();
            }
            return;
        }
    }

    m_applying = false;

    if (m_monitor)
        m_monitor->scheduleFrame();
}
