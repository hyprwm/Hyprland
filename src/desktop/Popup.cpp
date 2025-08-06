#include "Popup.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../Compositor.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/AnimationManager.hpp"
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
    popup->m_wlSurface->assign(resource->m_surface->m_surface.lock(), popup.get());

    popup->m_lastSize = resource->m_surface->m_current.geometry.size();
    popup->reposition();

    popup->initAllSignals();
    return popup;
}

CPopup::~CPopup() {
    if (m_wlSurface)
        m_wlSurface->unassign();
}

void CPopup::initAllSignals() {

    g_pAnimationManager->createAnimation(0.f, m_alpha, g_pConfigManager->getAnimationPropertyConfig("fadePopupsIn"), AVARDAMAGE_NONE);
    m_alpha->setUpdateCallback([this](auto) {
        //
        g_pHyprRenderer->damageBox(CBox{coordsGlobal(), size()});
    });
    m_alpha->setCallbackOnEnd(
        [this](auto) {
            if (inert()) {
                g_pHyprRenderer->damageBox(CBox{coordsGlobal(), size()});
                fullyDestroy();
            }
        },
        false);

    if (!m_resource) {
        if (!m_windowOwner.expired())
            m_listeners.newPopup = m_windowOwner->m_xdgSurface->m_events.newPopup.listen([this](const auto& resource) { this->onNewPopup(resource); });
        else if (!m_layerOwner.expired())
            m_listeners.newPopup = m_layerOwner->m_layerSurface->m_events.newPopup.listen([this](const auto& resource) { this->onNewPopup(resource); });
        else
            ASSERT(false);

        return;
    }

    m_listeners.reposition = m_resource->m_events.reposition.listen([this] { this->onReposition(); });
    m_listeners.map        = m_resource->m_surface->m_events.map.listen([this] { this->onMap(); });
    m_listeners.unmap      = m_resource->m_surface->m_events.unmap.listen([this] { this->onUnmap(); });
    m_listeners.dismissed  = m_resource->m_events.dismissed.listen([this] { this->onUnmap(); });
    m_listeners.destroy    = m_resource->m_surface->m_events.destroy.listen([this] { this->onDestroy(); });
    m_listeners.commit     = m_resource->m_surface->m_events.commit.listen([this] { this->onCommit(); });
    m_listeners.newPopup   = m_resource->m_surface->m_events.newPopup.listen([this](const auto& resource) { this->onNewPopup(resource); });
}

void CPopup::onNewPopup(SP<CXDGPopupResource> popup) {
    const auto& POPUP = m_children.emplace_back(CPopup::create(popup, m_self));
    POPUP->m_self     = POPUP;
    Debug::log(LOG, "New popup at {:x}", reinterpret_cast<uintptr_t>(this));
}

void CPopup::onDestroy() {
    m_inert = true;

    if (!m_parent)
        return; // head node

    m_subsurfaceHead.reset();
    m_children.clear();
    m_wlSurface.reset();

    if (m_fadingOut && m_alpha->isBeingAnimated()) {
        Debug::log(LOG, "popup {:x}: skipping full destroy, animating", reinterpret_cast<uintptr_t>(this));
        return;
    }

    fullyDestroy();
}

void CPopup::fullyDestroy() {
    Debug::log(LOG, "popup {:x} fully destroying", reinterpret_cast<uintptr_t>(this));

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_popupFramebuffers, [&](const auto& other) { return other.first.expired() || other.first == m_self; });

    std::erase_if(m_parent->m_children, [this](const auto& other) { return other.get() == this; });
}

void CPopup::onMap() {
    if (m_mapped)
        return;

    m_mapped   = true;
    m_lastSize = m_resource->m_surface->m_surface->m_current.size;

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
    m_resource->m_surface->m_surface->enter(PMONITOR->m_self.lock());

    if (!m_layerOwner.expired() && m_layerOwner->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP)
        g_pHyprOpenGL->markBlurDirtyForMonitor(g_pCompositor->getMonitorFromID(m_layerOwner->m_layer));

    m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadePopupsIn"));
    m_alpha->setValueAndWarp(0.F);
    *m_alpha = 1.F;

    Debug::log(LOG, "popup {:x}: mapped", reinterpret_cast<uintptr_t>(this));
}

