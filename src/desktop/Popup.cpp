#include "Popup.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../protocols/LayerShell.hpp"
#include <ranges>

CPopup::CPopup(PHLWINDOW pOwner) : m_pWindowOwner(pOwner) {
    initAllSignals();
}

CPopup::CPopup(PHLLS pOwner) : m_pLayerOwner(pOwner) {
    initAllSignals();
}

CPopup::CPopup(wlr_xdg_popup* popup, CPopup* pOwner) : m_pParent(pOwner), m_pWLR(popup) {
    m_pWLR->base->data = this;
    m_sWLSurface.assign(popup->base->surface, this);

    m_pLayerOwner  = pOwner->m_pLayerOwner;
    m_pWindowOwner = pOwner->m_pWindowOwner;

    m_vLastSize = {m_pWLR->current.geometry.width, m_pWLR->current.geometry.height};
    unconstrain();

    initAllSignals();
}

CPopup::~CPopup() {
    m_sWLSurface.unassign();
    if (m_pWLR)
        m_pWLR->base->data = nullptr;

    hyprListener_commitPopup.removeCallback();
    hyprListener_repositionPopup.removeCallback();
    hyprListener_mapPopup.removeCallback();
    hyprListener_unmapPopup.removeCallback();
    hyprListener_newPopup.removeCallback();
    hyprListener_destroyPopup.removeCallback();
}

static void onNewPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onNewPopup((wlr_xdg_popup*)data);
}

static void onMapPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onMap();
}

static void onDestroyPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onDestroy();
}

static void onUnmapPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onUnmap();
}

static void onCommitPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onCommit();
}

static void onRepositionPopup(void* owner, void* data) {
    const auto POPUP = (CPopup*)owner;
    POPUP->onReposition();
}

void CPopup::initAllSignals() {

    if (!m_pWLR) {
        if (!m_pWindowOwner.expired())
            hyprListener_newPopup.initCallback(&m_pWindowOwner->m_uSurface.xdg->events.new_popup, ::onNewPopup, this, "CPopup Head");
        else if (!m_pLayerOwner.expired())
            listeners.newPopup = m_pLayerOwner->layerSurface->events.newPopup.registerListener([this](std::any d) { this->onNewPopup(std::any_cast<wlr_xdg_popup*>(d)); });
        else
            ASSERT(false);

        return;
    }

    hyprListener_repositionPopup.initCallback(&m_pWLR->events.reposition, ::onRepositionPopup, this, "CPopup");
    hyprListener_destroyPopup.initCallback(&m_pWLR->events.destroy, ::onDestroyPopup, this, "CPopup");
    hyprListener_mapPopup.initCallback(&m_sWLSurface.wlr()->events.map, ::onMapPopup, this, "CPopup");
    hyprListener_unmapPopup.initCallback(&m_sWLSurface.wlr()->events.unmap, ::onUnmapPopup, this, "CPopup");
    hyprListener_commitPopup.initCallback(&m_sWLSurface.wlr()->events.commit, ::onCommitPopup, this, "CPopup");
    hyprListener_newPopup.initCallback(&m_pWLR->base->events.new_popup, ::onNewPopup, this, "CPopup");
}

void CPopup::onNewPopup(wlr_xdg_popup* popup) {
    const auto POPUP = m_vChildren.emplace_back(std::make_unique<CPopup>(popup, this)).get();
    Debug::log(LOG, "New popup at wlr {:x} and hl {:x}", (uintptr_t)popup, (uintptr_t)POPUP);
}

void CPopup::onDestroy() {
    m_bInert = true;

    if (!m_pParent)
        return; // head node

    std::erase_if(m_pParent->m_vChildren, [this](const auto& other) { return other.get() == this; });
}

void CPopup::onMap() {
    m_vLastSize       = {m_pWLR->base->current.geometry.width, m_pWLR->base->current.geometry.height};
    const auto COORDS = coordsGlobal();

    CBox       box;
    wlr_surface_get_extends(m_sWLSurface.wlr(), box.pWlr());
    box.applyFromWlr().translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(&box);

    m_vLastPos = coordsRelativeToParent();

    g_pInputManager->simulateMouseMovement();

    m_pSubsurfaceHead = std::make_unique<CSubsurface>(this);

    unconstrain();
    sendScale();

    if (!m_pLayerOwner.expired() && m_pLayerOwner->layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_pLayerOwner->layer));
}

void CPopup::onUnmap() {
    m_vLastSize       = {m_pWLR->base->current.geometry.width, m_pWLR->base->current.geometry.height};
    const auto COORDS = coordsGlobal();

    CBox       box;
    wlr_surface_get_extends(m_sWLSurface.wlr(), box.pWlr());
    box.applyFromWlr().translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(&box);

    m_pSubsurfaceHead.reset();

    g_pInputManager->simulateMouseMovement();

    if (!m_pLayerOwner.expired() && m_pLayerOwner->layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_pLayerOwner->layer));
}

