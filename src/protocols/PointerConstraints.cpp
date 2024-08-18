#include "PointerConstraints.hpp"
#include "../desktop/WLSurface.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Compositor.hpp"

CPointerConstraint::CPointerConstraint(SP<CZwpLockedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region_, zwpPointerConstraintsV1Lifetime lifetime_) :
    resourceL(resource_), locked(true), lifetime(lifetime_) {
    if (!resource_->resource())
        return;

    resource_->setOnDestroy([this](CZwpLockedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });
    resource_->setDestroy([this](CZwpLockedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });

    pHLSurface = CWLSurface::fromResource(surf);

    if (!pHLSurface)
        return;

    if (region_)
        region.set(CWLRegionResource::fromResource(region_)->region);

    resource_->setSetRegion([this](CZwpLockedPointerV1* p, wl_resource* region) { onSetRegion(region); });
    resource_->setSetCursorPositionHint([this](CZwpLockedPointerV1* p, wl_fixed_t x, wl_fixed_t y) {
        static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

        if (!pHLSurface)
            return;

        hintSet = true;

        float      scale   = 1.f;
        const auto PWINDOW = pHLSurface->getWindow();
        if (PWINDOW) {
            const auto ISXWL = PWINDOW->m_bIsX11;
            scale            = ISXWL && *PXWLFORCESCALEZERO ? PWINDOW->m_fX11SurfaceScaledBy : 1.f;
        }

        positionHint = {wl_fixed_to_double(x) / scale, wl_fixed_to_double(y) / scale};
        g_pInputManager->simulateMouseMovement();
    });

    sharedConstructions();
}

CPointerConstraint::CPointerConstraint(SP<CZwpConfinedPointerV1> resource_, SP<CWLSurfaceResource> surf, wl_resource* region_, zwpPointerConstraintsV1Lifetime lifetime_) :
    resourceC(resource_), locked(false), lifetime(lifetime_) {
    if (!resource_->resource())
        return;

    resource_->setOnDestroy([this](CZwpConfinedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });
    resource_->setDestroy([this](CZwpConfinedPointerV1* p) { PROTO::constraints->destroyPointerConstraint(this); });

    pHLSurface = CWLSurface::fromResource(surf);

    if (!pHLSurface)
        return;

    if (region_)
        region.set(CWLRegionResource::fromResource(region_)->region);

    resource_->setSetRegion([this](CZwpConfinedPointerV1* p, wl_resource* region) { onSetRegion(region); });

    sharedConstructions();
}

CPointerConstraint::~CPointerConstraint() {
    std::erase_if(g_pInputManager->m_vConstraints, [this](const auto& c) {
        const auto SHP = c.lock();
        return !SHP || SHP.get() == this;
    });

    if (pHLSurface)
        pHLSurface->m_pConstraint.reset();
}

void CPointerConstraint::sharedConstructions() {
    if (pHLSurface) {
        listeners.destroySurface = pHLSurface->events.destroy.registerListener([this](std::any d) {
            pHLSurface.reset();
            if (active)
                deactivate();

            std::erase_if(g_pInputManager->m_vConstraints, [this](const auto& c) {
                const auto SHP = c.lock();
                return !SHP || SHP.get() == this;
            });
        });
    }

    cursorPosOnActivate = g_pInputManager->getMouseCoordsInternal();

    if (g_pCompositor->m_pLastFocus == pHLSurface->resource())
        activate();
}

bool CPointerConstraint::good() {
    return locked ? resourceL->resource() : resourceC->resource();
}

void CPointerConstraint::deactivate() {
    if (!active)
        return;

    if (locked)
        resourceL->sendUnlocked();
    else
        resourceC->sendUnconfined();

    active = false;

    if (lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT) {
        dead = true;
        // remove from inputmgr
        std::erase_if(g_pInputManager->m_vConstraints, [this](const auto& c) {
            const auto SHP = c.lock();
            return !SHP || SHP.get() == this;
        });
    }
}

