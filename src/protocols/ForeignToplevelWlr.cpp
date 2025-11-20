#include "ForeignToplevelWlr.hpp"
#include "core/Output.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../render/Renderer.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/EventManager.hpp"

CForeignToplevelHandleWlr::CForeignToplevelHandleWlr(SP<CZwlrForeignToplevelHandleV1> resource_, PHLWINDOW pWindow_) : m_resource(resource_), m_window(pWindow_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setData(this);

    m_resource->setOnDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });
    m_resource->setDestroy([this](CZwlrForeignToplevelHandleV1* h) { PROTO::foreignToplevelWlr->destroyHandle(this); });

    m_resource->setActivate([this](CZwlrForeignToplevelHandleV1* p, wl_resource* seat) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        // these requests bypass the config'd stuff cuz it's usually like
        // window switchers and shit
        PWINDOW->activate(true);
        g_pInputManager->simulateMouseMovement();
    });

    m_resource->setSetFullscreen([this](CZwlrForeignToplevelHandleV1* p, wl_resource* output) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (PWINDOW->m_suppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        if UNLIKELY (!PWINDOW->m_isMapped) {
            PWINDOW->m_wantsInitialFullscreen = true;
            return;
        }

        if (output) {
            const auto wpMonitor = CWLOutputResource::fromResource(output)->m_monitor;

            if (!wpMonitor.expired()) {
                const auto monitor = wpMonitor.lock();

                if (PWINDOW->m_workspace != monitor->m_activeWorkspace) {
                    g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, monitor->m_activeWorkspace);
                    Desktop::focusState()->rawMonitorFocus(monitor);
                }
            }
        }

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_FULLSCREEN, true);
        g_pHyprRenderer->damageWindow(PWINDOW);
    });

    m_resource->setUnsetFullscreen([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (PWINDOW->m_suppressedEvents & SUPPRESS_FULLSCREEN)
            return;

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_FULLSCREEN, false);
    });

    m_resource->setSetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (PWINDOW->m_suppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        if UNLIKELY (!PWINDOW->m_isMapped) {
            PWINDOW->m_wantsInitialFullscreen = true;
            return;
        }

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_MAXIMIZED, true);
    });

    m_resource->setUnsetMaximized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (PWINDOW->m_suppressedEvents & SUPPRESS_MAXIMIZE)
            return;

        g_pCompositor->changeWindowFullscreenModeClient(PWINDOW, FSMODE_MAXIMIZED, false);
    });

    m_resource->setSetMinimized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (!PWINDOW->m_isMapped)
            return;

        g_pEventManager->postEvent(SHyprIPCEvent{.event = "minimized", .data = std::format("{:x},1", rc<uintptr_t>(PWINDOW.get()))});
    });

    m_resource->setUnsetMinimized([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        if UNLIKELY (!PWINDOW->m_isMapped)
            return;

        g_pEventManager->postEvent(SHyprIPCEvent{.event = "minimized", .data = std::format("{:x},0", rc<uintptr_t>(PWINDOW.get()))});
    });

    m_resource->setClose([this](CZwlrForeignToplevelHandleV1* p) {
        const auto PWINDOW = m_window.lock();

        if UNLIKELY (!PWINDOW)
            return;

        g_pCompositor->closeWindow(PWINDOW);
    });
}

bool CForeignToplevelHandleWlr::good() {
    return m_resource->resource();
}

PHLWINDOW CForeignToplevelHandleWlr::window() {
    return m_window.lock();
}

wl_resource* CForeignToplevelHandleWlr::res() {
    return m_resource->resource();
}

