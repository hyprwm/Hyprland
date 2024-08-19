#include "ForeignToplevelWlr.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "protocols/core/Output.hpp"
#include "render/Renderer.hpp"

CForeignToplevelHandleWlr::CForeignToplevelHandleWlr(SP<CZwlrForeignToplevelHandleV1> resource_, PHLWINDOW pWindow_) : resource(resource_), pWindow(pWindow_) {
    if (!resource_->resource())
        return;

    resource->setOnDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });
    resource->setDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });

    resource->setActivate([this](CZwlrForeignToplevelHandleV1* p, wl_resource* seat) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        // these requests bypass the config'd stuff cuz it's usually like
        // window switchers and shit
        PWINDOW->activate(true);
    });

    resource->setSetFullscreen([this](CZwlrForeignToplevelHandleV1* p, wl_resource* output) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        if (PWINDOW->m_eSuppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        if (!PWINDOW->m_bIsMapped) {
            PWINDOW->m_bWantsInitialFullscreen = true;
            return;
        }

        if (output) {
            const auto wpMonitor = CWLOutputResource::fromResource(output)->monitor;

            if (!wpMonitor.expired()) {
                const auto monitor = wpMonitor.lock();

                if (PWINDOW->m_pWorkspace != monitor->activeWorkspace) {
                    g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, monitor->activeWorkspace);
                    g_pCompositor->setActiveMonitor(monitor.get());
                }
            }
        }

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_FULLSCREEN, true);
        g_pHyprRenderer->damageWindow(PWINDOW);
    });

    resource->setUnsetFullscreen([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        if (PWINDOW->m_eSuppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_FULLSCREEN, false);
    });

    resource->setSetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        if (PWINDOW->m_eSuppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        if (!PWINDOW->m_bIsMapped) {
            PWINDOW->m_bWantsInitialFullscreen = true;
            return;
        }

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_MAXIMIZED, true);
    });

    resource->setUnsetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        if (PWINDOW->m_eSuppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_MAXIMIZED, false);
    });

    resource->setClose([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = pWindow.lock();

        if (!PWINDOW)
            return;

        g_pCompositor->closeWindow(PWINDOW);
    });
}

bool CForeignToplevelHandleWlr::good() {
    return resource->resource();
}

PHLWINDOW CForeignToplevelHandleWlr::window() {
    return pWindow.lock();
}

wl_resource* CForeignToplevelHandleWlr::res() {
    return resource->resource();
}

void CForeignToplevelHandleWlr::sendMonitor(CMonitor* pMonitor) {
    if (lastMonitorID == pMonitor->ID)
        return;

    const auto CLIENT = resource->client();

    if (const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(lastMonitorID); PLASTMONITOR && PROTO::outputs.contains(PLASTMONITOR->szName)) {
        const auto OLDRESOURCE = PROTO::outputs.at(PLASTMONITOR->szName)->outputResourceFrom(CLIENT);

        if (OLDRESOURCE)
            resource->sendOutputLeave(OLDRESOURCE->getResource()->resource());
    }

    if (PROTO::outputs.contains(pMonitor->szName)) {
        const auto NEWRESOURCE = PROTO::outputs.at(pMonitor->szName)->outputResourceFrom(CLIENT);

        if (NEWRESOURCE)
            resource->sendOutputEnter(NEWRESOURCE->getResource()->resource());
    }

    lastMonitorID = pMonitor->ID;
}

void CForeignToplevelHandleWlr::sendState() {
    const auto PWINDOW = pWindow.lock();

    if (!PWINDOW || !PWINDOW->m_pWorkspace || !PWINDOW->m_bIsMapped)
        return;

    wl_array state;
    wl_array_init(&state);

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        auto p = (uint32_t*)wl_array_add(&state, sizeof(uint32_t));
        *p     = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED;
    }

    if (PWINDOW->isFullscreen()) {
        auto p = (uint32_t*)wl_array_add(&state, sizeof(uint32_t));
        if (PWINDOW->isEffectiveInternalFSMode(FSMODE_FULLSCREEN))
            *p = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN;
        else
            *p = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED;
    }

    resource->sendState(&state);

    wl_array_release(&state);
}

CForeignToplevelWlrManager::CForeignToplevelWlrManager(SP<CZwlrForeignToplevelManagerV1> resource_) : resource(resource_) {
    if (!resource_->resource())
        return;

    resource->setOnDestroy([this](CZwlrForeignToplevelManagerV1* h) { PROTO::foreignToplevelWlr->onManagerResourceDestroy(this); });

    resource->setStop([this](CZwlrForeignToplevelManagerV1* h) {
        resource->sendFinished();
        finished = true;
        LOGM(LOG, "CForeignToplevelWlrManager: finished");
        PROTO::foreignToplevelWlr->onManagerResourceDestroy(this);
    });

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->m_bFadingOut)
            continue;

        onMap(w);
    }

    lastFocus = g_pCompositor->m_pLastWindow;
}

