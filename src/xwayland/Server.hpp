#pragma once

#include <array>
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
    bool                            tryOpenSockets();
    void                            runXWayland(int notifyFD);

    pid_t                           serverPID = 0;

    std::string                     displayName;
    int                             display       = -1;
    std::array<int, 2>              xFDs          = {-1, -1};
    std::array<wl_event_source*, 2> xFDReadEvents = {nullptr, nullptr};
    wl_event_source*                idleSource    = nullptr;
    wl_event_source*                pipeSource    = nullptr;
    int                             pipeFd        = -1;
    std::array<int, 2>              xwmFDs        = {-1, -1};
    std::array<int, 2>              waylandFDs    = {-1, -1};

    friend class CXWM;
};
