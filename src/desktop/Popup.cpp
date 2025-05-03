#include "Popup.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "../render/OpenGL.hpp"
#include <ranges>

UP<CPopup> CPopup::create(PHLWINDOW pOwner) {
    auto popup           = UP<CPopup>(new CPopup());
    popup->m_windowOwner = pOwner;
    popup->m_self        = popup;
    popup->initAllSignals();
    return popup;
}

UP<CPopup> CPopup::create(PHLLS pOwner) {
    auto popup          = UP<CPopup>(new CPopup());
    popup->m_layerOwner = pOwner;
    popup->m_self       = popup;
    popup->initAllSignals();
    return popup;
}

UP<CPopup> CPopup::create(SP<CXDGPopupResource> resource, WP<CPopup> pOwner) {
    auto popup           = UP<CPopup>(new CPopup());
    popup->m_resource    = resource;
    popup->m_windowOwner = pOwner->m_windowOwner;
    popup->m_layerOwner  = pOwner->m_layerOwner;
    popup->m_parent      = pOwner;
    popup->m_self        = popup;
    popup->m_wlSurface   = CWLSurface::create();
    popup->m_wlSurface->assign(resource->surface->surface.lock(), popup.get());

    popup->m_lastSize = resource->surface->current.geometry.size();
    popup->reposition();

    popup->initAllSignals();
    return popup;
}

CPopup::~CPopup() {
    if (m_wlSurface)
        m_wlSurface->unassign();
}

void CPopup::initAllSignals() {

    if (!m_resource) {
        if (!m_windowOwner.expired())
            m_listeners.newPopup = m_windowOwner->m_xdgSurface->events.newPopup.registerListener([this](std::any d) { this->onNewPopup(std::any_cast<SP<CXDGPopupResource>>(d)); });
        else if (!m_layerOwner.expired())
            m_listeners.newPopup =
                m_layerOwner->m_layerSurface->events.newPopup.registerListener([this](std::any d) { this->onNewPopup(std::any_cast<SP<CXDGPopupResource>>(d)); });
        else
            ASSERT(false);

        return;
    }

    m_listeners.reposition = m_resource->events.reposition.registerListener([this](std::any d) { this->onReposition(); });
    m_listeners.map        = m_resource->surface->events.map.registerListener([this](std::any d) { this->onMap(); });
    m_listeners.unmap      = m_resource->surface->events.unmap.registerListener([this](std::any d) { this->onUnmap(); });
    m_listeners.dismissed  = m_resource->events.dismissed.registerListener([this](std::any d) { this->onUnmap(); });
    m_listeners.destroy    = m_resource->surface->events.destroy.registerListener([this](std::any d) { this->onDestroy(); });
    m_listeners.commit     = m_resource->surface->events.commit.registerListener([this](std::any d) { this->onCommit(); });
    m_listeners.newPopup   = m_resource->surface->events.newPopup.registerListener([this](std::any d) { this->onNewPopup(std::any_cast<SP<CXDGPopupResource>>(d)); });
}

void CPopup::onNewPopup(SP<CXDGPopupResource> popup) {
    const auto& POPUP = m_children.emplace_back(CPopup::create(popup, m_self));
    POPUP->m_self     = POPUP;
    Debug::log(LOG, "New popup at {:x}", (uintptr_t)POPUP);
}

void CPopup::onDestroy() {
    m_inert = true;

    if (!m_parent)
        return; // head node

    std::erase_if(m_parent->m_children, [this](const auto& other) { return other.get() == this; });
}

void CPopup::onMap() {
    if (m_mapped)
        return;

    m_mapped   = true;
    m_lastSize = m_resource->surface->surface->m_current.size;

    const auto COORDS   = coordsGlobal();
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(COORDS);

    CBox       box = m_wlSurface->resource()->extends();
    box.translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(box);

    m_lastPos = coordsRelativeToParent();

    g_pInputManager->simulateMouseMovement();

    m_subsurfaceHead = CSubsurface::create(m_self);

    //unconstrain();
    sendScale();
    m_resource->surface->surface->enter(PMONITOR->m_self.lock());

    if (!m_layerOwner.expired() && m_layerOwner->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_layerOwner->m_layer));
}

void CPopup::onUnmap() {
    if (!m_mapped)
        return;

    if (!m_resource || !m_resource->surface) {
        Debug::log(ERR, "CPopup: orphaned (no surface/resource) and unmaps??");
        onDestroy();
        return;
    }

    m_mapped = false;

    m_lastSize = m_resource->surface->surface->m_current.size;

    const auto COORDS = coordsGlobal();

    CBox       box = m_wlSurface->resource()->extends();
    box.translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(box);

    m_subsurfaceHead.reset();

    if (!m_layerOwner.expired() && m_layerOwner->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_layerOwner->m_layer));

    // damage all children
    breadthfirst(
        [](WP<CPopup> p, void* data) {
            if (!p->m_resource)
                return;

            auto box = CBox{p->coordsGlobal(), p->size()};
            g_pHyprRenderer->damageBox(box);
        },
        nullptr);

    // TODO: probably refocus, but without a motion event?
    // const bool WASLASTFOCUS = g_pSeatManager->state.keyboardFocus == m_pWLSurface->resource() || g_pSeatManager->state.pointerFocus == m_pWLSurface->resource();

    // if (WASLASTFOCUS)
    //     g_pInputManager->simulateMouseMovement();
}

