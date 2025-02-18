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
class CXWaylandSurface;

class CANRManager {
  public:
    CANRManager();

    void                onResponse(SP<CXDGWMBase> wmBase);
    bool                isNotResponding(SP<CXDGWMBase> wmBase);

    void                onXWaylandResponse(SP<CXWaylandSurface> surf);
    bool                isXWaylandNotResponding(SP<CXWaylandSurface> surf);

    bool                m_active = false;
    SP<CEventLoopTimer> m_timer;

    void                onTick();

    struct SANRData {
        ~SANRData();

        int                         missedResponses      = 0;
        bool                        dialogThreadExited   = true;
        bool                        dialogThreadSaidWait = false;
        std::thread                 dialogThread;
        SP<Hyprutils::OS::CProcess> dialogProc;

        void                        runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID);
        bool                        isThreadRunning();
        void                        killDialog() const;
    };

    std::map<WP<CXDGWMBase>, SP<SANRData>>       m_data;
    std::map<WP<CXWaylandSurface>, SP<SANRData>> m_xwaylandData;

  private:
    std::pair<PHLWINDOW, int> findFirstWindowAndCount(const WP<CXDGWMBase>& wmBase);
    std::pair<PHLWINDOW, int> findFirstXWaylandWindowAndCount(const WP<CXWaylandSurface>& surf);
    void                      handleDialog(SP<SANRData>& data, PHLWINDOW firstWindow, pid_t pid, const WP<CXDGWMBase>& wmBase);
    void                      handleXWaylandDialog(SP<SANRData>& data, PHLWINDOW firstWindow, const WP<CXWaylandSurface>& surf);

    template <typename T>
    void setWindowTint(const T& owner, float tint);
};

inline UP<CANRManager> g_pANRManager;