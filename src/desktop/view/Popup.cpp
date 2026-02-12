#include "Popup.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../protocols/XDGShell.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/animation/AnimationManager.hpp"
#include "LayerSurface.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../render/OpenGL.hpp"
#include <ranges>

using namespace Desktop;
using namespace Desktop::View;

SP<CPopup> CPopup::create(PHLWINDOW pOwner) {
    auto popup           = SP<CPopup>(new CPopup());
    popup->m_windowOwner = pOwner;
    popup->m_self        = popup;
    popup->initAllSignals();
    return popup;
}

SP<CPopup> CPopup::create(PHLLS pOwner) {
    auto popup          = SP<CPopup>(new CPopup());
    popup->m_layerOwner = pOwner;
    popup->m_self       = popup;
    popup->initAllSignals();
    return popup;
}

SP<CPopup> CPopup::create(SP<CXDGPopupResource> resource, WP<CPopup> pOwner) {
    auto popup           = SP<CPopup>(new CPopup());
    popup->m_resource    = resource;
    popup->m_windowOwner = pOwner->m_windowOwner;
    popup->m_layerOwner  = pOwner->m_layerOwner;
    popup->m_parent      = pOwner;
    popup->m_self        = popup;
    popup->wlSurface()->assign(resource->m_surface->m_surface.lock(), popup);

    popup->m_lastSize = resource->m_surface->m_current.geometry.size();
    popup->reposition();

    popup->initAllSignals();
    return popup;
}

SP<CPopup> CPopup::fromView(SP<IView> v) {
    if (!v || v->type() != VIEW_TYPE_POPUP)
        return nullptr;
    return dynamicPointerCast<CPopup>(v);
}

CPopup::CPopup() : IView(CWLSurface::create()) {
    ;
}

CPopup::~CPopup() {
    if (m_wlSurface)
        m_wlSurface->unassign();
}

eViewType CPopup::type() const {
    return VIEW_TYPE_POPUP;
}

bool CPopup::visible() const {
    if ((!m_mapped || !m_wlSurface->resource()) && (!m_fadingOut || m_alpha->value() > 0.F))
        return false;

    if (!m_windowOwner.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_windowOwner.lock());

    if (!m_layerOwner.expired())
        return true;

    if (m_parent)
        return m_parent->visible();

    return false;
}

std::optional<CBox> CPopup::logicalBox() const {
    return surfaceLogicalBox();
}

std::optional<CBox> CPopup::surfaceLogicalBox() const {
    if (!visible())
        return std::nullopt;

    return CBox{coordsGlobal(), size()};
}

bool CPopup::desktopComponent() const {
    return true;
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
    m_listeners.destroy    = m_resource->m_events.destroy.listen([this] { this->onDestroy(); });
    m_listeners.commit     = m_resource->m_surface->m_events.commit.listen([this] { this->onCommit(); });
    m_listeners.newPopup   = m_resource->m_surface->m_events.newPopup.listen([this](const auto& resource) { this->onNewPopup(resource); });
}

void CPopup::onNewPopup(SP<CXDGPopupResource> popup) {
    const auto& POPUP = m_children.emplace_back(CPopup::create(popup, m_self));
    POPUP->m_self     = POPUP;
    Log::logger->log(Log::DEBUG, "New popup at {:x}", rc<uintptr_t>(this));
}

void CPopup::onDestroy() {
    m_inert = true;

    if (!m_parent)
        return; // head node

    m_subsurfaceHead.reset();
    m_children.clear();
    m_wlSurface.reset();

    m_listeners.map.reset();
    m_listeners.unmap.reset();
    m_listeners.commit.reset();
    m_listeners.newPopup.reset();

    if (m_fadingOut && m_alpha->isBeingAnimated()) {
        Log::logger->log(Log::DEBUG, "popup {:x}: skipping full destroy, animating", rc<uintptr_t>(this));
        return;
    }

    fullyDestroy();
}

void CPopup::fullyDestroy() {
    Log::logger->log(Log::DEBUG, "popup {:x} fully destroying", rc<uintptr_t>(this));

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

    Log::logger->log(Log::DEBUG, "popup {:x}: mapped", rc<uintptr_t>(this));
}

