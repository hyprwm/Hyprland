#include "FocusGrab.hpp"
#include "../Compositor.hpp"
#include <hyprland-focus-grab-v1.hpp>
#include "../managers/input/InputManager.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Compositor.hpp"
#include <cstdint>
#include <memory>
#include <wayland-server.h>

CFocusGrabSurfaceState::CFocusGrabSurfaceState(CFocusGrab* grab, SP<CWLSurfaceResource> surface) {
    listeners.destroy = surface->events.destroy.registerListener([=](std::any d) { grab->eraseSurface(surface); });
}

CFocusGrabSurfaceState::~CFocusGrabSurfaceState() {
    ;
}

CFocusGrab::CFocusGrab(SP<CHyprlandFocusGrabV1> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    grab           = makeShared<CSeatGrab>();
    grab->keyboard = true;
    grab->pointer  = true;
    grab->setCallback([this]() { finish(true); });

    resource->setDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    resource->setOnDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    resource->setAddSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { addSurface(CWLSurfaceResource::fromResource(surface)); });
    resource->setRemoveSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { removeSurface(CWLSurfaceResource::fromResource(surface)); });
    resource->setCommit([this](CHyprlandFocusGrabV1* pMgr) { commit(); });
}

CFocusGrab::~CFocusGrab() {
    finish(false);
}

bool CFocusGrab::good() {
    return resource->resource();
}

bool CFocusGrab::isSurfaceComitted(SP<CWLSurfaceResource> surface) {
    auto iter = std::find_if(m_mSurfaces.begin(), m_mSurfaces.end(), [surface](const auto& o) { return o.first == surface; });
    if (iter == m_mSurfaces.end())
        return false;

    return iter->second->state == CFocusGrabSurfaceState::Comitted;
}

void CFocusGrab::start() {
    if (!m_bGrabActive) {
        m_bGrabActive = true;
        g_pSeatManager->setGrab(grab);
    }

    // Ensure new surfaces are focused if under the mouse when comitted.
    g_pInputManager->simulateMouseMovement();
    refocusKeyboard();
}

void CFocusGrab::finish(bool sendCleared) {
    if (m_bGrabActive) {
        m_bGrabActive = false;

        if (g_pSeatManager->seatGrab == grab) {
            g_pSeatManager->setGrab(nullptr);
        }

        grab->clear();
        m_mSurfaces.clear();

        if (sendCleared)
            resource->sendCleared();
    }
}

void CFocusGrab::addSurface(SP<CWLSurfaceResource> surface) {
    auto iter = std::find_if(m_mSurfaces.begin(), m_mSurfaces.end(), [surface](const auto& e) { return e.first == surface; });
    if (iter == m_mSurfaces.end()) {
        m_mSurfaces.emplace(surface, std::make_unique<CFocusGrabSurfaceState>(this, surface));
    }
}

void CFocusGrab::removeSurface(SP<CWLSurfaceResource> surface) {
    auto iter = m_mSurfaces.find(surface);
    if (iter != m_mSurfaces.end()) {
        if (iter->second->state == CFocusGrabSurfaceState::PendingAddition) {
            m_mSurfaces.erase(iter);
        } else
            iter->second->state = CFocusGrabSurfaceState::PendingRemoval;
    }
}

void CFocusGrab::eraseSurface(SP<CWLSurfaceResource> surface) {
    removeSurface(surface);
    commit(true);
}

void CFocusGrab::refocusKeyboard() {
    auto keyboardSurface = g_pSeatManager->state.keyboardFocus;
    if (keyboardSurface && isSurfaceComitted(keyboardSurface.lock()))
        return;

    SP<CWLSurfaceResource> surface = nullptr;
    for (auto& [surf, state] : m_mSurfaces) {
        if (state->state == CFocusGrabSurfaceState::Comitted) {
            surface = surf.lock();
            break;
        }
    }

    if (surface)
        g_pCompositor->focusSurface(surface);
    else
        LOGM(ERR, "CFocusGrab::refocusKeyboard called with no committed surfaces. This should never happen.");
}

void CFocusGrab::commit(bool removeOnly) {
    auto surfacesChanged = false;
    auto anyComitted     = false;
    for (auto iter = m_mSurfaces.begin(); iter != m_mSurfaces.end();) {
        switch (iter->second->state) {
            case CFocusGrabSurfaceState::PendingRemoval:
                grab->remove(iter->first.lock());
                iter            = m_mSurfaces.erase(iter);
                surfacesChanged = true;
                continue;
            case CFocusGrabSurfaceState::PendingAddition:
                if (!removeOnly) {
                    iter->second->state = CFocusGrabSurfaceState::Comitted;
                    grab->add(iter->first.lock());
                    surfacesChanged = true;
                    anyComitted     = true;
                }
                break;
            case CFocusGrabSurfaceState::Comitted: anyComitted = true; break;
        }

        iter++;
    }

    if (surfacesChanged) {
        if (anyComitted)
            start();
        else
            finish(true);
    }
}

CFocusGrabProtocol::CFocusGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFocusGrabProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CHyprlandFocusGrabManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });
    RESOURCE->setCreateGrab([this](CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) { onCreateGrab(pMgr, id); });
}

void CFocusGrabProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CFocusGrabProtocol::destroyGrab(CFocusGrab* grab) {
    std::erase_if(m_vGrabs, [&](const auto& other) { return other.get() == grab; });
}

void CFocusGrabProtocol::onCreateGrab(CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) {
    m_vGrabs.push_back(std::make_unique<CFocusGrab>(makeShared<CHyprlandFocusGrabV1>(pMgr->client(), pMgr->version(), id)));
    const auto RESOURCE = m_vGrabs.back().get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vGrabs.pop_back();
    }
}
