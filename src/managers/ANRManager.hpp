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

        int                         missedResponses = 0;
        std::thread                 dialogThread;
        SP<Hyprutils::OS::CProcess> dialogProc;
        std::atomic<bool>           dialogThreadExited   = true;
        std::atomic<bool>           dialogThreadSaidWait = false;

        void                        runDialog(const std::string& title, const std::string& appName, const std::string appClass, pid_t dialogWmPID);
        bool                        isThreadRunning();
        void                        killDialog() const;
    };

  private:
    template <typename T>
    std::pair<PHLWINDOW, int> findFirstWindowAndCount(const WP<T>& owner);

    template <typename T>
    void handleANRDialog(SP<SANRData>& data, PHLWINDOW firstWindow, const WP<T>& owner);

    template <typename T>
    void setWindowTint(const T& owner, float tint);

    template <typename T>
    void handleANRData(std::map<WP<T>, SP<SANRData>>& dataMap);

    template <typename T>
    void handleResponse(std::map<WP<T>, SP<SANRData>>& dataMap, SP<T> owner);

    template <typename T>
    bool isNotRespondingImpl(const std::map<WP<T>, SP<SANRData>>& dataMap, SP<T> owner);

    template <typename T>
    bool isMatchingWindow(const PHLWINDOW& window, const T& owner);

    template <typename T>
    bool                                         hasWindowWithSameOwner(const PHLWINDOW& window);

    void                                         sendXWaylandPing(const WP<CXWaylandSurface>& surf);

    std::map<WP<CXDGWMBase>, SP<SANRData>>       m_data;
    std::map<WP<CXWaylandSurface>, SP<SANRData>> m_xwaylandData;

    static constexpr float                       NOT_RESPONDING_TINT = 0.2F;
};

inline UP<CANRManager> g_pANRManager;