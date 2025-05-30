#include "PointerConstraints.hpp"
#include "../desktop/WLSurface.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Compositor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "../helpers/Monitor.hpp"

CPointerConstraint::CPointerConstraint(SP<CZwpLockedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region_, zwpPointerConstraintsV1Lifetime lifetime_) :
    m_resourceLocked(resource_), m_locked(true), m_lifetime(lifetime_) {
    if UNLIKELY (!resource_->resource())
        return;

    resource_->setOnDestroy([this](CZwpLockedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });
    resource_->setDestroy([this](CZwpLockedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });

    m_hlSurface = CWLSurface::fromResource(surf);

    if (!m_hlSurface)
        return;

    if (region_)
        m_region.set(CWLRegionResource::fromResource(region_)->m_region);

    resource_->setSetRegion([this](CZwpLockedPointerV1* p, wl_resource* region) { onSetRegion(region); });
    resource_->setSetCursorPositionHint([this](CZwpLockedPointerV1* p, wl_fixed_t x, wl_fixed_t y) {
        static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

        if (!m_hlSurface)
            return;

        m_hintSet = true;

        float      scale   = 1.f;
        const auto PWINDOW = m_hlSurface->getWindow();
        if (PWINDOW) {
            const auto ISXWL = PWINDOW->m_isX11;
            scale            = ISXWL && *PXWLFORCESCALEZERO ? PWINDOW->m_X11SurfaceScaledBy : 1.f;
        }

        m_positionHint = {wl_fixed_to_double(x) / scale, wl_fixed_to_double(y) / scale};
        g_pInputManager->simulateMouseMovement();
    });

    sharedConstructions();
}

CPointerConstraint::CPointerConstraint(SP<CZwpConfinedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region_, zwpPointerConstraintsV1Lifetime lifetime_) :
    m_resourceConfined(resource_), m_lifetime(lifetime_) {
    if UNLIKELY (!resource_->resource())
        return;

    resource_->setOnDestroy([this](CZwpConfinedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });
    resource_->setDestroy([this](CZwpConfinedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });

    m_hlSurface = CWLSurface::fromResource(surf);

    if (!m_hlSurface)
        return;

    if (region_)
        m_region.set(CWLRegionResource::fromResource(region_)->m_region);

    resource_->setSetRegion([this](CZwpConfinedPointerV1* p, wl_resource* region) { onSetRegion(region); });

    sharedConstructions();
}

CPointerConstraint::~CPointerConstraint() {
    std::erase_if(g_pInputManager->m_constraints, [this](const auto& c) {
        const auto SHP = c.lock();
        return !SHP || SHP.get() == this;
    });

    if (m_hlSurface)
        m_hlSurface->m_constraint.reset();
}

void CPointerConstraint::sharedConstructions() {
    if (m_hlSurface) {
        m_listeners.destroySurface = m_hlSurface->m_events.destroy.registerListener([this](std::any d) {
            m_hlSurface.reset();
            if (m_active)
                deactivate();

            std::erase_if(g_pInputManager->m_constraints, [this](const auto& c) {
                const auto SHP = c.lock();
                return !SHP || SHP.get() == this;
            });
        });
    }

    m_cursorPosOnActivate = g_pInputManager->getMouseCoordsInternal();
}

bool CPointerConstraint::good() {
    return m_locked ? m_resourceLocked->resource() : m_resourceConfined->resource();
}

void CPointerConstraint::deactivate() {
    if (!m_active)
        return;

    if (m_locked)
        m_resourceLocked->sendUnlocked();
    else
        m_resourceConfined->sendUnconfined();

    m_active = false;

    if (m_lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT) {
        m_dead = true;
        // remove from inputmgr
        std::erase_if(g_pInputManager->m_constraints, [this](const auto& c) {
            const auto SHP = c.lock();
            return !SHP || SHP.get() == this;
        });
    }
}

void CPointerConstraint::activate() {
    if (m_dead || m_active)
        return;

    // TODO: hack, probably not a super duper great idea
    if (g_pSeatManager->m_state.pointerFocus != m_hlSurface->resource()) {
        const auto SURFBOX = m_hlSurface->getSurfaceBoxGlobal();
        const auto LOCAL   = SURFBOX.has_value() ? logicPositionHint() - SURFBOX->pos() : Vector2D{};
        g_pSeatManager->setPointerFocus(m_hlSurface->resource(), LOCAL);
    }

    if (m_locked)
        m_resourceLocked->sendLocked();
    else
        m_resourceConfined->sendConfined();

    m_active = true;

    g_pInputManager->simulateMouseMovement();
}

