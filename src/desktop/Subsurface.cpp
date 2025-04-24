#include "Subsurface.hpp"
#include "../events/Events.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Subcompositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"

UP<CSubsurface> CSubsurface::create(PHLWINDOW pOwner) {
    auto subsurface             = UP<CSubsurface>(new CSubsurface());
    subsurface->m_pWindowParent = pOwner;
    subsurface->m_pSelf         = subsurface;

    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pOwner->m_pWLSurface->resource());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(WP<CPopup> pOwner) {
    auto subsurface            = UP<CSubsurface>(new CSubsurface());
    subsurface->m_pPopupParent = pOwner;
    subsurface->m_pSelf        = subsurface;
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pOwner->m_wlSurface->resource());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner) {
    auto subsurface             = UP<CSubsurface>(new CSubsurface());
    subsurface->m_pWindowParent = pOwner;
    subsurface->m_pSubsurface   = pSubsurface;
    subsurface->m_pSelf         = subsurface;
    subsurface->m_pWLSurface    = CWLSurface::create();
    subsurface->m_pWLSurface->assign(pSubsurface->surface.lock(), subsurface.get());
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pSubsurface->surface.lock());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(SP<CWLSubsurfaceResource> pSubsurface, WP<CPopup> pOwner) {
    auto subsurface            = UP<CSubsurface>(new CSubsurface());
    subsurface->m_pPopupParent = pOwner;
    subsurface->m_pSubsurface  = pSubsurface;
    subsurface->m_pSelf        = subsurface;
    subsurface->m_pWLSurface   = CWLSurface::create();
    subsurface->m_pWLSurface->assign(pSubsurface->surface.lock(), subsurface.get());
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pSubsurface->surface.lock());
    return subsurface;
}

void CSubsurface::initSignals() {
    if (m_pSubsurface) {
        listeners.commitSubsurface  = m_pSubsurface->surface->events.commit.registerListener([this](std::any d) { onCommit(); });
        listeners.destroySubsurface = m_pSubsurface->events.destroy.registerListener([this](std::any d) { onDestroy(); });
        listeners.mapSubsurface     = m_pSubsurface->surface->events.map.registerListener([this](std::any d) { onMap(); });
        listeners.unmapSubsurface   = m_pSubsurface->surface->events.unmap.registerListener([this](std::any d) { onUnmap(); });
        listeners.newSubsurface =
            m_pSubsurface->surface->events.newSubsurface.registerListener([this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
    } else {
        if (m_pWindowParent)
            listeners.newSubsurface = m_pWindowParent->m_pWLSurface->resource()->events.newSubsurface.registerListener(
                [this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
        else if (m_pPopupParent)
            listeners.newSubsurface = m_pPopupParent->m_wlSurface->resource()->events.newSubsurface.registerListener(
                [this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
        else
            ASSERT(false);
    }
}

void CSubsurface::checkSiblingDamage() {
    if (!m_pParent)
        return; // ??????????

    const double SCALE = m_pWindowParent.lock() && m_pWindowParent->m_bIsX11 ? 1.0 / m_pWindowParent->m_fX11SurfaceScaledBy : 1.0;

    for (auto const& n : m_pParent->m_vChildren) {
        if (n.get() == this)
            continue;

        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_pWLSurface->resource(), COORDS.x, COORDS.y, SCALE);
    }
}

void CSubsurface::recheckDamageForSubsurfaces() {
    for (auto const& n : m_vChildren) {
        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_pWLSurface->resource(), COORDS.x, COORDS.y);
    }
}

void CSubsurface::onCommit() {
    // no damaging if it's not visible
    if (!m_pWindowParent.expired() && (!m_pWindowParent->m_bIsMapped || !m_pWindowParent->m_pWorkspace->m_bVisible)) {
        m_vLastSize = m_pWLSurface->resource()->current.size;

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_pWindowParent.lock());
        return;
    }

    const auto COORDS = coordsGlobal();

    g_pHyprRenderer->damageSurface(m_pWLSurface->resource(), COORDS.x, COORDS.y);

    if (m_pPopupParent && !m_pPopupParent->inert() && m_pPopupParent->m_wlSurface)
        m_pPopupParent->recheckTree();
    if (!m_pWindowParent.expired()) // I hate you firefox why are you doing this
        m_pWindowParent->m_pPopupHead->recheckTree();

    // I do not think this is correct, but it solves a lot of issues with some apps (e.g. firefox)
    checkSiblingDamage();

    if (m_vLastSize != m_pWLSurface->resource()->current.size || m_vLastPosition != m_pSubsurface->position) {
        damageLastArea();
        m_vLastSize     = m_pWLSurface->resource()->current.size;
        m_vLastPosition = m_pSubsurface->position;
    }
}

void CSubsurface::onDestroy() {
    // destroy children
    m_vChildren.clear();

    m_bInert = true;

    if (!m_pSubsurface)
        return; // dummy node, nothing to do, it's the parent dying

    // kill ourselves
    std::erase_if(m_pParent->m_vChildren, [this](const auto& other) { return other.get() == this; });
}

void CSubsurface::onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface) {
    WP<CSubsurface> PSUBSURFACE;

    if (!m_pWindowParent.expired())
        PSUBSURFACE = m_vChildren.emplace_back(CSubsurface::create(pSubsurface, m_pWindowParent.lock()));
    else if (m_pPopupParent)
        PSUBSURFACE = m_vChildren.emplace_back(CSubsurface::create(pSubsurface, m_pPopupParent));

    PSUBSURFACE->m_pSelf = PSUBSURFACE;

    ASSERT(PSUBSURFACE);

    PSUBSURFACE->m_pParent = m_pSelf;
}

void CSubsurface::onMap() {
    m_vLastSize     = m_pWLSurface->resource()->current.size;
    m_vLastPosition = m_pSubsurface->position;

    const auto COORDS = coordsGlobal();
    CBox       box{COORDS, m_vLastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(box);

    if (!m_pWindowParent.expired())
        m_pWindowParent->updateSurfaceScaleTransformDetails();
}

void CSubsurface::onUnmap() {
    damageLastArea();

    if (m_pWLSurface->resource() == g_pCompositor->m_lastFocus)
        g_pInputManager->releaseAllMouseButtons();

    g_pInputManager->simulateMouseMovement();

    // TODO: should this remove children? Currently it won't, only on .destroy
}

void CSubsurface::damageLastArea() {
    const auto COORDS = coordsGlobal() + m_vLastPosition - m_pSubsurface->position;
    CBox       box{COORDS, m_vLastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(box);
}

Vector2D CSubsurface::coordsRelativeToParent() {
    if (!m_pSubsurface)
        return {};
    return m_pSubsurface->posRelativeToParent();
}

Vector2D CSubsurface::coordsGlobal() {
    Vector2D coords = coordsRelativeToParent();

    if (!m_pWindowParent.expired())
        coords += m_pWindowParent->m_vRealPosition->value();
    else if (m_pPopupParent)
        coords += m_pPopupParent->coordsGlobal();

    return coords;
}

void CSubsurface::initExistingSubsurfaces(SP<CWLSurfaceResource> pSurface) {
    for (auto const& s : pSurface->subsurfaces) {
        if (!s || s->surface->hlSurface /* already assigned */)
            continue;
        onNewSubsurface(s.lock());
    }
}

Vector2D CSubsurface::size() {
    return m_pWLSurface->resource()->current.size;
}

bool CSubsurface::visible() {
    if (!m_pWindowParent.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_pWindowParent.lock());
    if (m_pPopupParent)
        return m_pPopupParent->visible();
    if (m_pParent)
        return m_pParent->visible();

    return false;
}
