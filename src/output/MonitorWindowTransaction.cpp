#include "MonitorWindowTransaction.hpp"

#include "Monitor.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/state/WindowFadeout.hpp"
#include "../animation/WorkspaceAnimationController.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "../managers/fullscreen/FullscreenPresentation.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../workspace/HLWorkspace.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/Types.hpp"
#include "../layout/LayoutManager.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

#include <algorithm>
#include <utility>

using namespace Monitor;
using namespace Desktop::View;

static void releaseFullscreen(std::vector<SP<Fullscreen::SFullscreenPresentation>> snapshots) {
    for (auto& held : snapshots) {
        // Drop each reference before the next fade can reenter another owner.
        const auto snapshot  = std::move(held);
        const auto WORKSPACE = snapshot->workspace;
        // A transferred or requeued participant keeps the same hold in its next batch.
        if (snapshot.strongRef() != 1 || WORKSPACE->m_fullscreenPresentation != snapshot)
            continue;

        WORKSPACE->m_fullscreenPresentation.reset();
        Animation::Workspace::applyFullscreenFade(WORKSPACE);
        if (g_pHyprRenderer && WORKSPACE->m_monitor)
            g_pHyprRenderer->damageMonitor(WORKSPACE->m_monitor.lock());
    }
}

static void releaseClosingWindows(WP<CMonitorWindowTransaction> transaction, std::vector<SP<Desktop::CWindowFadeout>> fadeouts) {
    for (const auto& fadeout : fadeouts) {
        if (!transaction)
            fadeout->discard();
        else if (!transaction->holdClosingWindow(fadeout))
            fadeout->startAnimation();
    }
}

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

bool CMonitorWindowTransaction::queue(PHLWINDOW window, const CBox& box, bool force) {
    if (!force && window->geometricBox(IGeometric::GEOMETRIC_GOAL) == box && (!window->m_pendingGeometry || window->m_geometryTransaction.get() == this))
        return window->m_pendingGeometry.has_value();

    const bool WARP = window->m_pendingGeometry && window->m_pendingGeometry->warp;
    for (const auto& workspace : {window->presentation().workspace(), window->m_workspace}) {
        if (!workspace)
            continue;
        auto snapshot = workspace->m_fullscreenPresentation.lock();
        if (snapshot && std::ranges::find(m_fullscreen, snapshot) == m_fullscreen.end())
            m_fullscreen.emplace_back(std::move(snapshot));
    }
    if (window->m_geometryTransaction && window->m_geometryTransaction.get() != this)
        window->m_geometryTransaction->remove(window, false);

    const bool QUEUED = window->m_geometryTransaction.get() == this;
    window->stageGeometry(box);
    window->m_pendingGeometry->warp = WARP;

    if (!QUEUED) {
        static const auto PTIMEOUT = CConfigValue<Config::INTEGER>("misc:transaction_timeout");

        if (m_pending.empty() && !m_timeout->armed() && !m_timedOut)
            m_timeout->updateTimeout(std::chrono::milliseconds(*PTIMEOUT));

        m_pending.emplace_back(window);
        window->m_geometryTransaction = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    }

    m_monitor->scheduleFrame();
    return true;
}

void CMonitorWindowTransaction::remove(PHLWINDOW window, bool discard) {
    const auto                               SELF = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    std::vector<SP<Desktop::CWindowFadeout>> closingWindows;
    if (!m_layoutUpdateDepth)
        closingWindows = std::exchange(m_closingWindows, {});
    const Hyprutils::Utils::CScopeGuard closingGuard([SELF, fadeouts = std::move(closingWindows)]() mutable { releaseClosingWindows(SELF, std::move(fadeouts)); });
    std::erase_if(m_pending, [&window](const auto& pending) { return !pending || pending == window; });

    if (window->m_geometryTransaction.get() == this) {
        window->m_geometryTransaction.reset();
        if (discard)
            window->discardPendingGeometry();
    }

    if (!SELF)
        return;

    if (m_pending.empty() && !m_layoutUpdateDepth) {
        m_timeout->updateTimeout(std::nullopt);
        m_timedOut = false;
        releaseFullscreen(std::exchange(m_fullscreen, {}));
    }
}

void CMonitorWindowTransaction::beginLayoutUpdate() {
    ++m_layoutUpdateDepth;
}

void CMonitorWindowTransaction::endLayoutUpdate() {
    if (--m_layoutUpdateDepth)
        return;

    const auto                          SELF = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    const Hyprutils::Utils::CScopeGuard closingGuard([SELF, fadeouts = std::exchange(m_closingWindows, {})]() mutable { releaseClosingWindows(SELF, std::move(fadeouts)); });
    if (m_pending.empty()) {
        m_timeout->updateTimeout(std::nullopt);
        m_timedOut = false;
        releaseFullscreen(std::exchange(m_fullscreen, {}));
    }
    if (SELF)
        requestApply();
}

