#pragma once

#include "memory/Memory.hpp"
#include <chrono>
#include <thread>
#include <condition_variable>

class CWatchdog {
  public:
    // must be called from the main thread
    CWatchdog();
    ~CWatchdog();

    void              startWatching();
    void              endWatching();

    std::atomic<bool> m_bWatchdogInitialized{false};

  private:
    std::chrono::high_resolution_clock::time_point m_tTriggered;

    pthread_t                                      m_iMainThreadPID = 0;

    std::atomic<bool>                              m_bWatching  = false;
    std::atomic<bool>                              m_bWillWatch = false;

    UP<std::thread>                                m_pWatchdog;
    std::mutex                                     m_mWatchdogMutex;
    std::atomic<bool>                              m_bNotified   = false;
    std::atomic<bool>                              m_bExitThread = false;
    std::condition_variable                        m_cvWatchdogCondition;
};

inline UP<CWatchdog> g_pWatchdog;