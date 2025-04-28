#pragma once

#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include "../desktop/Window.hpp"
#include <functional>
#include <sys/types.h>
#include <hyprutils/os/FileDescriptor.hpp>

// exposed for main.cpp
std::string systemInfoRequest(eHyprCtlOutputFormat format, std::string request);
std::string versionRequest(eHyprCtlOutputFormat format, std::string request);

class CHyprCtl {
  public:
    CHyprCtl();
    ~CHyprCtl();

    std::string                    makeDynamicCall(const std::string& input);
    SP<SHyprCtlCommand>            registerCommand(SHyprCtlCommand cmd);
    void                           unregisterCommand(const SP<SHyprCtlCommand>& cmd);
    std::string                    getReply(std::string);

    Hyprutils::OS::CFileDescriptor m_socketFD;

    struct {
        bool  all           = false;
        bool  sysInfoConfig = false;
        pid_t pid           = 0;
    } m_currentRequestParams;

    static std::string getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format);
    static std::string getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format);
    static std::string getMonitorData(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format);

  private:
    void                             startHyprCtlSocket();

    std::vector<SP<SHyprCtlCommand>> m_commands;
    wl_event_source*                 m_eventSource = nullptr;
    std::string                      m_socketPath;
};

inline UP<CHyprCtl> g_pHyprCtl;
