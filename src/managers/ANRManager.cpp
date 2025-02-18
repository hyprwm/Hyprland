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

using namespace Hyprutils::OS;

static constexpr auto TIMER_TIMEOUT = std::chrono::milliseconds(1500);

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

            for (const auto& w : g_pCompositor->m_vWindows) {
                if (!w->m_bIsX11 || w == window || !w->m_pXWaylandSurface)
                    continue;

                if (w->m_pXWaylandSurface->pid == window->m_pXWaylandSurface->pid)
                    return;
            }

            m_xwaylandData[window->m_pXWaylandSurface] = makeShared<SANRData>();
            return;
        }

        if (!window->m_pXDGSurface)
            return;

        for (const auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsX11 || w == window || !w->m_pXDGSurface)
                continue;

            if (w->m_pXDGSurface->owner == window->m_pXDGSurface->owner)
                return;
        }

        m_data[window->m_pXDGSurface->owner] = makeShared<SANRData>();
    });

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

std::pair<PHLWINDOW, int> CANRManager::findFirstWindowAndCount(const WP<CXDGWMBase>& wmBase) {
    PHLWINDOW firstWindow = nullptr;
    int       count       = 0;

    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->m_bIsX11 || !w->m_pXDGSurface)
            continue;

        if (w->m_pXDGSurface->owner != wmBase)
            continue;

        count++;
        if (!firstWindow)
            firstWindow = w;
    }

    return {firstWindow, count};
}

std::pair<PHLWINDOW, int> CANRManager::findFirstXWaylandWindowAndCount(const WP<CXWaylandSurface>& surf) {
    PHLWINDOW firstWindow = nullptr;
    int       count       = 0;

    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || !w->m_bIsX11 || !w->m_pXWaylandSurface)
            continue;

        if (w->m_pXWaylandSurface != surf)
            continue;

        count++;
        if (!firstWindow)
            firstWindow = w;
    }

    return {firstWindow, count};
}

template <typename T>
void CANRManager::setWindowTint(const T& owner, float tint) {
    for (const auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        if constexpr (std::is_same_v<T, WP<CXDGWMBase>>) {
            if (w->m_bIsX11 || !w->m_pXDGSurface || w->m_pXDGSurface->owner != owner)
                continue;
        } else {
            if (!w->m_bIsX11 || !w->m_pXWaylandSurface || w->m_pXWaylandSurface != owner)
                continue;
        }

        *w->m_notRespondingTint = tint;
    }
}

void CANRManager::handleDialog(SP<SANRData>& data, PHLWINDOW firstWindow, pid_t pid, const WP<CXDGWMBase>& wmBase) {
    if (!data->isThreadRunning() && !data->dialogThreadSaidWait) {
        data->runDialog("Application Not Responding", firstWindow->m_szTitle, firstWindow->m_szClass, pid);
        setWindowTint(wmBase, NOT_RESPONDING_TINT);
    }
}

void CANRManager::handleXWaylandDialog(SP<SANRData>& data, PHLWINDOW firstWindow, const WP<CXWaylandSurface>& surf) {
    if (!data->isThreadRunning() && !data->dialogThreadSaidWait) {
        data->runDialog("Application Not Responding", firstWindow->m_szTitle, firstWindow->m_szClass, surf->pid);
        setWindowTint(surf, NOT_RESPONDING_TINT);
    }
}

void CANRManager::sendXWaylandPing(const WP<CXWaylandSurface>& surf) {
    xcb_client_message_event_t event = {.response_type = XCB_CLIENT_MESSAGE,
                                        .format        = 32,
                                        .window        = surf->xID,
                                        .type          = HYPRATOMS["_NET_WM_PING"],
                                        .data          = {.data32 = {XCB_CURRENT_TIME, HYPRATOMS["_NET_WM_PING"], surf->xID}}};

    xcb_send_event(g_pXWayland->pWM->getConnection(), 0, surf->xID, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->pWM->getConnection());
}

void CANRManager::onTick() {
    std::erase_if(m_data, [](const auto& e) { return e.first.expired(); });
    std::erase_if(m_xwaylandData, [](const auto& e) { return e.first.expired(); });

    static auto PENABLEANR = CConfigValue<Hyprlang::INT>("misc:enable_anr_dialog");

    if (!*PENABLEANR) {
        m_timer->updateTimeout(TIMER_TIMEOUT * 10);
        return;
    }

    for (auto& [wmBase, data] : m_data) {
        auto [firstWindow, count] = findFirstWindowAndCount(wmBase);
        if (count == 0)
            continue;

        if (data->missedResponses > 0) {
            pid_t pid = 0;
            wl_client_get_credentials(wmBase->client(), &pid, nullptr, nullptr);
            handleDialog(data, firstWindow, pid, wmBase);
        } else if (data->isThreadRunning())
            data->killDialog();

        if (data->missedResponses == 0)
            data->dialogThreadSaidWait = false;

        data->missedResponses++;
        wmBase->ping();
    }

    for (auto& [surf, data] : m_xwaylandData) {
        auto [firstWindow, count] = findFirstXWaylandWindowAndCount(surf);
        if (count == 0)
            continue;

        if (data->missedResponses > 0) {
            handleXWaylandDialog(data, firstWindow, surf);
        } else if (data->isThreadRunning())
            data->killDialog();

        if (data->missedResponses == 0)
            data->dialogThreadSaidWait = false;

        data->missedResponses++;
        sendXWaylandPing(surf);
    }

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::onResponse(SP<CXDGWMBase> wmBase) {
    if (!m_data.contains(wmBase))
        return;

    auto& data            = m_data.at(wmBase);
    data->missedResponses = 0;
    if (data->isThreadRunning())
        data->killDialog();
}

void CANRManager::onXWaylandResponse(SP<CXWaylandSurface> surf) {
    if (!m_xwaylandData.contains(surf))
        return;

    auto& data            = m_xwaylandData.at(surf);
    data->missedResponses = 0;
    if (data->isThreadRunning())
        data->killDialog();
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

bool CANRManager::isNotResponding(SP<CXDGWMBase> wmBase) {
    if (!m_data.contains(wmBase))
        return false;
    return m_data[wmBase]->missedResponses > 1;
}

bool CANRManager::isXWaylandNotResponding(SP<CXWaylandSurface> surf) {
    if (!m_xwaylandData.contains(surf))
        return false;
    return m_xwaylandData[surf]->missedResponses > 1;
}
