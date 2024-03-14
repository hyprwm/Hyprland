#include "Subsurface.hpp"
#include "../events/Events.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"

static void onNewSubsurface(void* owner, void* data);

CSubsurface::CSubsurface(CWindow* pOwner) : m_pWindowParent(pOwner) {
    initSignals();

    wlr_subsurface* wlrSubsurface;
    wl_list_for_each(wlrSubsurface, &pOwner->m_pWLSurface.wlr()->current.subsurfaces_below, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
    wl_list_for_each(wlrSubsurface, &pOwner->m_pWLSurface.wlr()->current.subsurfaces_above, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
}

CSubsurface::CSubsurface(CPopup* pOwner) : m_pPopupParent(pOwner) {
    initSignals();

    wlr_subsurface* wlrSubsurface;
    wl_list_for_each(wlrSubsurface, &pOwner->m_sWLSurface.wlr()->current.subsurfaces_below, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
    wl_list_for_each(wlrSubsurface, &pOwner->m_sWLSurface.wlr()->current.subsurfaces_above, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
}

CSubsurface::CSubsurface(wlr_subsurface* pSubsurface, CWindow* pOwner) : m_pSubsurface(pSubsurface), m_pWindowParent(pOwner) {
    m_sWLSurface.assign(pSubsurface->surface, this);
    initSignals();
    initExistingSubsurfaces();
}

CSubsurface::CSubsurface(wlr_subsurface* pSubsurface, CPopup* pOwner) : m_pSubsurface(pSubsurface), m_pPopupParent(pOwner) {
    m_sWLSurface.assign(pSubsurface->surface, this);
    initSignals();
    initExistingSubsurfaces();
}

CSubsurface::~CSubsurface() {
    hyprListener_newSubsurface.removeCallback();

    if (!m_pSubsurface)
        return;

    hyprListener_commitSubsurface.removeCallback();
    hyprListener_destroySubsurface.removeCallback();
}

static void onNewSubsurface(void* owner, void* data) {
    const auto PSUBSURFACE = (CSubsurface*)owner;
    PSUBSURFACE->onNewSubsurface((wlr_subsurface*)data);
}

static void onDestroySubsurface(void* owner, void* data) {
    const auto PSUBSURFACE = (CSubsurface*)owner;
    PSUBSURFACE->onDestroy();
}

static void onCommitSubsurface(void* owner, void* data) {
    const auto PSUBSURFACE = (CSubsurface*)owner;
    PSUBSURFACE->onCommit();
}

static void onMapSubsurface(void* owner, void* data) {
    const auto PSUBSURFACE = (CSubsurface*)owner;
    PSUBSURFACE->onMap();
}

static void onUnmapSubsurface(void* owner, void* data) {
    const auto PSUBSURFACE = (CSubsurface*)owner;
    PSUBSURFACE->onUnmap();
}

void CSubsurface::initSignals() {
    if (m_pSubsurface) {
        hyprListener_commitSubsurface.initCallback(&m_pSubsurface->surface->events.commit, &onCommitSubsurface, this, "CSubsurface");
        hyprListener_destroySubsurface.initCallback(&m_pSubsurface->events.destroy, &onDestroySubsurface, this, "CSubsurface");
        hyprListener_newSubsurface.initCallback(&m_pSubsurface->surface->events.new_subsurface, &::onNewSubsurface, this, "CSubsurface");
        hyprListener_mapSubsurface.initCallback(&m_pSubsurface->surface->events.map, &onMapSubsurface, this, "CSubsurface");
        hyprListener_unmapSubsurface.initCallback(&m_pSubsurface->surface->events.unmap, &onUnmapSubsurface, this, "CSubsurface");
    } else {
        if (m_pWindowParent)
            hyprListener_newSubsurface.initCallback(&m_pWindowParent->m_pWLSurface.wlr()->events.new_subsurface, &::onNewSubsurface, this, "CSubsurface Head");
        else if (m_pPopupParent)
            hyprListener_newSubsurface.initCallback(&m_pPopupParent->m_sWLSurface.wlr()->events.new_subsurface, &::onNewSubsurface, this, "CSubsurface Head");
        else
            RASSERT(false, "CSubsurface::initSignals empty subsurface");
    }
}

void CSubsurface::checkSiblingDamage() {
    if (!m_pParent)
        return; // ??????????

    const double SCALE = m_pWindowParent && m_pWindowParent->m_bIsX11 ? 1.0 / m_pWindowParent->m_fX11SurfaceScaledBy : 1.0;

    for (auto& n : m_pParent->m_vChildren) {
        if (n.get() == this)
            continue;

        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_sWLSurface.wlr(), COORDS.x, COORDS.y, SCALE);
    }
}

void CSubsurface::recheckDamageForSubsurfaces() {
    for (auto& n : m_vChildren) {
        const auto COORDS = n->coordsGlobal();
        g_pHyprRenderer->damageSurface(n->m_sWLSurface.wlr(), COORDS.x, COORDS.y);
    }
}

void CSubsurface::onCommit() {
    // no damaging if it's not visible
    if (m_pWindowParent && !g_pHyprRenderer->shouldRenderWindow(m_pWindowParent)) {
        m_vLastSize = Vector2D{m_sWLSurface.wlr()->current.width, m_sWLSurface.wlr()->current.height};

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_pWindowParent);
        return;
    }

    const auto COORDS = coordsGlobal();

    g_pHyprRenderer->damageSurface(m_sWLSurface.wlr(), COORDS.x, COORDS.y);

    if (m_pPopupParent)
        m_pPopupParent->recheckTree();
    if (m_pWindowParent) // I hate you firefox why are you doing this
        m_pWindowParent->m_pPopupHead->recheckTree();

    // I do not think this is correct, but it solves a lot of issues with some apps (e.g. firefox)
    checkSiblingDamage();

    if (m_vLastSize != Vector2D{m_sWLSurface.wlr()->current.width, m_sWLSurface.wlr()->current.height}) {
        CBox box{COORDS, m_vLastSize};
        g_pHyprRenderer->damageBox(&box);
        m_vLastSize = Vector2D{m_sWLSurface.wlr()->current.width, m_sWLSurface.wlr()->current.height};
        box         = {COORDS, m_vLastSize};
        g_pHyprRenderer->damageBox(&box);
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

void CSubsurface::onNewSubsurface(wlr_subsurface* pSubsurface) {
    CSubsurface* PSUBSURFACE = nullptr;

    if (m_pWindowParent)
        PSUBSURFACE = m_vChildren.emplace_back(std::make_unique<CSubsurface>(pSubsurface, m_pWindowParent)).get();
    else if (m_pPopupParent)
        PSUBSURFACE = m_vChildren.emplace_back(std::make_unique<CSubsurface>(pSubsurface, m_pPopupParent)).get();
    PSUBSURFACE->m_pParent = this;

    ASSERT(PSUBSURFACE);
}

void CSubsurface::onMap() {
    m_vLastSize = {m_sWLSurface.wlr()->current.width, m_sWLSurface.wlr()->current.height};

    const auto COORDS = coordsGlobal();
    CBox       box{COORDS, m_vLastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(&box);

    if (m_pWindowParent)
        m_pWindowParent->updateSurfaceScaleTransformDetails();
}

void CSubsurface::onUnmap() {
    const auto COORDS = coordsGlobal();
    CBox       box{COORDS, m_vLastSize};
    box.expand(4);
    g_pHyprRenderer->damageBox(&box);

    if (m_sWLSurface.wlr() == g_pCompositor->m_pLastFocus)
        g_pInputManager->releaseAllMouseButtons();

    g_pInputManager->simulateMouseMovement();

    // TODO: should this remove children? Currently it won't, only on .destroy
}

Vector2D CSubsurface::coordsRelativeToParent() {
    Vector2D     offset;

    CSubsurface* current = this;

    while (current->m_pParent) {

        offset += {current->m_sWLSurface.wlr()->current.dx, current->m_sWLSurface.wlr()->current.dy};
        offset += {current->m_pSubsurface->current.x, current->m_pSubsurface->current.y};

        current = current->m_pParent;
    }

    return offset;
}

Vector2D CSubsurface::coordsGlobal() {
    Vector2D coords = coordsRelativeToParent();

    if (m_pWindowParent)
        coords += m_pWindowParent->m_vRealPosition.value();
    else if (m_pPopupParent)
        coords += m_pPopupParent->coordsGlobal();

    return coords;
}

void CSubsurface::initExistingSubsurfaces() {
    if (m_pWindowParent)
        return;

    wlr_subsurface* wlrSubsurface;
    wl_list_for_each(wlrSubsurface, &m_sWLSurface.wlr()->current.subsurfaces_below, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
    wl_list_for_each(wlrSubsurface, &m_sWLSurface.wlr()->current.subsurfaces_above, current.link) {
        ::onNewSubsurface(this, wlrSubsurface);
    }
}

Vector2D CSubsurface::size() {
    return {m_sWLSurface.wlr()->current.width, m_sWLSurface.wlr()->current.height};
}
