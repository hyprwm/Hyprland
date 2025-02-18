#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <map>
#include "./eventLoop/EventLoopTimer.hpp"
#include "../helpers/signal/Signal.hpp"
#include <atomic>
#include <thread>

class CXDGWMBase;

class CANRManager {
  public:
    CANRManager();

    void onResponse(SP<CXDGWMBase> wmBase);
    bool isNotResponding(SP<CXDGWMBase> wmBase);

  private:
    bool                m_active = false;
    SP<CEventLoopTimer> m_timer;

    void                onTick();

    struct SANRData {
        ~SANRData();

        int                         missedResponses = 0;
        std::thread                 dialogThread;
        SP<Hyprutils::OS::CProcess> dialogProc;
        std::atomic<bool>           dialogThreadExited   = false;
        std::atomic<bool>           dialogThreadSaidWait = false;

        void                        runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID);
        bool                        isThreadRunning();
        void                        killDialog() const;
    };

    std::map<WP<CXDGWMBase>, SP<SANRData>> m_data;
};

inline UP<CANRManager> g_pANRManager;