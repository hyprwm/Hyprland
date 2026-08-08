#include "ANRManager.hpp"

#include "../helpers/fs/FsUtils.hpp"
#include "../debug/log/Logger.hpp"
#include "../macros.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "./eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../i18n/Engine.hpp"
#include "../event/EventBus.hpp"

using namespace Hyprutils::OS;

static constexpr auto TIMER_TIMEOUT = std::chrono::milliseconds(1500);

CANRManager::CANRManager() {
    if (!NFsUtils::executableExistsInPath("hyprland-dialog")) {
        Log::logger->log(Log::ERR, "hyprland-dialog missing from PATH, cannot start ANRManager");
        return;
    }

    m_timer = makeShared<CEventLoopTimer>(TIMER_TIMEOUT, [this](SP<CEventLoopTimer> self, void* data) { onTick(); }, this);
    g_pEventLoopManager->addTimer(m_timer);

    m_active = true;

    static auto P = Event::bus()->m_events.window.open.listen([this](PHLWINDOW window) {
        for (const auto& d : m_data) {
            // Window is ANR dialog
            if (d->isRunning() && d->dialogBox->getPID() == window->backend().pid())
                return;
        }

        auto data = dataFor(window);
        if (!data)
            data = m_data.emplace_back(makeShared<SANRData>(window));

        data->windows.emplace_back(SANRData::SWindowData{
            .window  = window,
            .pong    = window->backend().m_events.pong.listen([this, clientID = data->clientID] { onResponse(clientID); }),
            .destroy = window->m_events.destroy.listen([this, clientID = data->clientID] {
                const auto DATA = dataFor(clientID);
                if (!DATA)
                    return;

                std::erase_if(DATA->windows, [](const auto& data) { return !data.window; });
                if (DATA->windows.empty())
                    std::erase(m_data, DATA);
            }),
        });
    });

    static auto P1 = Event::bus()->m_events.window.close.listen([this](PHLWINDOW window) {
        const auto DATA = dataFor(window);
        if (!DATA)
            return;

        // Kill the dialog and act as if we got a pong. If this client has more
        // windows, the dialog can reappear after they miss enough pings again.
        DATA->killDialog();
        DATA->missedResponses = 0;
        DATA->dialogSaidWait  = false;
        std::erase_if(DATA->windows, [&window](const auto& data) { return !data.window || data.window == window; });

        if (DATA->windows.empty())
            std::erase(m_data, DATA);
    });

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::onTick() {
    static auto PENABLEANR    = CConfigValue<Config::INTEGER>("misc:enable_anr_dialog");
    static auto PANRTHRESHOLD = CConfigValue<Config::INTEGER>("misc:anr_missed_pings");

    if (!*PENABLEANR) {
        m_timer->updateTimeout(TIMER_TIMEOUT * 10);
        return;
    }

    for (auto& data : m_data) {
        PHLWINDOW firstWindow;
        int       count = 0;
        for (const auto& w : Desktop::windowState()->windows()) {
            if (!w->mapped())
                continue;

            if (!data->fitsWindow(w))
                continue;

            count++;
            if (!firstWindow)
                firstWindow = w;
        }

        if (count == 0)
            continue;

        if (data->missedResponses >= *PANRTHRESHOLD) {
            if (!data->isRunning() && !data->dialogSaidWait) {
                data->runDialog(firstWindow->metadata().title(), firstWindow->metadata().appID(), data->pid);

                for (const auto& w : Desktop::windowState()->windows()) {
                    if (!w->mapped())
                        continue;

                    if (!data->fitsWindow(w))
                        continue;

                    w->presentation().setNotResponding(true);
                }
            }
        } else if (data->isRunning())
            data->killDialog();

        if (data->missedResponses == 0)
            data->dialogSaidWait = false;

        data->missedResponses++;

        data->ping();
    }

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::onResponse(Desktop::View::SBackendClientID clientID) {
    const auto DATA = dataFor(clientID);

    if (!DATA)
        return;

    onResponse(DATA);
}

void CANRManager::onResponse(SP<CANRManager::SANRData> data) {
    data->missedResponses = 0;
    if (data->isRunning())
        data->killDialog();
}

bool CANRManager::isNotResponding(PHLWINDOW pWindow) {
    const auto DATA = dataFor(pWindow);

    if (!DATA)
        return false;

    return isNotResponding(DATA);
}

bool CANRManager::isNotResponding(SP<CANRManager::SANRData> data) {
    static auto PANRTHRESHOLD = CConfigValue<Config::INTEGER>("misc:anr_missed_pings");
    return data->missedResponses > *PANRTHRESHOLD;
}

SP<CANRManager::SANRData> CANRManager::dataFor(PHLWINDOW pWindow) {
    return pWindow ? dataFor(pWindow->backend().clientID()) : nullptr;
}

SP<CANRManager::SANRData> CANRManager::dataFor(Desktop::View::SBackendClientID clientID) {
    auto it = std::ranges::find_if(m_data, [clientID](const auto& data) { return data->clientID == clientID; });
    return it == m_data.end() ? nullptr : *it;
}

CANRManager::SANRData::SANRData(PHLWINDOW pWindow) : clientID(pWindow->backend().clientID()), pid(pWindow->backend().pid()) {
    ;
}

CANRManager::SANRData::~SANRData() {
    if (dialogBox && dialogBox->isRunning())
        killDialog();
}

void CANRManager::SANRData::runDialog(const std::string& appName, const std::string appClass, pid_t dialogWmPID) {
    if (dialogBox && dialogBox->isRunning())
        killDialog();

    const auto OPTION_TERMINATE_STR = I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_OPTION_TERMINATE, {});
    const auto OPTION_WAIT_STR      = I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_OPTION_WAIT, {});
    const auto OPTIONS              = std::vector{OPTION_TERMINATE_STR, OPTION_WAIT_STR};
    const auto CLASS_STR            = appClass.empty() ? I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_PROP_UNKNOWN, {}) : appClass;
    const auto TITLE_STR            = appName.empty() ? I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_PROP_UNKNOWN, {}) : appName;
    const auto DESCRIPTION_STR      = I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_CONTENT, {{"title", TITLE_STR}, {"class", CLASS_STR}});

    dialogBox = CAsyncDialogBox::create(I18n::i18nEngine()->localize(I18n::TXT_KEY_ANR_TITLE, {}), DESCRIPTION_STR, OPTIONS);

    for (const auto& w : Desktop::windowState()->windows()) {
        if (!w->mapped())
            continue;

        if (!fitsWindow(w))
            continue;

        if (w->m_workspace)
            dialogBox->setExecRule(std::format("workspace {} silent", w->m_workspace->getConfigName()));

        break;
    }

    dialogBox->open()->then([dialogWmPID, this, OPTION_TERMINATE_STR, OPTION_WAIT_STR](SP<CPromiseResult<std::string>> r) {
        if (r->hasError()) {
            Log::logger->log(Log::ERR, "CANRManager::SANRData::runDialog: error spawning dialog");
            return;
        }

        const auto& result = r->result();

        if (result.starts_with(OPTION_TERMINATE_STR))
            ::kill(dialogWmPID, SIGKILL);
        else if (result.starts_with(OPTION_WAIT_STR))
            dialogSaidWait = true;
        else
            Log::logger->log(Log::ERR, "CANRManager::SANRData::runDialog: lambda: unrecognized result: {}", result);
    });
}

bool CANRManager::SANRData::isRunning() {
    return dialogBox && dialogBox->isRunning();
}

void CANRManager::SANRData::killDialog() {
    if (!dialogBox)
        return;

    dialogBox->kill();
    dialogBox = nullptr;
}

bool CANRManager::SANRData::fitsWindow(PHLWINDOW pWindow) const {
    return pWindow && pWindow->backend().clientID() == clientID;
}

void CANRManager::SANRData::ping() {
    for (const auto& data : windows) {
        const auto WINDOW = data.window.lock();
        if (!WINDOW || !WINDOW->backend().valid() || !WINDOW->mapped())
            continue;

        WINDOW->backend().ping();
        return;
    }
}
