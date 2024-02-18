#include "Watchdog.hpp"
#include <signal.h>
#include "config/ConfigManager.hpp"

CWatchdog::~CWatchdog() {
    m_bExitThread = true;
    m_bNotified   = true;
    m_cvWatchdogCondition.notify_all();
    m_pWatchdog.reset();
}

CWatchdog::CWatchdog() {
    m_iMainThreadPID = pthread_self();

    m_pWatchdog = std::make_unique<std::thread>([this] {
        static auto* const PTIMEOUT = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("debug:watchdog_timeout");

        while (1337) {
            std::unique_lock lk(m_mWatchdogMutex);

            if (!m_bWillWatch)
                m_cvWatchdogCondition.wait(lk, [this] { return m_bNotified; });
            else {
                if (m_cvWatchdogCondition.wait_for(lk, std::chrono::milliseconds((int)(**PTIMEOUT * 1000.0)), [this] { return m_bNotified; }) == false)
                    pthread_kill(m_iMainThreadPID, SIGUSR1);
            }

            if (m_bExitThread)
                break;

            m_bWatching = false;
            m_bNotified = false;
        }
    });

    m_pWatchdog->detach();
}

void CWatchdog::startWatching() {
    static auto* const PTIMEOUT = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("debug:watchdog_timeout");

    if (**PTIMEOUT == 0)
        return;

    m_tTriggered = std::chrono::high_resolution_clock::now();
    m_bWillWatch = true;
    m_bWatching  = true;

    m_bNotified = true;
    m_cvWatchdogCondition.notify_all();
}

void CWatchdog::endWatching() {
    m_bWatching  = false;
    m_bWillWatch = false;

    m_bNotified = true;
    m_cvWatchdogCondition.notify_all();
}