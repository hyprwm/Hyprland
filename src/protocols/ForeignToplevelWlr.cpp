#include "ForeignToplevelWlr.hpp"
#include <algorithm>
#include "../Compositor.hpp"

#define LOGM PROTO::foreignToplevelWlr->protoLog

CForeignToplevelHandleWlr::CForeignToplevelHandleWlr(SP<CZwlrForeignToplevelHandleV1> resource_, CWindow* pWindow_) : resource(resource_), pWindow(pWindow_) {
    if (!resource_->resource())
        return;

    resource->setOnDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });
    resource->setDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });

    resource->setActivate([this](CZwlrForeignToplevelHandleV1* p, wl_resource* seat) {
        if (!pWindow)
            return;

        if (pWindow->m_eSuppressedEvents & SUPPRESS_ACTIVATE)
            return;

        pWindow->activate();
    });

    resource->setSetFullscreen([this](CZwlrForeignToplevelHandleV1* p, wl_resource* output) {
        if (!pWindow)
            return;

        if (pWindow->m_eSuppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        if (!pWindow->m_bIsMapped) {
            pWindow->m_bWantsInitialFullscreen = true;
            return;
        }

        g_pCompositor->setWindowFullscreen(pWindow, true);
    });

    resource->setUnsetFullscreen([this](CZwlrForeignToplevelHandleV1* p) {
        if (!pWindow)
            return;

        if (pWindow->m_eSuppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        g_pCompositor->setWindowFullscreen(pWindow, false);
    });

    resource->setSetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        if (!pWindow)
            return;

        if (pWindow->m_eSuppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        if (!pWindow->m_bIsMapped) {
            pWindow->m_bWantsInitialFullscreen = true;
            return;
        }

        g_pCompositor->setWindowFullscreen(pWindow, true, FULLSCREEN_MAXIMIZED);
    });

    resource->setUnsetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        if (!pWindow)
            return;

        if (pWindow->m_eSuppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        g_pCompositor->setWindowFullscreen(pWindow, false);
    });

    resource->setClose([this](CZwlrForeignToplevelHandleV1* p) {
        if (!pWindow)
            return;

        g_pCompositor->closeWindow(pWindow);
    });
}

bool CForeignToplevelHandleWlr::good() {
    return resource->resource();
}

CWindow* CForeignToplevelHandleWlr::window() {
    return pWindow;
}

wl_resource* CForeignToplevelHandleWlr::res() {
    return resource->resource();
}

void CForeignToplevelHandleWlr::sendMonitor(CMonitor* pMonitor) {
    if (lastMonitorID == (int64_t)pMonitor->ID)
        return;

    const auto          CLIENT = wl_resource_get_client(resource->resource());

    struct wl_resource* outputResource;

    if (const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(lastMonitorID); PLASTMONITOR) {
        wl_resource_for_each(outputResource, &PLASTMONITOR->output->resources) {
            if (wl_resource_get_client(outputResource) != CLIENT)
                continue;

            resource->sendOutputLeave(outputResource);
        }
    }

    wl_resource_for_each(outputResource, &pMonitor->output->resources) {
        if (wl_resource_get_client(outputResource) != CLIENT)
            continue;

        resource->sendOutputEnter(outputResource);
    }

    lastMonitorID = pMonitor->ID;
}

void CForeignToplevelHandleWlr::sendState() {
    if (!pWindow || !pWindow->m_pWorkspace || !pWindow->m_bIsMapped)
        return;

    wl_array state;
    wl_array_init(&state);

    if (pWindow == g_pCompositor->m_pLastWindow) {
        auto p = (uint32_t*)wl_array_add(&state, sizeof(uint32_t));
        *p     = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED;
    }

    if (pWindow->m_bIsFullscreen) {
        auto p = (uint32_t*)wl_array_add(&state, sizeof(uint32_t));
        if (pWindow->m_pWorkspace->m_efFullscreenMode == FULLSCREEN_FULL)
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

        onMap(w.get());
    }

    lastFocus = g_pCompositor->m_pLastWindow;
}