void CForeignToplevelWlrManager::onMap(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto NEWHANDLE = PROTO::foreignToplevelWlr->m_vHandles.emplace_back(
        makeShared<CForeignToplevelHandleWlr>(makeShared<CZwlrForeignToplevelHandleV1>(resource->client(), resource->version(), 0), pWindow));

    if (!NEWHANDLE->good()) {
        LOGM(ERR, "Couldn't create a foreign handle");
        resource->noMemory();
        PROTO::foreignToplevelWlr->m_vHandles.pop_back();
        return;
    }

    LOGM(LOG, "Newly mapped window {:016x}", (uintptr_t)pWindow.get());
    resource->sendToplevel(NEWHANDLE->resource.get());
    NEWHANDLE->resource->sendAppId(pWindow->m_szClass.c_str());
    NEWHANDLE->resource->sendTitle(pWindow->m_szTitle.c_str());
    if (const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID); PMONITOR)
        NEWHANDLE->sendMonitor(PMONITOR);
    NEWHANDLE->sendState();
    NEWHANDLE->resource->sendDone();

    handles.push_back(NEWHANDLE);
}

SP<CForeignToplevelHandleWlr> CForeignToplevelWlrManager::handleForWindow(PHLWINDOW pWindow) {
    std::erase_if(handles, [](const auto& wp) { return wp.expired(); });
    const auto IT = std::find_if(handles.begin(), handles.end(), [pWindow](const auto& h) { return h->window() == pWindow; });
    return IT == handles.end() ? SP<CForeignToplevelHandleWlr>{} : IT->lock();
}

void CForeignToplevelWlrManager::onTitle(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendTitle(pWindow->m_szTitle.c_str());
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onClass(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendAppId(pWindow->m_szClass.c_str());
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onUnmap(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H)
        return;

    H->resource->sendClosed();
    H->resource->sendDone();
    H->closed = true;
}

void CForeignToplevelWlrManager::onMoveMonitor(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR)
        return;

    H->sendMonitor(PMONITOR);
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onFullscreen(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->sendState();
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onNewFocus(PHLWINDOW pWindow) {
    if (finished)
        return;

    if (const auto HOLD = handleForWindow(lastFocus.lock()); HOLD) {
        HOLD->sendState();
        HOLD->resource->sendDone();
    }

    lastFocus = pWindow;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->sendState();
    H->resource->sendDone();
}

bool CForeignToplevelWlrManager::good() {
    return resource->resource();
}

CForeignToplevelWlrProtocol::CForeignToplevelWlrProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("openWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        for (auto& m : m_vManagers) {
            m->onMap(PWINDOW);
        }
    });

    static auto P1 = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        for (auto& m : m_vManagers) {
            m->onUnmap(PWINDOW);
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("windowTitle", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        for (auto& m : m_vManagers) {
            m->onTitle(PWINDOW);
        }
    });

    static auto P3 = g_pHookSystem->hookDynamic("activeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        for (auto& m : m_vManagers) {
            m->onNewFocus(PWINDOW);
        }
    });

    static auto P4 = g_pHookSystem->hookDynamic("moveWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(std::any_cast<std::vector<std::any>>(data).at(0));
        for (auto& m : m_vManagers) {
            m->onMoveMonitor(PWINDOW);
        }
    });

    static auto P5 = g_pHookSystem->hookDynamic("fullscreen", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        for (auto& m : m_vManagers) {
            m->onFullscreen(PWINDOW);
        }
    });
}

void CForeignToplevelWlrProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CForeignToplevelWlrManager>(makeShared<CZwlrForeignToplevelManagerV1>(client, ver, id))).get();

    if (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a foreign list");
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CForeignToplevelWlrProtocol::onManagerResourceDestroy(CForeignToplevelWlrManager* mgr) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == mgr; });
}

void CForeignToplevelWlrProtocol::destroyHandle(CForeignToplevelHandleWlr* handle) {
    std::erase_if(m_vHandles, [&](const auto& other) { return other.get() == handle; });
}

PHLWINDOW CForeignToplevelWlrProtocol::windowFromHandleResource(wl_resource* res) {
    for (auto& h : m_vHandles) {
        if (h->res() != res)
            continue;

        return h->window();
    }

    return nullptr;
}