void CMonitorWindowTransaction::clear() {
    auto fullscreen     = std::exchange(m_fullscreen, {});
    auto closingWindows = std::exchange(m_closingWindows, {});
    if (!m_timeout->cancelled())
        m_timeout->updateTimeout(std::nullopt);
    m_timedOut = false;

    for (const auto& pending : std::exchange(m_pending, {})) {
        if (!pending || pending->m_geometryTransaction.get() != this)
            continue;

        pending->m_geometryTransaction.reset();
        pending->discardPendingGeometry();
    }

    releaseFullscreen(std::move(fullscreen));
    releaseClosingWindows(nullptr, std::move(closingWindows));
}

bool CMonitorWindowTransaction::holdClosingWindow(SP<Desktop::CWindowFadeout> fadeout) {
    if (!fadeout || fadeout->alpha() <= 0.F || !fadeout->workspace() || fadeout->monitor() != m_monitor || !std::ranges::any_of(m_pending, [&](const auto& window) {
            return window && window->mapped() && window->m_workspace == fadeout->workspace() && window->pendingGeometryResizes();
        }))
        return false;

    m_closingWindows.emplace_back(std::move(fadeout));
    return true;
}

void CMonitorWindowTransaction::holdFullscreen(PHLWORKSPACE workspace) {
    if (!workspace || std::ranges::any_of(m_fullscreen, [&](const auto& snapshot) { return snapshot == workspace->m_fullscreenPresentation; }))
        return;

    if (auto snapshot = workspace->m_fullscreenPresentation.lock()) {
        m_fullscreen.emplace_back(std::move(snapshot));
        return;
    }

    const auto SNAPSHOT = makeShared<Fullscreen::SFullscreenPresentation>(Fullscreen::SFullscreenPresentation{
        .workspace = workspace,
        .window    = Fullscreen::controller()->getPresentedFullscreenWindow(workspace),
        .modes     = Fullscreen::controller()->getPresentedFullscreenModes(workspace),
    });
    m_fullscreen.emplace_back(SNAPSHOT);
    workspace->m_fullscreenPresentation = SNAPSHOT;

    // Peers need their old stacking policy too, even when their size is unchanged.
    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->m_workspace == workspace)
            window->presentation().stageFullscreen();
    }
}

bool CMonitorWindowTransaction::hasPendingChanges() const {
    return m_applying || !m_pending.empty() || hasPendingPresentation();
}

bool CMonitorWindowTransaction::hasPendingPresentation() const {
    return !m_closingWindows.empty() || !m_fullscreen.empty() || (m_monitor && m_monitor->m_activeWorkspace && m_monitor->m_activeWorkspace->m_fullscreenPresentation) ||
        (m_monitor && m_monitor->m_activeSpecialWorkspace && m_monitor->m_activeSpecialWorkspace->m_fullscreenPresentation) ||
        std::ranges::any_of(m_pending, [](const auto& window) { return window && window->presentation().hasPendingState(); });
}

void CMonitorWindowTransaction::flush(PHLWINDOW window) {
    // Interactive fullscreen changes also take over the peers' presentation.
    if (!m_fullscreen.empty() || !m_closingWindows.empty()) {
        apply();
        return;
    }

    remove(window, false);
    window->applyPendingGeometry();
}

bool CMonitorWindowTransaction::onlyMoves() const {
    return std::ranges::all_of(m_pending, [](const auto& window) { return !window || !window->mapped() || !window->pendingGeometryResizes(); });
}

bool CMonitorWindowTransaction::ready() const {
    static const auto PTIMEOUT = CConfigValue<Config::INTEGER>("misc:transaction_timeout");
    if (m_timedOut || *PTIMEOUT == 0)
        return true;

    // Do not make transactions when we are dragging, this is terrible UX.
    if (g_layoutManager && g_layoutManager->dragController()->target())
        return true;

    bool hasResize = false;
    for (const auto& window : m_pending) {
        if (!window || !window->mapped() || !window->pendingGeometryResizes())
            continue;

        hasResize = true;
        if (!window->m_pendingGeometry->ready || !matchesPendingResize(window.lock()))
            return false;
    }

    // Move-only batches are applied at pre-render, not from commit callbacks.
    return hasResize;
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
    if (!m_rendering && !m_applying && ready())
        apply();
}

void CMonitorWindowTransaction::apply() {
    if (m_pending.empty() || m_applying || m_layoutUpdateDepth)
        return;

    const auto SELF = WP<CMonitorWindowTransaction>(m_monitor->m_windowTransaction);
    m_applying      = true;
    m_timedOut      = false;
    m_timeout->updateTimeout(std::nullopt);

    auto                                pending = std::exchange(m_pending, {});
    const Hyprutils::Utils::CScopeGuard closingGuard([SELF, fadeouts = std::exchange(m_closingWindows, {})]() mutable { releaseClosingWindows(SELF, std::move(fadeouts)); });
    const Hyprutils::Utils::CScopeGuard fullscreenGuard([snapshots = std::exchange(m_fullscreen, {})]() mutable { releaseFullscreen(std::move(snapshots)); });

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
