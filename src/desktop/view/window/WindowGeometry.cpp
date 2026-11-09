#include "Window.hpp"
#include "WindowPresentation.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorWindowTransaction.hpp"
#include "../../../render/Renderer.hpp"

#include <utility>

using namespace Desktop::View;

Vector2D CWindow::position(eGeometricValueType type) const {
    if (type == GEOMETRIC_GOAL && m_pendingGeometry)
        return m_pendingGeometry->box.pos();

    return CGeometricMovableAnimated::position(type);
}

Vector2D CWindow::size(eGeometricValueType type) const {
    if (type == GEOMETRIC_GOAL && m_pendingGeometry)
        return m_pendingGeometry->box.size();

    return CGeometricMovableAnimated::size(type);
}

CBox CWindow::geometricBox(eGeometricValueType type) const {
    return {position(type), size(type)};
}

void CWindow::move(const Vector2D& position) {
    if (m_pendingGeometry) {
        setBox({position, size(GEOMETRIC_GOAL)});
        return;
    }

    CGeometricMovableAnimated::move(position);
}

void CWindow::resize(const Vector2D& size) {
    if (m_pendingGeometry) {
        setBox({position(GEOMETRIC_GOAL), size});
        return;
    }

    CGeometricMovableAnimated::resize(size);
}

void CWindow::setBox(const CBox& box) {
    if (m_pendingGeometry && canStageGeometry() && m_monitor && m_monitor->m_windowTransaction) {
        m_monitor->m_windowTransaction->queue(m_self.lock(), box);
        return;
    }

    cancelPendingGeometry();

    CGeometricMovableAnimated::setBox(box);
}

void CWindow::finishAnimation() {
    if (m_pendingGeometry) {
        m_pendingGeometry->warp = true;
        return;
    }

    CGeometricMovableAnimated::finishAnimation();
}

void CWindow::stageGeometry(const CBox& box) {
    const bool WARP   = m_pendingGeometry && m_pendingGeometry->warp;
    const bool READY  = m_pendingGeometry && m_pendingGeometry->ready && m_pendingGeometry->box.size() == box.size();
    m_pendingGeometry = SPendingGeometry{.box = box, .warp = WARP, .ready = READY};
}

bool CWindow::canStageGeometry() const {
    return mapped() && !m_geometryTransactionsBlocked && !backend().traits().overrideRedirect;
}

void CWindow::flushPendingGeometry() {
    const auto PENDING = m_pendingGeometry;
    cancelPendingGeometry();
    m_pendingGeometry = PENDING;
    applyPendingGeometry();
}

bool CWindow::pendingGeometryResizes() const {
    return m_pendingGeometry && m_pendingGeometry->box.size() != CGeometricMovableAnimated::size(GEOMETRIC_GOAL);
}

void CWindow::applyPendingGeometry() {
    const auto PENDING = std::exchange(m_pendingGeometry, std::nullopt);
    if (!PENDING || !mapped())
        return;

    const auto SELF = m_self.lock();
    g_pHyprRenderer->damageWindow(SELF);

    CGeometricMovableAnimated::setBox(PENDING->box);

    static auto PANIMATIONS   = CConfigValue<Config::INTEGER>("animations:enabled");
    const bool  WARP_POSITION = PENDING->warp || m_ruleApplicator->noAnim().valueOr(!*PANIMATIONS || !m_realPosition->enabled());
    const bool  WARP_SIZE     = PENDING->warp || m_ruleApplicator->noAnim().valueOr(!*PANIMATIONS || !m_realSize->enabled());

    if (WARP_POSITION)
        m_realPosition->warp();
    if (WARP_SIZE)
        m_realSize->warp();

    presentation().updateDecorations();
    g_pHyprRenderer->damageWindow(SELF);
}

void CWindow::discardPendingGeometry() {
    m_pendingGeometry.reset();
}

void CWindow::cancelPendingGeometry() {
    if (m_geometryTransaction)
        m_geometryTransaction->remove(m_self.lock());
    else
        discardPendingGeometry();
}
