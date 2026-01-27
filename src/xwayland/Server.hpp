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
    int        ready(int fd, uint32_t mask);

    void       die();

    wl_client* m_xwaylandClient = nullptr;

  private:
    bool                                          tryOpenSockets();
    void                                          runXWayland(Hyprutils::OS::CFileDescriptor& notifyFD);

    std::string                                   m_displayName;
    int                                           m_display = -1;
    std::array<Hyprutils::OS::CFileDescriptor, 2> m_xFDs;
    std::array<wl_event_source*, 2>               m_xFDReadEvents = {nullptr, nullptr};
    wl_event_source*                              m_idleSource    = nullptr;
    wl_event_source*                              m_pipeSource    = nullptr;
    Hyprutils::OS::CFileDescriptor                m_pipeFd;
    std::array<Hyprutils::OS::CFileDescriptor, 2> m_xwmFDs;
    std::array<Hyprutils::OS::CFileDescriptor, 2> m_waylandFDs;

    friend class CXWM;
};