void CPopup::onCommit(bool ignoreSiblings) {
    if (m_pWLR->base->initial_commit) {
        wlr_xdg_surface_schedule_configure(m_pWLR->base);
        return;
    }

    if (!m_pWindowOwner.expired() && (!m_pWindowOwner->m_bIsMapped || !m_pWindowOwner->m_pWorkspace->m_bVisible)) {
        m_vLastSize = {m_pWLR->base->current.geometry.width, m_pWLR->base->current.geometry.height};

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_pWindowOwner.lock());
        return;
    }

    if (!m_pWLR->base->surface->mapped)
        return;

    const auto COORDS      = coordsGlobal();
    const auto COORDSLOCAL = coordsRelativeToParent();

    if (m_vLastSize != Vector2D{m_pWLR->base->current.geometry.width, m_pWLR->base->current.geometry.height} || m_bRequestedReposition || m_vLastPos != COORDSLOCAL) {
        CBox box = {localToGlobal(m_vLastPos), m_vLastSize};
        g_pHyprRenderer->damageBox(&box);
        m_vLastSize = {m_pWLR->base->current.geometry.width, m_pWLR->base->current.geometry.height};
        box         = {COORDS, m_vLastSize};
        g_pHyprRenderer->damageBox(&box);

        m_vLastPos = COORDSLOCAL;
    }

    if (!ignoreSiblings && m_pSubsurfaceHead)
        m_pSubsurfaceHead->recheckDamageForSubsurfaces();

    g_pHyprRenderer->damageSurface(m_sWLSurface.wlr(), COORDS.x, COORDS.y);

    m_bRequestedReposition = false;

    if (!m_pLayerOwner.expired() && m_pLayerOwner->layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_pLayerOwner->layer));
}

void CPopup::onReposition() {
    Debug::log(LOG, "Popup {:x} requests reposition", (uintptr_t)this);

    m_bRequestedReposition = true;

    m_vLastPos = coordsRelativeToParent();

    unconstrain();
}

void CPopup::unconstrain() {
    const auto COORDS   = t1ParentCoords();
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(COORDS);

    if (!PMONITOR)
        return;

    CBox box = {PMONITOR->vecPosition.x - COORDS.x, PMONITOR->vecPosition.y - COORDS.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    wlr_xdg_popup_unconstrain_from_box(m_pWLR, box.pWlr());
}

Vector2D CPopup::coordsRelativeToParent() {
    Vector2D offset;

    CPopup*  current = this;

    offset -= {m_pWLR->base->current.geometry.x, m_pWLR->base->current.geometry.y};

    while (current->m_pParent) {

        offset += {current->m_sWLSurface.wlr()->current.dx, current->m_sWLSurface.wlr()->current.dy};
        offset += {current->m_pWLR->current.geometry.x, current->m_pWLR->current.geometry.y};

        current = current->m_pParent;
    }

    return offset;
}

Vector2D CPopup::coordsGlobal() {
    return localToGlobal(coordsRelativeToParent());
}

Vector2D CPopup::localToGlobal(const Vector2D& rel) {
    return t1ParentCoords() + rel;
}

Vector2D CPopup::t1ParentCoords() {
    if (!m_pWindowOwner.expired())
        return m_pWindowOwner->m_vRealPosition.value();
    if (!m_pLayerOwner.expired())
        return m_pLayerOwner->realPosition.value();

    ASSERT(false);
    return {};
}

void CPopup::recheckTree() {
    CPopup* curr = this;
    while (curr->m_pParent) {
        curr = curr->m_pParent;
    }

    curr->recheckChildrenRecursive();
}

void CPopup::recheckChildrenRecursive() {
    for (auto& c : m_vChildren) {
        c->onCommit(true);
        c->recheckChildrenRecursive();
    }
}

Vector2D CPopup::size() {
    return m_vLastSize;
}

void CPopup::sendScale() {
    if (!m_pWindowOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_sWLSurface.wlr(), m_pWindowOwner->m_pWLSurface.m_fLastScale);
    else if (!m_pLayerOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_sWLSurface.wlr(), m_pLayerOwner->surface.m_fLastScale);
    else
        UNREACHABLE();
}

bool CPopup::visible() {
    if (!m_pWindowOwner.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_pWindowOwner.lock());
    if (!m_pLayerOwner.expired())
        return true;
    if (m_pParent)
        return m_pParent->visible();

    return false;
}

void CPopup::bfHelper(std::vector<CPopup*> nodes, std::function<void(CPopup*, void*)> fn, void* data) {
    for (auto& n : nodes) {
        fn(n, data);
    }

    std::vector<CPopup*> nodes2;

    for (auto& n : nodes) {
        for (auto& c : n->m_vChildren) {
            nodes2.push_back(c.get());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);
}

void CPopup::breadthfirst(std::function<void(CPopup*, void*)> fn, void* data) {
    std::vector<CPopup*> popups;
    popups.push_back(this);
    bfHelper(popups, fn, data);
}

CPopup* CPopup::at(const Vector2D& globalCoords, bool allowsInput) {
    std::vector<CPopup*> popups;
    breadthfirst([](CPopup* popup, void* data) { ((std::vector<CPopup*>*)data)->push_back(popup); }, &popups);

    for (auto& p : popups | std::views::reverse) {
        if (!p->m_pWLR)
            continue;

        if (!allowsInput) {
            const auto BOX = CBox{p->coordsGlobal(), p->size()};
            if (BOX.containsPoint(globalCoords))
                return p;
        } else {
            const auto REGION = CRegion{&p->m_sWLSurface.wlr()->current.input}.translate(p->coordsGlobal());
            if (REGION.containsPoint(globalCoords))
                return p;
        }
    }

    return nullptr;
}
