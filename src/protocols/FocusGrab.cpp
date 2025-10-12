#include "FocusGrab.hpp"
#include "../Compositor.hpp"
#include <hyprland-focus-grab-v1.hpp>
#include "../managers/input/InputManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "core/Compositor.hpp"
#include <cstdint>
#include <wayland-server.h>

CFocusGrabSurfaceState::CFocusGrabSurfaceState(CFocusGrab* grab, SP<CWLSurfaceResource> surface) {
    m_listeners.destroy = surface->m_events.destroy.listen([grab, surface] { grab->eraseSurface(surface); });
}

CFocusGrab::CFocusGrab(SP<CHyprlandFocusGrabV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_grab             = makeShared<CSeatGrab>();
    m_grab->m_keyboard = true;
    m_grab->m_pointer  = true;
    m_grab->setCallback([this]() { finish(true); });

    m_resource->setDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    m_resource->setOnDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    m_resource->setAddSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { addSurface(CWLSurfaceResource::fromResource(surface)); });
    m_resource->setRemoveSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { removeSurface(CWLSurfaceResource::fromResource(surface)); });
    m_resource->setCommit([this](CHyprlandFocusGrabV1* pMgr) { commit(); });
}

CFocusGrab::~CFocusGrab() {
    finish(false);
}

bool CFocusGrab::good() {
    return m_resource->resource();
}

bool CFocusGrab::isSurfaceCommitted(SP<CWLSurfaceResource> surface) {
    auto iter = std::ranges::find_if(m_surfaces, [surface](const auto& o) { return o.first == surface; });
    if (iter == m_surfaces.end())
        return false;

    return iter->second->m_state == CFocusGrabSurfaceState::Committed;
}

void CFocusGrab::start() {
    if (!m_grabActive) {
        m_grabActive = true;
        g_pSeatManager->setGrab(m_grab);
    }

    // Ensure new surfaces are focused if under the mouse when committed.
    g_pInputManager->simulateMouseMovement();
    refocusKeyboard();
}

void CFocusGrab::finish(bool sendCleared) {
    if (m_grabActive) {
        m_grabActive = false;

        if (g_pSeatManager->m_seatGrab == m_grab)
            g_pSeatManager->setGrab(nullptr);

        m_grab->clear();
        m_surfaces.clear();

        if (sendCleared)
            m_resource->sendCleared();
    }
}

void CFocusGrab::addSurface(SP<CWLSurfaceResource> surface) {
    auto iter = std::ranges::find_if(m_surfaces, [surface](const auto& e) { return e.first == surface; });
    if (iter == m_surfaces.end())
        m_surfaces.emplace(surface, makeUnique<CFocusGrabSurfaceState>(this, surface));
}

void CFocusGrab::removeSurface(SP<CWLSurfaceResource> surface) {
    auto iter = m_surfaces.find(surface);
    if (iter != m_surfaces.end()) {
        if (iter->second->m_state == CFocusGrabSurfaceState::PendingAddition)
            m_surfaces.erase(iter);
        else
            iter->second->m_state = CFocusGrabSurfaceState::PendingRemoval;
    }
}

void CFocusGrab::eraseSurface(SP<CWLSurfaceResource> surface) {
    removeSurface(surface);
    commit(true);
}

void CFocusGrab::refocusKeyboard() {
    auto keyboardSurface = g_pSeatManager->m_state.keyboardFocus;
    if (keyboardSurface && isSurfaceCommitted(keyboardSurface.lock()))
        return;

    SP<CWLSurfaceResource> surface = nullptr;
    for (auto const& [surf, state] : m_surfaces) {
        if (state->m_state == CFocusGrabSurfaceState::Committed) {
            surface = surf.lock();
            break;
        }
    }

    if (surface)
        Desktop::focusState()->rawSurfaceFocus(surface);
    else
        LOGM(ERR, "CFocusGrab::refocusKeyboard called with no committed surfaces. This should never happen.");
}

void CFocusGrab::commit(bool removeOnly) {
    auto surfacesChanged = false;
    auto anyCommitted    = false;
    for (auto iter = m_surfaces.begin(); iter != m_surfaces.end();) {
        switch (iter->second->m_state) {
            case CFocusGrabSurfaceState::PendingRemoval:
                m_grab->remove(iter->first.lock());
                iter            = m_surfaces.erase(iter);
                surfacesChanged = true;
                continue;
            case CFocusGrabSurfaceState::PendingAddition:
                if (!removeOnly) {
                    iter->second->m_state = CFocusGrabSurfaceState::Committed;
                    m_grab->add(iter->first.lock());
                    surfacesChanged = true;
                    anyCommitted    = true;
                }
                break;
            case CFocusGrabSurfaceState::Committed: anyCommitted = true; break;
        }

        iter++;
    }

    if (surfacesChanged) {
        if (anyCommitted)
            start();
        else
            finish(true);
    }
}

CFocusGrabProtocol::CFocusGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFocusGrabProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CHyprlandFocusGrabManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });
    RESOURCE->setCreateGrab([this](CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) { onCreateGrab(pMgr, id); });
}

void CFocusGrabProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CFocusGrabProtocol::destroyGrab(CFocusGrab* grab) {
    std::erase_if(m_grabs, [&](const auto& other) { return other.get() == grab; });
}

void CFocusGrabProtocol::onCreateGrab(CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) {
    m_grabs.push_back(makeUnique<CFocusGrab>(makeShared<CHyprlandFocusGrabV1>(pMgr->client(), pMgr->version(), id)));
    const auto RESOURCE = m_grabs.back().get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_grabs.pop_back();
    }
}
