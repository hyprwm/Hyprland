#include "PointerConstraintController.hpp"
#include "InputManager.hpp"
#include "../SeatManager.hpp"
#include "../../desktop/view/Popup.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/state/ViewState.hpp"
#include "../../event/EventBus.hpp"
#include "../../helpers/time/Time.hpp"
#include "../../pointer/PointerController.hpp"
#include "../../protocols/PointerConstraints.hpp"
#include "../../protocols/RelativePointer.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/core/DataDevice.hpp"
#include "../../protocols/core/Subcompositor.hpp"
#include "../../workspace/HLWorkspace.hpp"

static SP<CWLSurfaceResource> focusOwner(SP<CWLSurfaceResource> surface) {
    while (surface && surface->m_role->role() == SURFACE_ROLE_SUBSURFACE) {
        const auto SUB = sc<CSubsurfaceRole*>(surface->m_role.get())->m_subsurface.lock();
        surface        = SUB ? SUB->m_parent.lock() : nullptr;
    }

    const auto HLSURFACE = Desktop::View::CWLSurface::fromResource(surface);
    if (const auto POPUP = HLSURFACE ? Desktop::View::CPopup::fromView(HLSURFACE->view()) : nullptr) {
        if (const auto WINDOW = POPUP->windowOwner())
            return WINDOW->wlSurface()->resource();
        if (const auto LAYER = POPUP->layerOwner())
            return LAYER->wlSurface()->resource();
    }

    return surface;
}

static bool surfaceVisible(SP<Desktop::View::CWLSurface> surface) {
    const auto VIEW = surface ? surface->view() : nullptr;
    if (!VIEW || !VIEW->mapped() || !VIEW->acceptsInput())
        return false;

    const auto ROOT   = Desktop::View::CWLSurface::fromResource(focusOwner(surface->resource()));
    const auto WINDOW = ROOT ? Desktop::View::CWindow::fromView(ROOT->view()) : nullptr;
    return !WINDOW || (WINDOW->m_workspace && WINDOW->m_workspace->visible());
}

static bool eligible(SP<Desktop::View::CWLSurface> surface) {
    if (!surfaceVisible(surface))
        return false;

    const auto OWNER = focusOwner(surface->resource());
    return OWNER && OWNER == focusOwner(g_pSeatManager->m_state.keyboardFocus.lock());
}

static PHLWINDOW ruleWindow(SP<Desktop::View::CWLSurface> surface) {
    const auto ROOT = surface ? Desktop::View::CWLSurface::fromResource(focusOwner(surface->resource())) : nullptr;
    return ROOT ? Desktop::View::CWindow::fromView(ROOT->view()) : nullptr;
}

void CPointerConstraintController::init() {
    m_listeners.pointerFocus  = g_pSeatManager->m_events.pointerFocusChange.listen([this] { refresh(true); });
    m_listeners.keyboardFocus = g_pSeatManager->m_events.keyboardFocusChange.listen([this] { refresh(true); });
    m_listeners.rules         = Event::bus()->m_events.window.updateRules.listen([this](auto) { refresh(); });
}

void CPointerConstraintController::release() {
    m_released = true;
    if (const auto NATIVE = m_native.lock())
        NATIVE->deactivate();
    m_ruleState.deactivate();
}

void CPointerConstraintController::refresh(bool rearm) {
    // A rule/property refresh must not undo an explicit compositor release.
    if (rearm)
        m_released = false;
    if (m_suspensions)
        return;

    const auto SURFACE  = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
    const bool ELIGIBLE = eligible(SURFACE);
    const auto NATIVE   = ELIGIBLE ? SURFACE->constraint() : nullptr;

    if (m_native != NATIVE) {
        if (const auto OLD = m_native.lock())
            OLD->deactivate();
        m_native = NATIVE;
    }

    if (NATIVE && !m_released)
        NATIVE->activate();

    const auto WINDOW = ELIGIBLE ? ruleWindow(SURFACE) : nullptr;
    const bool RULE   = WINDOW && WINDOW->m_ruleApplicator->confinePointer().valueOrDefault() && !(NATIVE && NATIVE->isActive());

    if (m_ruleSurface != SURFACE || !RULE || m_released)
        m_ruleState.deactivate();

    m_ruleSurface = RULE ? SURFACE : nullptr;
    if (RULE && !m_released)
        m_ruleState.activate();
}

Hyprutils::Utils::CScopeGuard CPointerConstraintController::suspend() {
    if (!m_suspensions) {
        m_hadConstraint  = !!activeSurface();
        m_keyboardBefore = g_pSeatManager->m_state.keyboardFocus;
        m_positionBefore = g_pInputManager->getMouseCoordsInternal();
    }
    ++m_suspensions;
    return Hyprutils::Utils::CScopeGuard([this] { resume(); });
}

void CPointerConstraintController::resume() {
    // Hit-test before rearming, not after: the old constraint must not decide
    // which surface receives focus on the destination workspace/layer.
    if (m_suspensions == 1 && m_keyboardBefore != g_pSeatManager->m_state.keyboardFocus) {
        const auto POINTER = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
        // Preserve ordinary pointer/keyboard separation, and pointer focus that
        // the focus policy has already transferred to the destination.
        if (!surfaceVisible(POINTER) || (m_hadConstraint && !eligible(POINTER)) || m_positionBefore != g_pInputManager->getMouseCoordsInternal())
            g_pInputManager->refocusPointer();
    }

    if (--m_suspensions)
        return;

    refresh(true);
    enforce();
}