void CForeignToplevelHandleWlr::sendMonitor(PHLMONITOR pMonitor) {
    if (m_lastMonitorID == pMonitor->m_id)
        return;

    const auto CLIENT = m_resource->client();

    if (const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(m_lastMonitorID); PLASTMONITOR && PROTO::outputs.contains(PLASTMONITOR->m_name)) {
        const auto OLDRESOURCE = PROTO::outputs.at(PLASTMONITOR->m_name)->outputResourceFrom(CLIENT);

        if LIKELY (OLDRESOURCE)
            m_resource->sendOutputLeave(OLDRESOURCE->getResource()->resource());
    }

    if (PROTO::outputs.contains(pMonitor->m_name)) {
        const auto NEWRESOURCE = PROTO::outputs.at(pMonitor->m_name)->outputResourceFrom(CLIENT);

        if LIKELY (NEWRESOURCE)
            m_resource->sendOutputEnter(NEWRESOURCE->getResource()->resource());
    }

    m_lastMonitorID = pMonitor->m_id;
}

void CForeignToplevelHandleWlr::sendState() {
    const auto PWINDOW = m_window.lock();

    if UNLIKELY (!PWINDOW || !PWINDOW->m_workspace || !PWINDOW->m_isMapped)
        return;

    wl_array state;
    wl_array_init(&state);

    if (PWINDOW == Desktop::focusState()->window()) {
        auto p = sc<uint32_t*>(wl_array_add(&state, sizeof(uint32_t)));
        *p     = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED;
    }

    if (PWINDOW->isFullscreen()) {
        auto p = sc<uint32_t*>(wl_array_add(&state, sizeof(uint32_t)));
        if (PWINDOW->isEffectiveInternalFSMode(FSMODE_FULLSCREEN))
            *p = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN;
        else
            *p = ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED;
    }

    m_resource->sendState(&state);

    wl_array_release(&state);
}

CForeignToplevelWlrManager::CForeignToplevelWlrManager(SP<CZwlrForeignToplevelManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setOnDestroy([this](CZwlrForeignToplevelManagerV1* h) { PROTO::foreignToplevelWlr->onManagerResourceDestroy(this); });

    m_resource->setStop([this](CZwlrForeignToplevelManagerV1* h) {
        m_resource->sendFinished();
        m_finished = true;
        LOGM(LOG, "CForeignToplevelWlrManager: finished");
        PROTO::foreignToplevelWlr->onManagerResourceDestroy(this);
    });

    for (auto const& w : g_pCompositor->m_windows) {
        if (!PROTO::foreignToplevelWlr->windowValidForForeign(w))
            continue;

        onMap(w);
    }

    m_lastFocus = Desktop::focusState()->window();
}

void CForeignToplevelWlrManager::onMap(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto NEWHANDLE = PROTO::foreignToplevelWlr->m_handles.emplace_back(
        makeShared<CForeignToplevelHandleWlr>(makeShared<CZwlrForeignToplevelHandleV1>(m_resource->client(), m_resource->version(), 0), pWindow));

    if UNLIKELY (!NEWHANDLE->good()) {
        LOGM(ERR, "Couldn't create a foreign handle");
        m_resource->noMemory();
        PROTO::foreignToplevelWlr->m_handles.pop_back();
        return;
    }

    LOGM(LOG, "Newly mapped window {:016x}", (uintptr_t)pWindow.get());
    m_resource->sendToplevel(NEWHANDLE->m_resource.get());
    NEWHANDLE->m_resource->sendAppId(pWindow->m_class.c_str());
    NEWHANDLE->m_resource->sendTitle(pWindow->m_title.c_str());
    if LIKELY (const auto PMONITOR = pWindow->m_monitor.lock(); PMONITOR)
        NEWHANDLE->sendMonitor(PMONITOR);
    NEWHANDLE->sendState();
    NEWHANDLE->m_resource->sendDone();

    m_handles.push_back(NEWHANDLE);
}

SP<CForeignToplevelHandleWlr> CForeignToplevelWlrManager::handleForWindow(PHLWINDOW pWindow) {
    std::erase_if(m_handles, [](const auto& wp) { return wp.expired(); });
    const auto IT = std::ranges::find_if(m_handles, [pWindow](const auto& h) { return h->window() == pWindow; });
    return IT == m_handles.end() ? SP<CForeignToplevelHandleWlr>{} : IT->lock();
}

