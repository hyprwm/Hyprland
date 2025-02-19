#include "ANRManager.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "HookSystemManager.hpp"
#include "../Compositor.hpp"
#include "../protocols/XDGShell.hpp"
#include "./eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../xwayland/XWayland.hpp"
#ifndef NO_XWAYLAND
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#endif

using namespace Hyprutils::OS;

static constexpr auto TIMER_TIMEOUT = std::chrono::milliseconds(1500);

template <typename T>
bool CANRManager::isMatchingWindow(const PHLWINDOW& window, const T& owner) {
    if constexpr (std::is_same_v<T, WP<CXDGWMBase>>)
        return !window->m_bIsX11 && window->m_pXDGSurface && window->m_pXDGSurface->owner == owner;
    else
        return window->m_bIsX11 && window->m_pXWaylandSurface && window->m_pXWaylandSurface == owner;
}

template <typename T>
bool CANRManager::hasWindowWithSameOwner(const PHLWINDOW& window) {
    if constexpr (std::is_same_v<T, CXDGWMBase>) {
        if (!window->m_pXDGSurface)
            return false;

        for (const auto& w : g_pCompositor->m_vWindows) {
            if (w == window || w->m_bIsX11 || !w->m_pXDGSurface)
                continue;

            if (w->m_pXDGSurface->owner == window->m_pXDGSurface->owner)
                return true;
        }
    } else {
        if (!window->m_pXWaylandSurface)
            return false;

        for (const auto& w : g_pCompositor->m_vWindows) {
            if (w == window || !w->m_bIsX11 || !w->m_pXWaylandSurface)
                continue;

            if (w->m_pXWaylandSurface->pid == window->m_pXWaylandSurface->pid)
                return true;
        }
    }
    return false;
}

template <typename T>
std::pair<PHLWINDOW, int> CANRManager::findFirstWindowAndCount(const WP<T>& owner) {
    PHLWINDOW firstWindow = nullptr;
    int       count       = 0;

    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        if (!isMatchingWindow(w, owner))
            continue;

        count++;
        if (!firstWindow)
            firstWindow = w;
    }

    return {firstWindow, count};
}

template <typename T>
void CANRManager::handleANRDialog(SP<SANRData>& data, PHLWINDOW firstWindow, const WP<T>& owner) {
    if (!data->isThreadRunning() && !data->dialogThreadSaidWait) {
        pid_t pid = 0;
        if constexpr (std::is_same_v<T, CXDGWMBase>)
            wl_client_get_credentials(owner->client(), &pid, nullptr, nullptr);
        else
            pid = owner->pid;

        data->runDialog("Application Not Responding", firstWindow->m_szTitle, firstWindow->m_szClass, pid);
        setWindowTint(owner, NOT_RESPONDING_TINT);
    }
}

template <typename T>
void CANRManager::setWindowTint(const T& owner, float tint) {
    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        if (!isMatchingWindow(w, owner))
            continue;

        *w->m_notRespondingTint = tint;
    }
}

void CANRManager::sendXWaylandPing(const WP<CXWaylandSurface>& surf) {
#ifndef NO_XWAYLAND
    xcb_client_message_event_t event = {.response_type = XCB_CLIENT_MESSAGE,
                                        .format        = 32,
                                        .window        = surf->xID,
                                        .type          = HYPRATOMS["_NET_WM_PING"],
                                        .data          = {.data32 = {XCB_CURRENT_TIME, HYPRATOMS["_NET_WM_PING"], surf->xID}}};

    xcb_send_event(g_pXWayland->pWM->getConnection(), 0, surf->xID, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->pWM->getConnection());
#endif
}

template <typename T>
void CANRManager::handleANRData(std::map<WP<T>, SP<SANRData>>& dataMap) {
    for (auto& [owner, data] : dataMap) {
        auto [firstWindow, count] = findFirstWindowAndCount(owner);
        if (count == 0)
            continue;

        if (data->missedResponses > 0)
            handleANRDialog(data, firstWindow, owner);
        else if (data->isThreadRunning())
            data->killDialog();

        if (data->missedResponses == 0)
            data->dialogThreadSaidWait = false;

        data->missedResponses++;

        if constexpr (std::is_same_v<T, CXDGWMBase>)
            owner->ping();
        else
            sendXWaylandPing(owner);
    }
}

