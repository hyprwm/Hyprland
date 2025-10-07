#pragma once

#include "../defines.hpp"
#include "hyprland-toplevel-export-v1.hpp"
#include "WaylandProtocol.hpp"
#include "Screencopy.hpp"
#include "../helpers/time/Time.hpp"

#include <vector>

class CMonitor;
class CWindow;

class CToplevelExportClient {
  public:
    CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_);
    ~CToplevelExportClient();

    bool good();

  private:
    SP<CHyprlandToplevelExportManagerV1> m_resource;
    WP<CToplevelExportClient>            m_self;

    void                                 captureToplevel(uint32_t frame, int32_t overlayCursor, PHLWINDOW handle);

    friend class CToplevelExportProtocol;
};

class CToplevelExportFrame {
  public:
    CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource, WP<CScreenshareSession> session, bool overlayCursor);

    bool good();

  private:
    SP<CHyprlandToplevelExportFrameV1> m_resource;
    WP<CToplevelExportFrame>           m_self;
    WP<CScreenshareSession>            m_session;
    WP<CToplevelExportClient>          m_client;

    CHLBufferReference                 m_buffer;
    Time::steady_tp                    m_timestamp;
    bool                               m_overlayCursorRequested = false;

    struct {
        CHyprSignalListener stopped;
    } m_listeners;

    void shareFrame(wl_resource* buffer, bool ignoreDamage);

    friend class CToplevelExportProtocol;
    friend class CToplevelExportClient;
};

class CToplevelExportProtocol : IWaylandProtocol {
  public:
    CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void destroyResource(CToplevelExportClient* client);
    void destroyResource(CToplevelExportFrame* frame);

    void onOutputCommit(PHLMONITOR pMonitor);

  private:
    std::vector<SP<CToplevelExportClient>> m_clients;
    std::vector<SP<CToplevelExportFrame>>  m_frames;

    friend class CToplevelExportClient;
    friend class CToplevelExportFrame;
};

namespace PROTO {
    inline UP<CToplevelExportProtocol> toplevelExport;
};
