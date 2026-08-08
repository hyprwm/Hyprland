#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "./eventLoop/EventLoopTimer.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/AsyncDialogBox.hpp"
#include "../desktop/view/window/WindowBackend.hpp"
#include <vector>

class CANRManager {
  public:
    CANRManager();

    void onResponse(Desktop::View::SBackendClientID clientID);
    bool isNotResponding(PHLWINDOW pWindow);

  private:
    bool                m_active = false;
    SP<CEventLoopTimer> m_timer;

    void                onTick();

    struct SANRData {
        SANRData(PHLWINDOW pWindow);
        ~SANRData();

        struct SWindowData {
            PHLWINDOWREF        window;
            CHyprSignalListener pong;
            CHyprSignalListener destroy;
        };

        Desktop::View::SBackendClientID clientID;
        pid_t                           pid = 0;
        std::vector<SWindowData>        windows;
        int                             missedResponses = 0;

        bool                            dialogSaidWait = false;
        SP<CAsyncDialogBox>             dialogBox;

        void                            runDialog(const std::string& appName, const std::string appClass, pid_t dialogWmPID);
        bool                            isRunning();
        void                            killDialog();
        bool                            fitsWindow(PHLWINDOW pWindow) const;
        void                            ping();
    };

    void                      onResponse(SP<SANRData> data);
    bool                      isNotResponding(SP<SANRData> data);
    SP<SANRData>              dataFor(PHLWINDOW pWindow);
    SP<SANRData>              dataFor(Desktop::View::SBackendClientID clientID);

    std::vector<SP<SANRData>> m_data;
};

inline UP<CANRManager> g_pANRManager;