template <typename T>
void CANRManager::handleResponse(std::map<WP<T>, SP<SANRData>>& dataMap, SP<T> owner) {
    if (auto it = dataMap.find(owner); it != dataMap.end()) {
        auto& data            = it->second;
        data->missedResponses = 0;
        if (data->isThreadRunning())
            data->killDialog();
    }
}

template <typename T>
bool CANRManager::isNotRespondingImpl(const std::map<WP<T>, SP<SANRData>>& dataMap, SP<T> owner) {
    if (auto it = dataMap.find(owner); it != dataMap.end())
        return it->second->missedResponses > 1;
    return false;
}

void CANRManager::onResponse(SP<CXDGWMBase> wmBase) {
    handleResponse(m_data, wmBase);
}

void CANRManager::onXWaylandResponse(SP<CXWaylandSurface> surf) {
    handleResponse(m_xwaylandData, surf);
}

bool CANRManager::isNotResponding(SP<CXDGWMBase> wmBase) {
    return isNotRespondingImpl(m_data, wmBase);
}

bool CANRManager::isXWaylandNotResponding(SP<CXWaylandSurface> surf) {
    return isNotRespondingImpl(m_xwaylandData, surf);
}

CANRManager::CANRManager() {
    if (!NFsUtils::executableExistsInPath("hyprland-dialog")) {
        Debug::log(ERR, "hyprland-dialog missing from PATH, cannot start ANRManager");
        return;
    }

    m_timer = makeShared<CEventLoopTimer>(TIMER_TIMEOUT, [this](SP<CEventLoopTimer> self, void* data) { onTick(); }, this);
    g_pEventLoopManager->addTimer(m_timer);

    m_active = true;

    static auto P = g_pHookSystem->hookDynamic("openWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        if (window->m_bIsX11) {
            if (!window->m_pXWaylandSurface)
                return;

            if (hasWindowWithSameOwner<CXWaylandSurface>(window))
                return;

            m_xwaylandData[window->m_pXWaylandSurface] = makeShared<SANRData>();
            return;
        }

        if (!window->m_pXDGSurface)
            return;

        if (hasWindowWithSameOwner<CXDGWMBase>(window))
            return;

        m_data[window->m_pXDGSurface->owner] = makeShared<SANRData>();
    });

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::onTick() {
    std::erase_if(m_data, [](const auto& e) { return e.first.expired(); });
    std::erase_if(m_xwaylandData, [](const auto& e) { return e.first.expired(); });

    static auto PENABLEANR = CConfigValue<Hyprlang::INT>("misc:enable_anr_dialog");

    if (!m_active && *PENABLEANR) {
        m_active = true;
        m_timer->updateTimeout(TIMER_TIMEOUT);
    }

    handleANRData(m_data);
    handleANRData(m_xwaylandData);

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::SANRData::runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID) {
    if (!dialogThreadExited)
        killDialog();

    // dangerous: might lock if the above failed!!
    if (dialogThread.joinable())
        dialogThread.join();

    dialogThreadExited   = false;
    dialogThreadSaidWait = false;
    dialogThread         = std::thread([title, appName, appClass, dialogWmPID, this]() {
        const auto   name      = appName.empty() ? "unnamed" : appName;
        const auto   className = appClass.empty() ? "unnamed" : appClass;

        SP<CProcess> proc =
            makeShared<CProcess>("hyprland-dialog",
                                         std::vector<std::string>{"--title", title, "--text",
                                                          std::format("Application {} with class of {} is not responding.\nWhat do you want to do with it?", name, className),
                                                          "--buttons", "Terminate;Wait"});

        dialogProc = proc;
        proc->runSync();

        dialogThreadExited = true;

        if (proc->stdOut().empty())
            return;

        if (proc->stdOut().starts_with("Terminate"))
            kill(dialogWmPID, SIGKILL);
        if (proc->stdOut().starts_with("Wait"))
            dialogThreadSaidWait = true;
    });
}

bool CANRManager::SANRData::isThreadRunning() {
    if (dialogThread.native_handle() == 0)
        return false;
    if (dialogThreadExited)
        return false;
    return pthread_kill(dialogThread.native_handle(), 0) != ESRCH;
}

void CANRManager::SANRData::killDialog() const {
    if (!dialogProc)
        return;

    kill(dialogProc->pid(), SIGKILL);
}

CANRManager::SANRData::~SANRData() {
    if (dialogThread.joinable()) {
        killDialog();
        // dangerous: might lock if the above failed!!
        dialogThread.join();
    }
}