void CPopup::onCommit(bool ignoreSiblings) {
    if (!m_resource || !m_resource->surface) {
        Debug::log(ERR, "CPopup: orphaned (no surface/resource) and commits??");
        onDestroy();
        return;
    }

    if (m_resource->surface->initialCommit) {
        m_resource->surface->scheduleConfigure();
        return;
    }

    if (!m_windowOwner.expired() && (!m_windowOwner->m_isMapped || !m_windowOwner->m_workspace->m_visible)) {
        m_lastSize = m_resource->surface->surface->m_current.size;

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_windowOwner.lock());
        return;
    }

    if (!m_resource->surface->mapped)
        return;

    const auto COORDS      = coordsGlobal();
    const auto COORDSLOCAL = coordsRelativeToParent();

    if (m_lastSize != m_resource->surface->surface->m_current.size || m_requestedReposition || m_lastPos != COORDSLOCAL) {
        CBox box = {localToGlobal(m_lastPos), m_lastSize};
        g_pHyprRenderer->damageBox(box);
        m_lastSize = m_resource->surface->surface->m_current.size;
        box        = {COORDS, m_lastSize};
        g_pHyprRenderer->damageBox(box);

        m_lastPos = COORDSLOCAL;
    }

    if (!ignoreSiblings && m_subsurfaceHead)
        m_subsurfaceHead->recheckDamageForSubsurfaces();

    g_pHyprRenderer->damageSurface(m_wlSurface->resource(), COORDS.x, COORDS.y);

    m_requestedReposition = false;

    if (!m_layerOwner.expired() && m_layerOwner->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_layerOwner->m_layer));
}

void CPopup::onReposition() {
    Debug::log(LOG, "Popup {:x} requests reposition", (uintptr_t)this);

    m_requestedReposition = true;

    m_lastPos = coordsRelativeToParent();

    reposition();
}

void CPopup::reposition() {
    const auto COORDS   = t1ParentCoords();
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(COORDS);

    if (!PMONITOR)
        return;

    CBox box = {PMONITOR->m_position.x, PMONITOR->m_position.y, PMONITOR->m_size.x, PMONITOR->m_size.y};
    m_resource->applyPositioning(box, COORDS);
}

SP<CWLSurface> CPopup::getT1Owner() {
    if (m_windowOwner)
        return m_windowOwner->m_wlSurface;
    else
        return m_layerOwner->m_surface;
}

Vector2D CPopup::coordsRelativeToParent() {
    Vector2D offset;

    if (!m_resource)
        return {};

    WP<CPopup> current = m_self;
    offset -= current->m_resource->surface->current.geometry.pos();

    while (current->m_parent && current->m_resource) {

        offset += current->m_wlSurface->resource()->m_current.offset;
        offset += current->m_resource->geometry.pos();

        current = current->m_parent;
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
    if (!m_windowOwner.expired())
        return m_windowOwner->m_realPosition->value();
    if (!m_layerOwner.expired())
        return m_layerOwner->m_realPosition->value();

    ASSERT(false);
    return {};
}

void CPopup::recheckTree() {
    WP<CPopup> curr = m_self;
    while (curr->m_parent) {
        curr = curr->m_parent;
    }

    curr->recheckChildrenRecursive();
}

void CPopup::recheckChildrenRecursive() {
    if (m_inert || !m_wlSurface)
        return;

    std::vector<WP<CPopup>> cpy;
    std::ranges::for_each(m_children, [&cpy](const auto& el) { cpy.emplace_back(el); });
    for (auto const& c : cpy) {
        c->onCommit(true);
        c->recheckChildrenRecursive();
    }
}

Vector2D CPopup::size() {
    return m_lastSize;
}

void CPopup::sendScale() {
    if (!m_windowOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_wlSurface->resource(), m_windowOwner->m_wlSurface->m_lastScaleFloat);
    else if (!m_layerOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_wlSurface->resource(), m_layerOwner->m_surface->m_lastScaleFloat);
    else
        UNREACHABLE();
}

bool CPopup::visible() {
    if (!m_windowOwner.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_windowOwner.lock());
    if (!m_layerOwner.expired())
        return true;
    if (m_parent)
        return m_parent->visible();

    return false;
}

void CPopup::bfHelper(std::vector<WP<CPopup>> const& nodes, std::function<void(WP<CPopup>, void*)> fn, void* data) {
    for (auto const& n : nodes) {
        fn(n, data);
    }

    std::vector<WP<CPopup>> nodes2;
    nodes2.reserve(nodes.size() * 2);

    for (auto const& n : nodes) {
        for (auto const& c : n->m_children) {
            nodes2.push_back(c->m_self);
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);
}

void CPopup::breadthfirst(std::function<void(WP<CPopup>, void*)> fn, void* data) {
    std::vector<WP<CPopup>> popups;
    popups.push_back(m_self);
    bfHelper(popups, fn, data);
}

WP<CPopup> CPopup::at(const Vector2D& globalCoords, bool allowsInput) {
    std::vector<WP<CPopup>> popups;
    breadthfirst([&popups](WP<CPopup> popup, void* data) { popups.push_back(popup); }, &popups);

    for (auto const& p : popups | std::views::reverse) {
        if (!p->m_resource || !p->m_mapped)
            continue;

        if (!allowsInput) {
            const bool HASSURFACE = p->m_resource && p->m_resource->surface;

            Vector2D   offset = HASSURFACE ? p->m_resource->surface->current.geometry.pos() : Vector2D{};
            Vector2D   size   = HASSURFACE ? p->m_resource->surface->current.geometry.size() : p->size();

            if (size == Vector2D{})
                size = p->size();

            const auto BOX = CBox{p->coordsGlobal() + offset, size};
            if (BOX.containsPoint(globalCoords))
                return p;
        } else {
            const auto REGION = CRegion{p->m_wlSurface->resource()->m_current.input}.intersect(CBox{{}, p->m_wlSurface->resource()->m_current.size}).translate(p->coordsGlobal());
            if (REGION.containsPoint(globalCoords))
                return p;
        }
    }

    return {};
}

bool CPopup::inert() const {
    return m_inert;
}