void CForeignToplevelWlrManager::onTitle(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->m_resource->sendTitle(pWindow->m_title.c_str());
    H->m_resource->sendDone();
}

void CForeignToplevelWlrManager::onClass(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->m_resource->sendAppId(pWindow->m_class.c_str());
    H->m_resource->sendDone();
}

void CForeignToplevelWlrManager::onUnmap(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H)
        return;

    H->m_resource->sendClosed();
    H->m_resource->sendDone();
    H->m_closed = true;
}

void CForeignToplevelWlrManager::onMoveMonitor(PHLWINDOW pWindow, PHLMONITOR pMonitor) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed || !pMonitor)
        return;

    H->sendMonitor(pMonitor);
    H->m_resource->sendDone();
}

void CForeignToplevelWlrManager::onFullscreen(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->sendState();
    H->m_resource->sendDone();
}

void CForeignToplevelWlrManager::onNewFocus(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    if LIKELY (const auto HOLD = handleForWindow(m_lastFocus.lock()); HOLD) {
        HOLD->sendState();
        HOLD->m_resource->sendDone();
    }

    m_lastFocus = pWindow;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->sendState();
    H->m_resource->sendDone();
}

bool CForeignToplevelWlrManager::good() {
    return m_resource->resource();
}

CForeignToplevelWlrProtocol::CForeignToplevelWlrProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("openWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(PWINDOW))
            return;

        for (auto const& m : m_managers) {
            m->onMap(PWINDOW);
        }
    });

    static auto P1 = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(PWINDOW))
            return;

        for (auto const& m : m_managers) {
            m->onUnmap(PWINDOW);
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("windowTitle", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(PWINDOW))
            return;

        for (auto const& m : m_managers) {
            m->onTitle(PWINDOW);
        }
    });

    static auto P3 = g_pHookSystem->hookDynamic("activeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

        if (PWINDOW && !windowValidForForeign(PWINDOW))
            return;

        for (auto const& m : m_managers) {
            m->onNewFocus(PWINDOW);
        }
    });

    static auto P4 = g_pHookSystem->hookDynamic("moveWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW    = std::any_cast<PHLWINDOW>(std::any_cast<std::vector<std::any>>(data).at(0));
        const auto PWORKSPACE = std::any_cast<PHLWORKSPACE>(std::any_cast<std::vector<std::any>>(data).at(1));

        if (!PWORKSPACE)
            return;

        for (auto const& m : m_managers) {
            m->onMoveMonitor(PWINDOW, PWORKSPACE->m_monitor.lock());
        }
    });

    static auto P5 = g_pHookSystem->hookDynamic("fullscreen", [this](void* self, SCallbackInfo& info, std::any data) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(PWINDOW))
            return;

        for (auto const& m : m_managers) {
            m->onFullscreen(PWINDOW);
        }
    });
}

void CForeignToplevelWlrProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CForeignToplevelWlrManager>(makeShared<CZwlrForeignToplevelManagerV1>(client, ver, id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a foreign list");
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CForeignToplevelWlrProtocol::onManagerResourceDestroy(CForeignToplevelWlrManager* mgr) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == mgr; });
}

void CForeignToplevelWlrProtocol::destroyHandle(CForeignToplevelHandleWlr* handle) {
    std::erase_if(m_handles, [&](const auto& other) { return other.get() == handle; });
}

PHLWINDOW CForeignToplevelWlrProtocol::windowFromHandleResource(wl_resource* res) {
    auto data = sc<CForeignToplevelHandleWlr*>(sc<CZwlrForeignToplevelHandleV1*>(wl_resource_get_user_data(res))->data());
    return data ? data->window() : nullptr;
}

bool CForeignToplevelWlrProtocol::windowValidForForeign(PHLWINDOW pWindow) {
    return validMapped(pWindow) && !pWindow->isX11OverrideRedirect();
}
