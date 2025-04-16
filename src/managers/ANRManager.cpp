#include "ANRManager.hpp"
#include "../helpers/fs/FsUtils.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "HookSystemManager.hpp"
#include "../Compositor.hpp"
#include "../protocols/XDGShell.hpp"
#include "./eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../xwayland/XSurface.hpp"

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

        for (const auto& d : m_data) {
            if (d->fitsWindow(window))
                return;
        }

        m_data.emplace_back(makeShared<SANRData>(window));
    });

    m_timer->updateTimeout(TIMER_TIMEOUT);
}

void CANRManager::onTick() {
    static auto PENABLEANR    = CConfigValue<Hyprlang::INT>("misc:enable_anr_dialog");
    static auto PANRTHRESHOLD = CConfigValue<Hyprlang::INT>("misc:anr_missed_pings");

    if (!*PENABLEANR) {
        m_timer->updateTimeout(TIMER_TIMEOUT * 10);
        return;
    }

    for (auto& data : m_data) {
        PHLWINDOW firstWindow;
        int       count = 0;
        for (const auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped)
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
                data->runDialog("Application Not Responding", firstWindow->m_szTitle, firstWindow->m_szClass, data->getPid());

                for (const auto& w : g_pCompositor->m_vWindows) {
                    if (!w->m_bIsMapped)
                        continue;

                    if (!data->fitsWindow(w))
                        continue;

                    *w->m_notRespondingTint = 0.2F;
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

void CANRManager::onResponse(SP<CXDGWMBase> wmBase) {
    const auto DATA = dataFor(wmBase);

    if (!DATA)
        return;

    onResponse(DATA);
}

void CANRManager::onResponse(SP<CXWaylandSurface> pXwaylandSurface) {
    const auto DATA = dataFor(pXwaylandSurface);

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
    static auto PANRTHRESHOLD = CConfigValue<Hyprlang::INT>("misc:anr_missed_pings");
    return data->missedResponses > *PANRTHRESHOLD;
}

SP<CANRManager::SANRData> CANRManager::dataFor(PHLWINDOW pWindow) {
    auto it = m_data.end();
    if (pWindow->m_pXWaylandSurface)
        it = std::ranges::find_if(m_data, [&pWindow](const auto& data) { return data->xwaylandSurface && data->xwaylandSurface == pWindow->m_pXWaylandSurface; });
    else if (pWindow->m_pXDGSurface)
        it = std::ranges::find_if(m_data, [&pWindow](const auto& data) { return data->xdgBase && data->xdgBase == pWindow->m_pXDGSurface->owner; });
    return it == m_data.end() ? nullptr : *it;
}

SP<CANRManager::SANRData> CANRManager::dataFor(SP<CXDGWMBase> wmBase) {
    auto it = std::ranges::find_if(m_data, [&wmBase](const auto& data) { return data->xdgBase && data->xdgBase == wmBase; });
    return it == m_data.end() ? nullptr : *it;
}

SP<CANRManager::SANRData> CANRManager::dataFor(SP<CXWaylandSurface> pXwaylandSurface) {
    auto it = std::ranges::find_if(m_data, [&pXwaylandSurface](const auto& data) { return data->xwaylandSurface && data->xwaylandSurface == pXwaylandSurface; });
    return it == m_data.end() ? nullptr : *it;
}

CANRManager::SANRData::SANRData(PHLWINDOW pWindow) :
    xwaylandSurface(pWindow->m_pXWaylandSurface), xdgBase(pWindow->m_pXDGSurface ? pWindow->m_pXDGSurface->owner : WP<CXDGWMBase>{}) {
    ;
}

CANRManager::SANRData::~SANRData() {
    if (dialogBox && dialogBox->isRunning())
        killDialog();
}

void CANRManager::SANRData::runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID) {
    if (dialogBox && dialogBox->isRunning())
        killDialog();

    dialogBox = CAsyncDialogBox::create(title,
                                        std::format("Application {} with class of {} is not responding.\nWhat do you want to do with it?", appName.empty() ? "unknown" : appName,
                                                    appClass.empty() ? "unknown" : appClass),
                                        std::vector<std::string>{"Terminate", "Wait"});

    dialogBox->open([dialogWmPID, this](std::string result) {
        if (result.starts_with("Terminate"))
            ::kill(dialogWmPID, SIGKILL);
        else if (result.starts_with("Wait"))
            dialogSaidWait = true;
        else
            Debug::log(ERR, "CANRManager::SANRData::runDialog: lambda: unrecognized result: {}", result);
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
    if (pWindow->m_pXWaylandSurface)
        return pWindow->m_pXWaylandSurface == xwaylandSurface;
    else if (pWindow->m_pXDGSurface)
        return pWindow->m_pXDGSurface->owner == xdgBase && xdgBase;
    return false;
}

bool CANRManager::SANRData::isDefunct() const {
    return xdgBase.expired() && xwaylandSurface.expired();
}

pid_t CANRManager::SANRData::getPid() const {
    if (xdgBase) {
        pid_t pid = 0;
        wl_client_get_credentials(xdgBase->client(), &pid, nullptr, nullptr);
        return pid;
    }

    if (xwaylandSurface)
        return xwaylandSurface->pid;

    return 0;
}

void CANRManager::SANRData::ping() {
    if (xdgBase) {
        xdgBase->ping();
        return;
    }

    if (xwaylandSurface)
        xwaylandSurface->ping();
}
