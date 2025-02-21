#include "ANRManager.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "HookSystemManager.hpp"
#include "../Compositor.hpp"
#include "../protocols/XDGShell.hpp"
#include "./eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"

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

        if (window->m_bIsX11)
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

void CANRManager::onTick() {
    std::erase_if(m_data, [](const auto& e) { return e.first.expired(); });

    static auto PENABLEANR = CConfigValue<Hyprlang::INT>("misc:enable_anr_dialog");

    if (!*PENABLEANR) {
        m_timer->updateTimeout(TIMER_TIMEOUT * 10);
        return;
    }

    for (auto& [wmBase, data] : m_data) {
        PHLWINDOW firstWindow;
        int       count = 0;
        for (const auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->m_bIsX11 || !w->m_pXDGSurface)
                continue;

            if (w->m_pXDGSurface->owner != wmBase)
                continue;

            count++;
            if (!firstWindow)
                firstWindow = w;
        }

        if (count == 0)
            continue;

        if (data->missedResponses > 0) {
            if (!data->isThreadRunning() && !data->dialogThreadSaidWait) {
                pid_t pid = 0;
                wl_client_get_credentials(wmBase->client(), &pid, nullptr, nullptr);
                data->runDialog("Application Not Responding", firstWindow->m_szTitle, firstWindow->m_szClass, pid);

                for (const auto& w : g_pCompositor->m_vWindows) {
                    if (!w->m_bIsMapped || w->m_bIsX11 || !w->m_pXDGSurface)
                        continue;

                    if (w->m_pXDGSurface->owner != wmBase)
                        continue;

                    *w->m_notRespondingTint = 0.2F;
                }
            }
        } else if (data->isThreadRunning())
            data->killDialog();

        if (data->missedResponses == 0)
            data->dialogThreadSaidWait = false;

        data->missedResponses++;

        wmBase->ping();
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

void CANRManager::SANRData::runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID) {
    if (!dialogThreadExited)
        killDialog();

    // dangerous: might lock if the above failed!!
    if (dialogThread.joinable())
        dialogThread.join();

    dialogThreadExited   = false;
    dialogThreadSaidWait = false;
    dialogThread         = std::thread([title, appName, appClass, dialogWmPID, this]() {
        SP<CProcess> proc =
            makeShared<CProcess>("hyprland-dialog",
                                         std::vector<std::string>{"--title", title, "--text",
                                                          std::format("Application {} with class of {} is not responding.\nWhat do you want to do with it?", appName, appClass),
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

    if (!dialogProc->pid()) {
        Debug::log(ERR, "ANR: cannot kill dialogProc, as it doesn't have a pid. If you have hyprutils <= 0.6.0, you will crash soon. Otherwise, dialog failed to spawn??");
        return;
    }

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