void CPopup::onUnmap() {
    if (!m_mapped)
        return;

    if (!m_resource || !m_resource->m_surface) {
        Debug::log(ERR, "CPopup: orphaned (no surface/resource) and unmaps??");
        onDestroy();
        return;
    }

    Debug::log(LOG, "popup {:x}: unmapped", reinterpret_cast<uintptr_t>(this));

    // if the popup committed a different size right now, we also need to damage the old size.
    const Vector2D MAX_DAMAGE_SIZE = {std::max(m_lastSize.x, m_resource->m_surface->m_surface->m_current.size.x),
                                      std::max(m_lastSize.y, m_resource->m_surface->m_surface->m_current.size.y)};

    m_lastSize = m_resource->m_surface->m_surface->m_current.size;
    m_lastPos  = coordsRelativeToParent();

    const auto COORDS = coordsGlobal();

    CBox       box = m_wlSurface->resource()->extends();
    box.translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(box);

    // damage the last popup's explicit max size as well
    box = CBox{COORDS, MAX_DAMAGE_SIZE}.expand(4);
    g_pHyprRenderer->damageBox(box);

    m_lastSize = MAX_DAMAGE_SIZE;

    g_pHyprRenderer->makeSnapshot(m_self);

    m_fadingOut = true;
    m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig("fadePopupsOut"));
    m_alpha->setValueAndWarp(1.F);
    *m_alpha = 0.F;

    m_mapped = false;

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
    if (!m_resource || !m_resource->m_surface) {
        Debug::log(ERR, "CPopup: orphaned (no surface/resource) and commits??");
        onDestroy();
        return;
    }

    if (m_resource->m_surface->m_initialCommit) {
        m_resource->m_surface->scheduleConfigure();
        return;
    }

    if (!m_windowOwner.expired() && (!m_windowOwner->m_isMapped || !m_windowOwner->m_workspace->m_visible)) {
        m_lastSize = m_resource->m_surface->m_surface->m_current.size;

        static auto PLOGDAMAGE = CConfigValue<Hyprlang::INT>("debug:log_damage");
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_windowOwner.lock());
        return;
    }

    if (!m_resource->m_surface->m_mapped)
        return;

    const auto COORDS      = coordsGlobal();
    const auto COORDSLOCAL = coordsRelativeToParent();

    if (m_lastSize != m_resource->m_surface->m_surface->m_current.size || m_requestedReposition || m_lastPos != COORDSLOCAL) {
        CBox box = {localToGlobal(m_lastPos), m_lastSize};
        g_pHyprRenderer->damageBox(box);
        m_lastSize = m_resource->m_surface->m_surface->m_current.size;
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
    Debug::log(LOG, "Popup {:x} requests reposition", reinterpret_cast<uintptr_t>(this));

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
        return m_lastPos;

    WP<CPopup> current = m_self;
    offset -= current->m_resource->m_surface->m_current.geometry.pos();

    while (current->m_parent && current->m_resource) {

        offset += current->m_wlSurface->resource()->m_current.offset;
        offset += current->m_resource->m_geometry.pos();

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
            const bool HASSURFACE = p->m_resource && p->m_resource->m_surface;

            Vector2D   offset = HASSURFACE ? p->m_resource->m_surface->m_current.geometry.pos() : Vector2D{};
            Vector2D   size   = HASSURFACE ? p->m_resource->m_surface->m_current.geometry.size() : p->size();

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

PHLMONITOR CPopup::getMonitor() {
    if (!m_windowOwner.expired())
        return m_windowOwner->m_monitor.lock();
    if (!m_layerOwner.expired())
        return m_layerOwner->m_monitor.lock();
    return nullptr;
}
