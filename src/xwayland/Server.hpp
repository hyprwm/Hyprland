#pragma once

#include <array>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../helpers/signal/Signal.hpp"

struct wl_event_source;
struct wl_client;

// TODO: add lazy mode
class CXWaylandServer {
  public:
    CXWaylandServer();
    ~CXWaylandServer();

    // create the server.
    bool create();

    // starts the server, meant to be called by CXWaylandServer.
    bool start();

    // called on ready
    int  ready(int fd, uint32_t mask);

    void die();

    struct {
        CSignal ready;
    } events;

    wl_client* xwaylandClient = nullptr;

  private:
    bool                                          tryOpenSockets();
    void                                          runXWayland(Hyprutils::OS::CFileDescriptor& notifyFD);

    pid_t                                         serverPID = 0;

    std::string                                   displayName;
    int                                           display = -1;
    std::array<Hyprutils::OS::CFileDescriptor, 2> xFDs;
    std::array<wl_event_source*, 2>               xFDReadEvents = {nullptr, nullptr};
    wl_event_source*                              idleSource    = nullptr;
    wl_event_source*                              pipeSource    = nullptr;
    Hyprutils::OS::CFileDescriptor                pipeFd;
    std::array<Hyprutils::OS::CFileDescriptor, 2> xwmFDs;
    std::array<Hyprutils::OS::CFileDescriptor, 2> waylandFDs;

    friend class CXWM;
};