void CForeignToplevelWlrManager::onMap(CWindow* pWindow) {
    if (finished)
        return;

    const auto NEWHANDLE = PROTO::foreignToplevelWlr->m_vHandles.emplace_back(std::make_shared<CForeignToplevelHandleWlr>(
        std::make_shared<CZwlrForeignToplevelHandleV1>(wl_resource_get_client(resource->resource()), wl_resource_get_version(resource->resource()), 0), pWindow));

    if (!NEWHANDLE->good()) {
        LOGM(ERR, "Couldn't create a foreign handle");
        wl_resource_post_no_memory(resource->resource());
        PROTO::foreignToplevelWlr->m_vHandles.pop_back();
        return;
    }

    LOGM(LOG, "Newly mapped window {:016x}", (uintptr_t)pWindow);
    resource->sendToplevel(NEWHANDLE->resource.get());
    NEWHANDLE->resource->sendAppId(pWindow->m_szInitialClass.c_str());
    NEWHANDLE->resource->sendTitle(pWindow->m_szInitialTitle.c_str());
    if (const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID); PMONITOR)
        NEWHANDLE->sendMonitor(PMONITOR);
    NEWHANDLE->sendState();
    NEWHANDLE->resource->sendDone();

    handles.push_back(NEWHANDLE);
}

SP<CForeignToplevelHandleWlr> CForeignToplevelWlrManager::handleForWindow(CWindow* pWindow) {
    std::erase_if(handles, [](const auto& wp) { return !wp.lock(); });
    const auto IT = std::find_if(handles.begin(), handles.end(), [pWindow](const auto& h) { return h.lock()->window() == pWindow; });
    return IT == handles.end() ? SP<CForeignToplevelHandleWlr>{} : IT->lock();
}

void CForeignToplevelWlrManager::onTitle(CWindow* pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendTitle(pWindow->m_szTitle.c_str());
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onClass(CWindow* pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendAppId(g_pXWaylandManager->getAppIDClass(pWindow).c_str());
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onUnmap(CWindow* pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H)
        return;

    H->resource->sendClosed();
    H->resource->sendDone();
    H->closed = true;
}

void CForeignToplevelWlrManager::onMoveMonitor(CWindow* pWindow) {
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

void CForeignToplevelWlrManager::onFullscreen(CWindow* pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->sendState();
    H->resource->sendDone();
}

void CForeignToplevelWlrManager::onNewFocus(CWindow* pWindow) {
    if (finished)
        return;

    if (const auto HOLD = handleForWindow(lastFocus); HOLD) {
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
        const auto PWINDOW = std::any_cast<CWindow*>(data);
        for (auto& m : m_vManagers) {
            m->onMap(PWINDOW);
        }
    });

    static auto P1 = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<CWindow*>(data);
        for (auto& m : m_vManagers) {
            m->onUnmap(PWINDOW);
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("windowTitle", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<CWindow*>(data);
        for (auto& m : m_vManagers) {
            m->onTitle(PWINDOW);
        }
    });

    static auto P3 = g_pHookSystem->hookDynamic("activeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<CWindow*>(data);
        for (auto& m : m_vManagers) {
            m->onNewFocus(PWINDOW);
        }
    });

    static auto P4 = g_pHookSystem->hookDynamic("moveWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<CWindow*>(std::any_cast<std::vector<std::any>>(data).at(0));
        for (auto& m : m_vManagers) {
            m->onMoveMonitor(PWINDOW);
        }
    });

    static auto P5 = g_pHookSystem->hookDynamic("fullscreen", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<CWindow*>(data);
        for (auto& m : m_vManagers) {
            m->onFullscreen(PWINDOW);
        }
    });
}

void CForeignToplevelWlrProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CForeignToplevelWlrManager>(std::make_shared<CZwlrForeignToplevelManagerV1>(client, ver, id))).get();

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

CWindow* CForeignToplevelWlrProtocol::windowFromHandleResource(wl_resource* res) {
    for (auto& h : m_vHandles) {
        if (h->res() != res)
            continue;

        return h->window();
    }

    return nullptr;
}