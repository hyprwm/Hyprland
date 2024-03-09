#include "Constraint.hpp"
#include "WLSurface.hpp"
#include "../Compositor.hpp"

CConstraint::CConstraint(wlr_pointer_constraint_v1* constraint, CWLSurface* owner) : m_pOwner(owner), m_pConstraint(constraint) {
    initSignals();

    m_vCursorPosOnActivate = g_pInputManager->getMouseCoordsInternal();

    g_pInputManager->m_vConstraints.push_back(this);

    if (g_pCompositor->m_pLastFocus == m_pOwner->wlr())
        activate();
}

CConstraint::~CConstraint() {
    std::erase(g_pInputManager->m_vConstraints, this);
}

static void onConstraintDestroy(void* owner, void* data) {
    const auto CONSTRAINT = (CConstraint*)owner;
    CONSTRAINT->onDestroy();
}

static void onConstraintSetRegion(void* owner, void* data) {
    const auto CONSTRAINT = (CConstraint*)owner;
    CONSTRAINT->onSetRegion();
}

void CConstraint::initSignals() {
    hyprListener_setConstraintRegion.initCallback(&m_pConstraint->events.set_region, ::onConstraintSetRegion, this, "CConstraint");
    hyprListener_destroyConstraint.initCallback(&m_pConstraint->events.destroy, ::onConstraintDestroy, this, "CConstraint");
}

void CConstraint::onDestroy() {
    if (active())
        deactivate();

    // this is us
    m_pOwner->m_pConstraint.reset();
}

void CConstraint::onSetRegion() {
    if (!m_bActive)
        return;

    m_rRegion.set(&m_pConstraint->region);
    m_vPositionHint = m_rRegion.closestPoint(m_vPositionHint);
    g_pInputManager->simulateMouseMovement(); // to warp the cursor if anything's amiss
}

void CConstraint::onCommit() {
    if (!m_bActive)
        return;

    const auto COMMITTED = m_pConstraint->current.committed;

    if (COMMITTED & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
        m_bHintSet      = true;
        m_vPositionHint = {m_pConstraint->current.cursor_hint.x, m_pConstraint->current.cursor_hint.y};
        g_pInputManager->simulateMouseMovement();
    }

    if (COMMITTED & WLR_POINTER_CONSTRAINT_V1_STATE_REGION)
        onSetRegion();
}

CRegion CConstraint::logicConstraintRegion() {
    CRegion    rg            = m_rRegion;
    const auto SURFBOX       = m_pOwner->getSurfaceBoxGlobal();
    const auto CONSTRAINTPOS = SURFBOX.has_value() ? SURFBOX->pos() : Vector2D{};
    rg.translate(CONSTRAINTPOS);
    return rg;
}

CWLSurface* CConstraint::owner() {
    return m_pOwner;
}

bool CConstraint::isLocked() {
    return m_pConstraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED;
}

Vector2D CConstraint::logicPositionHint() {
    const auto SURFBOX       = m_pOwner->getSurfaceBoxGlobal();
    const auto CONSTRAINTPOS = SURFBOX.has_value() ? SURFBOX->pos() : Vector2D{};

    return m_bHintSet ? CONSTRAINTPOS + m_vPositionHint : m_vCursorPosOnActivate;
}

void CConstraint::deactivate() {
    if (!m_bActive)
        return;

    wlr_pointer_constraint_v1_send_deactivated(m_pConstraint);
    m_bActive = false;

    if (isLocked())
        g_pCompositor->warpCursorTo(logicPositionHint(), true);

    if (m_pConstraint->lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT)
        m_bDead = true;
}

void CConstraint::activate() {
    if (m_bActive || m_bDead)
        return;

    // TODO: hack, probably not a super duper great idea
    if (g_pCompositor->m_sSeat.seat->pointer_state.focused_surface != m_pOwner->wlr()) {
        const auto SURFBOX = m_pOwner->getSurfaceBoxGlobal();
        const auto LOCAL   = SURFBOX.has_value() ? logicPositionHint() - SURFBOX->pos() : Vector2D{};
        wlr_seat_pointer_enter(g_pCompositor->m_sSeat.seat, m_pOwner->wlr(), LOCAL.x, LOCAL.y);
    }

    g_pCompositor->warpCursorTo(logicPositionHint(), true);
    wlr_pointer_constraint_v1_send_activated(m_pConstraint);
    m_bActive = true;
}

bool CConstraint::active() {
    return m_bActive;
}