bool CPointerConstraint::isActive() {
    return m_active;
}

void CPointerConstraint::onSetRegion(wl_resource* wlRegion) {
    if (!wlRegion) {
        m_region.clear();
        return;
    }

    const auto REGION = m_region.set(CWLRegionResource::fromResource(wlRegion)->m_region);

    m_region.set(REGION);
    m_positionHint = m_region.closestPoint(m_positionHint);
    g_pInputManager->simulateMouseMovement(); // to warp the cursor if anything's amiss
}

SP<CWLSurface> CPointerConstraint::owner() {
    return m_hlSurface.lock();
}

CRegion CPointerConstraint::logicConstraintRegion() {
    CRegion    rg      = m_region;
    const auto SURFBOX = m_hlSurface->getSurfaceBoxGlobal();

    // if region wasn't set in pointer-constraints request take surface region
    if (rg.empty() && SURFBOX.has_value()) {
        rg.set(SURFBOX.value());
        return rg;
    }

    const auto CONSTRAINTPOS = SURFBOX.has_value() ? SURFBOX->pos() : Vector2D{};
    rg.translate(CONSTRAINTPOS);
    return rg;
}

bool CPointerConstraint::isLocked() {
    return m_locked;
}

Vector2D CPointerConstraint::logicPositionHint() {
    if UNLIKELY (!m_hlSurface)
        return {};

    const auto SURFBOX       = m_hlSurface->getSurfaceBoxGlobal();
    const auto CONSTRAINTPOS = SURFBOX.has_value() ? SURFBOX->pos() : Vector2D{};

    return m_hintSet ? CONSTRAINTPOS + m_positionHint : m_cursorPosOnActivate;
}

CPointerConstraintsProtocol::CPointerConstraintsProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPointerConstraintsProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpPointerConstraintsV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpPointerConstraintsV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpPointerConstraintsV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setConfinePointer([this](CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                       zwpPointerConstraintsV1Lifetime lifetime) { this->onConfinePointer(pMgr, id, surface, pointer, region, lifetime); });
    RESOURCE->setLockPointer([this](CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                    zwpPointerConstraintsV1Lifetime lifetime) { this->onLockPointer(pMgr, id, surface, pointer, region, lifetime); });
}

void CPointerConstraintsProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CPointerConstraintsProtocol::destroyPointerConstraint(CPointerConstraint* hyprlandEgg) {
    std::erase_if(m_constraints, [&](const auto& other) { return other.get() == hyprlandEgg; });
}

void CPointerConstraintsProtocol::onNewConstraint(SP<CPointerConstraint> constraint, CZwpPointerConstraintsV1* pMgr) {
    if UNLIKELY (!constraint->good()) {
        LOGM(ERR, "Couldn't create constraint??");
        pMgr->noMemory();
        m_constraints.pop_back();
        return;
    }

    if UNLIKELY (!constraint->owner()) {
        LOGM(ERR, "New constraint has no CWLSurface owner??");
        return;
    }

    const auto OWNER = constraint->owner();

    const auto DUPES = std::ranges::count_if(m_constraints, [OWNER](const auto& c) { return c->owner() == OWNER; });

    if UNLIKELY (DUPES > 1) {
        LOGM(ERR, "Constraint for surface duped");
        pMgr->error(ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED, "Surface already confined");
        m_constraints.pop_back();
        return;
    }

    OWNER->appendConstraint(constraint);

    g_pInputManager->m_constraints.emplace_back(constraint);

    if (g_pCompositor->m_lastFocus == OWNER->resource())
        constraint->activate();
}

void CPointerConstraintsProtocol::onLockPointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                                zwpPointerConstraintsV1Lifetime lifetime) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_constraints.emplace_back(
        makeShared<CPointerConstraint>(makeShared<CZwpLockedPointerV1>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surface), region, lifetime));

    onNewConstraint(RESOURCE, pMgr);
}

void CPointerConstraintsProtocol::onConfinePointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                                   zwpPointerConstraintsV1Lifetime lifetime) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_constraints.emplace_back(
        makeShared<CPointerConstraint>(makeShared<CZwpConfinedPointerV1>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surface), region, lifetime));

    onNewConstraint(RESOURCE, pMgr);
}