void CPopup::onUnmap() {
    if (!m_mapped)
        return;

    if (!m_resource || !m_resource->m_surface) {
        Log::logger->log(Log::ERR, "CPopup: orphaned (no surface/resource) and unmaps??");
        onDestroy();
        return;
    }

    Log::logger->log(Log::DEBUG, "popup {:x}: unmapped", rc<uintptr_t>(this));

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
        Log::logger->log(Log::ERR, "CPopup: orphaned (no surface/resource) and commits??");
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
            Log::logger->log(Log::DEBUG, "Refusing to commit damage from a subsurface of {} because it's invisible.", m_windowOwner.lock());
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
    Log::logger->log(Log::DEBUG, "Popup {:x} requests reposition", rc<uintptr_t>(this));

    m_requestedReposition = true;

    m_lastPos = coordsRelativeToParent();

    reposition();
}

void CPopup::reposition() {
    const auto COORDS   = t1ParentCoords();
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(COORDS);

    if (!PMONITOR)
        return;

    m_resource->applyPositioning(PMONITOR->logicalBoxMinusReserved(), COORDS);
}

SP<Desktop::View::CWLSurface> CPopup::getT1Owner() const {
    if (m_windowOwner)
        return m_windowOwner->wlSurface();
    else
        return m_layerOwner->wlSurface();
}

Vector2D CPopup::coordsRelativeToParent() const {
    Vector2D offset;

    if (!m_resource)
        return m_lastPos;

    WP<CPopup> current = m_self;
    offset -= current->m_resource->m_surface->m_current.geometry.pos();

    while (current->m_parent && current->m_resource) {

        offset += current->wlSurface()->resource()->m_current.offset;
        offset += current->m_resource->m_geometry.pos();

        current = current->m_parent;
    }

    return offset;
}

Vector2D CPopup::coordsGlobal() const {
    return localToGlobal(coordsRelativeToParent());
}

Vector2D CPopup::localToGlobal(const Vector2D& rel) const {
    return t1ParentCoords() + rel;
}

Vector2D CPopup::t1ParentCoords() const {
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
        if (!c->visible())
            continue;
        c->onCommit(true);
        c->recheckChildrenRecursive();
    }
}

Vector2D CPopup::size() const {
    return m_lastSize;
}

void CPopup::sendScale() {
    if (!m_windowOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_wlSurface->resource(), m_windowOwner->wlSurface()->m_lastScaleFloat);
    else if (!m_layerOwner.expired())
        g_pCompositor->setPreferredScaleForSurface(m_wlSurface->resource(), m_layerOwner->wlSurface()->m_lastScaleFloat);
    else
        UNREACHABLE();
}

void CPopup::bfHelper(std::vector<SP<CPopup>> const& nodes, std::function<void(SP<CPopup>, void*)> fn, void* data) {
    for (auto const& n : nodes) {
        fn(n, data);
    }

    std::vector<SP<CPopup>> nodes2;
    nodes2.reserve(nodes.size() * 2);

    for (auto const& n : nodes) {
        if (!n)
            continue;

        for (auto const& c : n->m_children) {
            nodes2.emplace_back(c->m_self.lock());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);
}

void CPopup::breadthfirst(std::function<void(SP<CPopup>, void*)> fn, void* data) {
    if (!m_self)
        return;

    std::vector<SP<CPopup>> popups;
    popups.emplace_back(m_self.lock());
    bfHelper(popups, fn, data);
}

SP<CPopup> CPopup::at(const Vector2D& globalCoords, bool allowsInput) {
    std::vector<SP<CPopup>> popups;
    breadthfirst([&popups](SP<CPopup> popup, void* data) { popups.push_back(popup); }, &popups);

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
            const auto REGION = CRegion{p->wlSurface()->resource()->m_current.input}.intersect(CBox{{}, p->wlSurface()->resource()->m_current.size}).translate(p->coordsGlobal());
            if (REGION.containsPoint(globalCoords))
                return p;
        }
    }

    return {};
}

bool CPopup::inert() const {
    return m_inert;
}

PHLMONITOR CPopup::getMonitor() const {
    if (!m_windowOwner.expired())
        return m_windowOwner->m_monitor.lock();
    if (!m_layerOwner.expired())
        return m_layerOwner->m_monitor.lock();
    return nullptr;
}