void CPointerConstraint::activate() {
    if (dead || active)
        return;

    // TODO: hack, probably not a super duper great idea
    if (g_pSeatManager->state.pointerFocus != pHLSurface->resource()) {
        const auto SURFBOX = pHLSurface->getSurfaceBoxGlobal();
        const auto LOCAL   = SURFBOX.has_value() ? logicPositionHint() - SURFBOX->pos() : Vector2D{};
        g_pSeatManager->setPointerFocus(pHLSurface->resource(), LOCAL);
    }

    if (locked)
        resourceL->sendLocked();
    else
        resourceC->sendConfined();

    active = true;

    g_pInputManager->simulateMouseMovement();
}

bool CPointerConstraint::isActive() {
    return active;
}

void CPointerConstraint::onSetRegion(wl_resource* wlRegion) {
    if (!wlRegion) {
        region.clear();
        return;
    }

    const auto REGION = region.set(CWLRegionResource::fromResource(wlRegion)->region);

    region.set(REGION);
    positionHint = region.closestPoint(positionHint);
    g_pInputManager->simulateMouseMovement(); // to warp the cursor if anything's amiss
}

SP<CWLSurface> CPointerConstraint::owner() {
    return pHLSurface.lock();
}

CRegion CPointerConstraint::logicConstraintRegion() {
    CRegion    rg      = region;
    const auto SURFBOX = pHLSurface->getSurfaceBoxGlobal();

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
    return locked;
}

Vector2D CPointerConstraint::logicPositionHint() {
    if (!pHLSurface)
        return {};

    const auto SURFBOX       = pHLSurface->getSurfaceBoxGlobal();
    const auto CONSTRAINTPOS = SURFBOX.has_value() ? SURFBOX->pos() : Vector2D{};

    return hintSet ? CONSTRAINTPOS + positionHint : (locked ? CONSTRAINTPOS + SURFBOX->size() / 2.f : cursorPosOnActivate);
}

CPointerConstraintsProtocol::CPointerConstraintsProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CPointerConstraintsProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpPointerConstraintsV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpPointerConstraintsV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpPointerConstraintsV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setConfinePointer([this](CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                       zwpPointerConstraintsV1Lifetime lifetime) { this->onConfinePointer(pMgr, id, surface, pointer, region, lifetime); });
    RESOURCE->setLockPointer([this](CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                    zwpPointerConstraintsV1Lifetime lifetime) { this->onLockPointer(pMgr, id, surface, pointer, region, lifetime); });
}

void CPointerConstraintsProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CPointerConstraintsProtocol::destroyPointerConstraint(CPointerConstraint* hyprlandEgg) {
    std::erase_if(m_vConstraints, [&](const auto& other) { return other.get() == hyprlandEgg; });
}

void CPointerConstraintsProtocol::onNewConstraint(SP<CPointerConstraint> constraint, CZwpPointerConstraintsV1* pMgr) {
    if (!constraint->good()) {
        LOGM(ERR, "Couldn't create constraint??");
        pMgr->noMemory();
        m_vConstraints.pop_back();
        return;
    }

    if (!constraint->owner()) {
        LOGM(ERR, "New constraint has no CWLSurface owner??");
        return;
    }

    const auto OWNER = constraint->owner();

    const auto DUPES = std::count_if(m_vConstraints.begin(), m_vConstraints.end(), [OWNER](const auto& c) { return c->owner() == OWNER; });

    if (DUPES > 1) {
        LOGM(ERR, "Constraint for surface duped");
        pMgr->error(ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED, "Surface already confined");
        m_vConstraints.pop_back();
        return;
    }

    OWNER->appendConstraint(constraint);

    g_pInputManager->m_vConstraints.push_back(constraint);
}

void CPointerConstraintsProtocol::onLockPointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                                zwpPointerConstraintsV1Lifetime lifetime) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vConstraints.emplace_back(
        makeShared<CPointerConstraint>(makeShared<CZwpLockedPointerV1>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surface), region, lifetime));

    onNewConstraint(RESOURCE, pMgr);
}

void CPointerConstraintsProtocol::onConfinePointer(CZwpPointerConstraintsV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* pointer, wl_resource* region,
                                                   zwpPointerConstraintsV1Lifetime lifetime) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vConstraints.emplace_back(
        makeShared<CPointerConstraint>(makeShared<CZwpConfinedPointerV1>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surface), region, lifetime));

    onNewConstraint(RESOURCE, pMgr);
}
