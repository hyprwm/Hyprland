#include "Subsurface.hpp"
#include "../events/Events.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Subcompositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"

UP<CSubsurface> CSubsurface::create(PHLWINDOW pOwner) {
    auto subsurface            = UP<CSubsurface>(new CSubsurface());
    subsurface->m_windowParent = pOwner;
    subsurface->m_self         = subsurface;

    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pOwner->m_wlSurface->resource());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(WP<CPopup> pOwner) {
    auto subsurface           = UP<CSubsurface>(new CSubsurface());
    subsurface->m_popupParent = pOwner;
    subsurface->m_self        = subsurface;
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pOwner->m_wlSurface->resource());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(SP<CWLSubsurfaceResource> pSubsurface, PHLWINDOW pOwner) {
    auto subsurface            = UP<CSubsurface>(new CSubsurface());
    subsurface->m_windowParent = pOwner;
    subsurface->m_subsurface   = pSubsurface;
    subsurface->m_self         = subsurface;
    subsurface->m_wlSurface    = CWLSurface::create();
    subsurface->m_wlSurface->assign(pSubsurface->surface.lock(), subsurface.get());
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pSubsurface->surface.lock());
    return subsurface;
}

UP<CSubsurface> CSubsurface::create(SP<CWLSubsurfaceResource> pSubsurface, WP<CPopup> pOwner) {
    auto subsurface           = UP<CSubsurface>(new CSubsurface());
    subsurface->m_popupParent = pOwner;
    subsurface->m_subsurface  = pSubsurface;
    subsurface->m_self        = subsurface;
    subsurface->m_wlSurface   = CWLSurface::create();
    subsurface->m_wlSurface->assign(pSubsurface->surface.lock(), subsurface.get());
    subsurface->initSignals();
    subsurface->initExistingSubsurfaces(pSubsurface->surface.lock());
    return subsurface;
}

void CSubsurface::initSignals() {
    if (m_subsurface) {
        m_listeners.commitSubsurface  = m_subsurface->surface->events.commit.registerListener([this](std::any d) { onCommit(); });
        m_listeners.destroySubsurface = m_subsurface->events.destroy.registerListener([this](std::any d) { onDestroy(); });
        m_listeners.mapSubsurface     = m_subsurface->surface->events.map.registerListener([this](std::any d) { onMap(); });
        m_listeners.unmapSubsurface   = m_subsurface->surface->events.unmap.registerListener([this](std::any d) { onUnmap(); });
        m_listeners.newSubsurface =
            m_subsurface->surface->events.newSubsurface.registerListener([this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
    } else {
        if (m_windowParent)
            m_listeners.newSubsurface = m_windowParent->m_wlSurface->resource()->events.newSubsurface.registerListener(
                [this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
        else if (m_popupParent)
            m_listeners.newSubsurface =
                m_popupParent->m_wlSurface->resource()->events.newSubsurface.registerListener([this](std::any d) { onNewSubsurface(std::any_cast<SP<CWLSubsurfaceResource>>(d)); });
        else
            ASSERT(false);
    }
}

void CSubsurface::checkSiblingDamage() {
    if (!m_parent)
        return; // ??????????

    const double SCALE = m_windowParent.lock() && m_windowParent->m_isX11 ? 1.0 / m_windowParent->m_X11SurfaceScaledBy : 1.0;

    for (auto const& n : m_parent->m_children) {
        if (n.get() == this)
            continue;

        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_wlSurface->resource(), COORDS.x, COORDS.y, SCALE);
    }
}

void CSubsurface::recheckDamageForSubsurfaces() {
    for (auto const& n : m_children) {
        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_wlSurface->resource(), COORDS.x, COORDS.y);
    }
}

void CSubsurface::onCommit() {
    // no damaging if it's not visible
    if (!m_windowParent.expired() && (!m_windowParent->m_isMapped || !m_windowParent->m_workspace->m_visible)) {
        m_lastSize = m_wlSurface->resource()->current.size;

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_windowParent.lock());
        return;
    }

    const auto COORDS = coordsGlobal();

    g_pHyprRenderer->damageSurface(m_wlSurface->resource(), COORDS.x, COORDS.y);

    if (m_popupParent && !m_popupParent->inert() && m_popupParent->m_wlSurface)
        m_popupParent->recheckTree();
    if (!m_windowParent.expired()) // I hate you firefox why are you doing this
        m_windowParent->m_popupHead->recheckTree();

    // I do not think this is correct, but it solves a lot of issues with some apps (e.g. firefox)
    checkSiblingDamage();

    if (m_lastSize != m_wlSurface->resource()->current.size || m_lastPosition != m_subsurface->position) {
        damageLastArea();
        m_lastSize     = m_wlSurface->resource()->current.size;
        m_lastPosition = m_subsurface->position;
    }
}

void CSubsurface::onDestroy() {
    // destroy children
    m_children.clear();

    m_inert = true;

    if (!m_subsurface)
        return; // dummy node, nothing to do, it's the parent dying

    // kill ourselves
    std::erase_if(m_parent->m_children, [this](const auto& other) { return other.get() == this; });
}

void CSubsurface::onNewSubsurface(SP<CWLSubsurfaceResource> pSubsurface) {
    WP<CSubsurface> PSUBSURFACE;

    if (!m_windowParent.expired())
        PSUBSURFACE = m_children.emplace_back(CSubsurface::create(pSubsurface, m_windowParent.lock()));
    else if (m_popupParent)
        PSUBSURFACE = m_children.emplace_back(CSubsurface::create(pSubsurface, m_popupParent));

    PSUBSURFACE->m_self = PSUBSURFACE;

    ASSERT(PSUBSURFACE);

    PSUBSURFACE->m_parent = m_self;
}

void CSubsurface::onMap() {
    m_lastSize     = m_wlSurface->resource()->current.size;
    m_lastPosition = m_subsurface->position;

    const auto COORDS = coordsGlobal();
    CBox       box{COORDS, m_lastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(box);

    if (!m_windowParent.expired())
        m_windowParent->updateSurfaceScaleTransformDetails();
}

void CSubsurface::onUnmap() {
    damageLastArea();

    if (m_wlSurface->resource() == g_pCompositor->m_lastFocus)
        g_pInputManager->releaseAllMouseButtons();

    g_pInputManager->simulateMouseMovement();

    // TODO: should this remove children? Currently it won't, only on .destroy
}

void CSubsurface::damageLastArea() {
    const auto COORDS = coordsGlobal() + m_lastPosition - m_subsurface->position;
    CBox       box{COORDS, m_lastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(box);
}

Vector2D CSubsurface::coordsRelativeToParent() {
    if (!m_subsurface)
        return {};
    return m_subsurface->posRelativeToParent();
}

Vector2D CSubsurface::coordsGlobal() {
    Vector2D coords = coordsRelativeToParent();

    if (!m_windowParent.expired())
        coords += m_windowParent->m_realPosition->value();
    else if (m_popupParent)
        coords += m_popupParent->coordsGlobal();

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
    return m_wlSurface->resource()->current.size;
}

bool CSubsurface::visible() {
    if (!m_windowParent.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_windowParent.lock());
    if (m_popupParent)
        return m_popupParent->visible();
    if (m_parent)
        return m_parent->visible();

    return false;
}
