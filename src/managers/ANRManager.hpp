#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "./eventLoop/EventLoopTimer.hpp"
#include "../helpers/signal/Signal.hpp"
#include <atomic>
#include <thread>
#include <vector>

class CXDGWMBase;
class CXWaylandSurface;

class CANRManager {
  public:
    CANRManager();

    void onResponse(SP<CXDGWMBase> wmBase);
    void onResponse(SP<CXWaylandSurface> xwaylandSurface);
    bool isNotResponding(PHLWINDOW pWindow);

  private:
    bool                m_active = false;
    SP<CEventLoopTimer> m_timer;

    void                onTick();

    struct SANRData {
        SANRData(PHLWINDOW pWindow);
        ~SANRData();

        WP<CXWaylandSurface>        xwaylandSurface;
        WP<CXDGWMBase>              xdgBase;

        int                         missedResponses = 0;
        std::thread                 dialogThread;
        SP<Hyprutils::OS::CProcess> dialogProc;
        std::atomic<bool>           dialogThreadExited   = false;
        std::atomic<bool>           dialogThreadSaidWait = false;

        void                        runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID);
        bool                        isThreadRunning();
        void                        killDialog() const;
        bool                        isDefunct() const;
        bool                        fitsWindow(PHLWINDOW pWindow) const;
        pid_t                       getPid() const;
        void                        ping();
    };

    void                      onResponse(SP<SANRData> data);
    bool                      isNotResponding(SP<SANRData> data);
    SP<SANRData>              dataFor(PHLWINDOW pWindow);
    SP<SANRData>              dataFor(SP<CXDGWMBase> wmBase);
    SP<SANRData>              dataFor(SP<CXWaylandSurface> pXwaylandSurface);

    std::vector<SP<SANRData>> m_data;
};

inline UP<CANRManager> g_pANRManager;