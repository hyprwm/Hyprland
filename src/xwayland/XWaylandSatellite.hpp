#pragma once

#ifdef USE_XWAYLAND_SATELLITE

#include <array>
#include <string>
#include <thread>
#include <hyprutils/os/FileDescriptor.hpp>

struct wl_event_source;
struct wl_event_loop;

class CXWaylandSatellite {
  public:
    CXWaylandSatellite();
    ~CXWaylandSatellite();

    // Initialize: create sockets, probe binary, set DISPLAY
    bool setup(wl_event_loop* eventLoop);

    // Register FD watchers for on-demand spawn
    void               setupWatch();

    bool               enabled() const;
    const std::string& displayName() const;

  private:
    bool        tryOpenSockets();
    bool        testOnDemand();
    void        spawn();
    void        clearPendingConnections(Hyprutils::OS::CFileDescriptor& fd);
    void        removeWatches();

    static int  onSocketActivity(int fd, uint32_t mask, void* data);

    std::string m_displayName;
    int         m_display = -1;

    // X11 sockets: [0] = abstract or first regular, [1] = regular
    std::array<Hyprutils::OS::CFileDescriptor, 2> m_xFDs;

    wl_event_source*                              m_abstractWatch = nullptr;
    wl_event_source*                              m_unixWatch     = nullptr;
    wl_event_loop*                                m_eventLoop     = nullptr;
    bool                                          m_enabled       = false;

    // Lock file path for cleanup
    std::string m_lockPath;
    std::string m_socketPathRegular;
    std::string m_socketPathAbstract; // only used on Linux with abstract sockets
};

#endif // USE_XWAYLAND_SATELLITE