SP<Desktop::View::CWLSurface> CPointerConstraintController::activeSurface() const {
    if (m_suspensions || m_released || g_pSeatManager->m_mouse.expired())
        return nullptr;

    const auto NATIVE  = m_native.lock();
    const auto SURFACE = NATIVE && NATIVE->isActive() ? NATIVE->owner() : (m_ruleState.active() ? m_ruleSurface.lock() : nullptr);
    if (!SURFACE || SURFACE->resource() != g_pSeatManager->m_state.pointerFocus || !eligible(SURFACE) || SURFACE->view()->cantLockCursor())
        return nullptr;

    if (!(NATIVE && NATIVE->isActive())) {
        const auto WINDOW = ruleWindow(SURFACE);
        if (!WINDOW || !WINDOW->m_ruleApplicator->confinePointer().valueOrDefault())
            return nullptr;
    }

    return SURFACE;
}

bool CPointerConstraintController::nativeActive() const {
    const auto NATIVE = m_native.lock();
    return NATIVE && NATIVE->isActive() && activeSurface();
}

bool CPointerConstraintController::locked() const {
    const auto NATIVE = m_native.lock();
    return nativeActive() && NATIVE->isLocked();
}

bool CPointerConstraintController::blocksAbsoluteMotion() const {
    const auto NATIVE  = m_native.lock();
    const auto SURFACE = NATIVE ? NATIVE->owner() : nullptr;
    // Suspension permits compositor hit-testing, not locked or unclamped
    // motion while the client's native constraint remains active.
    return SURFACE && SURFACE->resource() == g_pSeatManager->m_state.pointerFocus && NATIVE->isActive() && (NATIVE->isLocked() || m_suspensions);
}

bool CPointerConstraintController::preservesPointerFocus(SP<CWLSurfaceResource> target) const {
    if (!m_suspensions || !target || g_pSeatManager->m_seatGrab)
        return false;

    const auto NATIVE  = m_native.lock();
    const auto SURFACE = NATIVE ? NATIVE->owner() : nullptr;
    if (!NATIVE || !NATIVE->isActive() || !eligible(SURFACE) || SURFACE->view()->cantLockCursor())
        return false;

    // A no-op focus/warp or a pinned workspace switch must not consume a
    // one-shot constraint just by rediscovering another child of its owner.
    return SURFACE->resource() == g_pSeatManager->m_state.pointerFocus && focusOwner(target) == focusOwner(SURFACE->resource());
}

void CPointerConstraintController::enforce() {
    apply(Time::millis(Time::steadyNow()), g_pInputManager->getMouseCoordsInternal());
}

bool CPointerConstraintController::apply(uint32_t time, const Vector2D& position) {
    const auto SURFACE = activeSurface();
    if (!SURFACE)
        return false;

    const auto NATIVE = m_native.lock();
    if (NATIVE && NATIVE->isActive() && NATIVE->isLocked()) {
        Pointer::pointerController()->warpTo(NATIVE->logicPositionHint(), true);
        return true;
    }

    const auto BOX = SURFACE->getSurfaceBoxGlobal();
    if (!BOX)
        return false;

    const auto RULEWINDOW = NATIVE && NATIVE->isActive() ? nullptr : ruleWindow(SURFACE);
    const auto RULEBOX    = RULEWINDOW ? RULEWINDOW->wlSurface()->getSurfaceBoxGlobal() : std::nullopt;
    if (RULEWINDOW && !RULEBOX)
        return false;

    const auto REGION  = RULEWINDOW ? CRegion(*RULEBOX) : NATIVE->logicConstraintRegion();
    const auto CLOSEST = REGION.closestPoint(position);

    // A window rule confines to the window, not to whichever child currently
    // has pointer focus. Native constraints still take precedence on entry.
    const bool HELD_FOCUS = g_pInputManager->hasHeldButtons() && !PROTO::data->dndActive() && !g_pInputManager->m_hardInput;
    if (RULEWINDOW && !RULEWINDOW->backend().isX11() && !HELD_FOCUS) {
        Vector2D   local;
        const auto TARGET = Desktop::viewState()->hitTest().windowSurfaceAt(CLOSEST, RULEWINDOW, local);
        if (TARGET && TARGET != SURFACE->resource() && (!g_pSeatManager->m_seatGrab || g_pSeatManager->m_seatGrab->accepts(TARGET))) {
            Pointer::pointerController()->warpTo(CLOSEST, true);
            g_pSeatManager->setPointerFocus(TARGET, local);
            if (g_pSeatManager->m_state.pointerFocus == TARGET && !apply(time, CLOSEST))
                g_pSeatManager->sendPointerMotion(time, local);
            return true;
        }
    }

    const auto WINDOW      = Desktop::View::CWindow::fromView(SURFACE->view());
    const auto LOCAL       = CLOSEST - BOX->pos();
    const auto BUFFERLOCAL = WINDOW ? WINDOW->backend().surfaceLocalToBuffer(LOCAL) : LOCAL;

    Pointer::pointerController()->warpTo(CLOSEST, true);
    g_pSeatManager->sendPointerMotion(time, BUFFERLOCAL);
    PROTO::relativePointer->sendRelativeMotion(sc<uint64_t>(time) * 1000, {}, {});
    return true;
}
