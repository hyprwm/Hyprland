#pragma once

#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/defer/Promise.hpp"
#include "../desktop/view/Window.hpp"
#include "../desktop/DesktopTypes.hpp"
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
        bool                      all              = false;
        bool                      sysInfoConfig    = false;
        bool                      isDynamicKeyword = false;
        pid_t                     pid              = 0;
        SP<CPromise<std::string>> pendingPromise;
    } m_currentRequestParams;

    static std::string getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format);
    static std::string getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format);
    static std::string getSolitaryBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format);
    static std::string getDSBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format);
    static std::string getTearingBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format);
    static std::string getMonitorData(PHLMONITOR m, eHyprCtlOutputFormat format);

  private:
    void startHyprCtlSocket();

    struct SHyprCtlClient {
        Hyprutils::OS::CFileDescriptor fd;
        wl_event_source*               eventSource     = nullptr;
        std::string                    request         = "";
        std::string                    reply           = "";
        size_t                         replyWritten    = 0;
        pid_t                          pid             = 0;
        bool                           followRolling   = false;
        bool                           waitingForReply = false;
        bool                           closed          = false;
    };

    static int                       onSocketEvent(int fd, uint32_t mask, void* data);
    static int                       onClientEvent(int fd, uint32_t mask, void* data);

    void                             acceptClient();
    void                             onClientEvent(SHyprCtlClient* client, uint32_t mask);
    void                             readClient(const SP<SHyprCtlClient>& client);
    void                             processClientRequest(const SP<SHyprCtlClient>& client);
    void                             queueClientReply(const SP<SHyprCtlClient>& client, std::string&& reply);
    void                             writeClientReply(const SP<SHyprCtlClient>& client);
    void                             removeClient(SHyprCtlClient* client);
    SP<SHyprCtlClient>               clientFromPtr(SHyprCtlClient* client);

    std::vector<SP<SHyprCtlCommand>> m_commands;
    wl_event_source*                 m_eventSource = nullptr;
    std::string                      m_socketPath;
    std::vector<SP<SHyprCtlClient>>  m_clients;
};

inline UP<CHyprCtl> g_pHyprCtl;
