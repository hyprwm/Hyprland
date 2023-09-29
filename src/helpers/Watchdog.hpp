#pragma once

#include <memory>
#include <chrono>
#include <thread>
#include <condition_variable>

class CWatchdog {
  public:
    // must be called from the main thread
    CWatchdog();
    ~CWatchdog();

    void startWatching();
    void endWatching();

  private:
    std::chrono::high_resolution_clock::time_point m_tTriggered;

    pthread_t                                      m_iMainThreadPID = 0;

    bool                                           m_bWatching  = false;
    bool                                           m_bWillWatch = false;

    std::unique_ptr<std::thread>                   m_pWatchdog;
    std::mutex                                     m_mWatchdogMutex;
    bool                                           m_bNotified   = false;
    bool                                           m_bExitThread = false;
    std::condition_variable                        m_cvWatchdogCondition;
};

inline std::unique_ptr<CWatchdog> g_pWatchdog;