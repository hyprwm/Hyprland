#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include <functional>

// exposed for main.cpp
std::string systemInfoRequest(eHyprCtlOutputFormat format, std::string request);

class CHyprCtl {
  public:
    CHyprCtl();
    ~CHyprCtl();

    std::string         makeDynamicCall(const std::string& input);
    SP<SHyprCtlCommand> registerCommand(SHyprCtlCommand cmd);
    void                unregisterCommand(const SP<SHyprCtlCommand>& cmd);
    std::string         getReply(std::string);

    int                 m_iSocketFD = -1;

    struct {
        bool all           = false;
        bool sysInfoConfig = false;
    } m_sCurrentRequestParams;

    static std::string getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format);
    static std::string getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format);
    static std::string getMonitorData(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format);

  private:
    void                             startHyprCtlSocket();

    std::vector<SP<SHyprCtlCommand>> m_vCommands;
    wl_event_source*                 m_eventSource = nullptr;
    std::string                      m_socketPath;
};

inline std::unique_ptr<CHyprCtl> g_pHyprCtl;
