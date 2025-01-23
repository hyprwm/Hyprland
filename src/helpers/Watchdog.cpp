#include "Watchdog.hpp"
#include <csignal>
#include "config/ConfigManager.hpp"
#include "../config/ConfigValue.hpp"

CWatchdog::~CWatchdog() {
    m_bExitThread = true;
    m_bNotified   = true;
    m_cvWatchdogCondition.notify_all();

    if (m_pWatchdog && m_pWatchdog->joinable())
        m_pWatchdog->join();
}

CWatchdog::CWatchdog() : m_iMainThreadPID(pthread_self()) {

    m_pWatchdog = makeUnique<std::thread>([this] {
        static auto PTIMEOUT = CConfigValue<Hyprlang::INT>("debug:watchdog_timeout");

        m_bWatchdogInitialized = true;
        while (!m_bExitThread) {
            std::unique_lock<std::mutex> lk(m_mWatchdogMutex);

            if (!m_bWillWatch)
                m_cvWatchdogCondition.wait(lk, [this] { return m_bNotified || m_bExitThread; });
            else if (!m_cvWatchdogCondition.wait_for(lk, std::chrono::milliseconds((int)(*PTIMEOUT * 1000.0)), [this] { return m_bNotified || m_bExitThread; }))
                pthread_kill(m_iMainThreadPID, SIGUSR1);

            if (m_bExitThread)
                break;

            m_bWatching = false;
            m_bNotified = false;
        }
    });
}

void CWatchdog::startWatching() {
    static auto PTIMEOUT = CConfigValue<Hyprlang::INT>("debug:watchdog_timeout");

    if (*PTIMEOUT == 0)
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