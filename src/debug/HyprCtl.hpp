#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include <functional>

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

  private:
    void                             startHyprCtlSocket();

    std::vector<SP<SHyprCtlCommand>> m_vCommands;
    wl_event_source*                 m_eventSource = nullptr;
};

inline std::unique_ptr<CHyprCtl> g_pHyprCtl;
