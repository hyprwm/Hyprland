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

    bool                      good();

    WP<CToplevelExportClient> m_self;
    eClientOwners             m_clientOwner = CLIENT_TOPLEVEL_EXPORT;

    CTimer                    m_lastFrame;
    int                       m_frameCounter = 0;

  private:
    SP<CHyprlandToplevelExportManagerV1> m_resource;

    int                                  m_framesInLastHalfSecond = 0;
    CTimer                               m_lastMeasure;
    bool                                 m_sentScreencast = false;

    SP<HOOK_CALLBACK_FN>                 m_tickCallback;
    void                                 onTick();

    void                                 captureToplevel(CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, PHLWINDOW handle);

    friend class CToplevelExportProtocol;
};

class CToplevelExportFrame {
  public:
    CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource_, int32_t overlayCursor, PHLWINDOW pWindow);

    bool                      good();

    WP<CToplevelExportFrame>  m_self;
    WP<CToplevelExportClient> m_client;

  private:
    SP<CHyprlandToplevelExportFrameV1> m_resource;

    PHLWINDOW                          m_window;
    bool                               m_cursorOverlayRequested = false;
    bool                               m_ignoreDamage           = false;

    CHLBufferReference                 m_buffer;
    bool                               m_bufferDMA    = false;
    uint32_t                           m_shmFormat    = 0;
    uint32_t                           m_dmabufFormat = 0;
    int                                m_shmStride    = 0;
    CBox                               m_box          = {};

    void                               copy(CHyprlandToplevelExportFrameV1* pFrame, wl_resource* buffer, int32_t ignoreDamage);
    bool                               copyDmabuf(const Time::steady_tp& now);
    bool                               copyShm(const Time::steady_tp& now);
    void                               share();
    bool                               shouldOverlayCursor() const;

    friend class CToplevelExportProtocol;
};

class CToplevelExportProtocol : IWaylandProtocol {
  public:
    CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void destroyResource(CToplevelExportClient* client);
    void destroyResource(CToplevelExportFrame* frame);

    void onWindowUnmap(PHLWINDOW pWindow);
    void onOutputCommit(PHLMONITOR pMonitor);

  private:
    std::vector<SP<CToplevelExportClient>> m_clients;
    std::vector<SP<CToplevelExportFrame>>  m_frames;
    std::vector<WP<CToplevelExportFrame>>  m_framesAwaitingWrite;

    void                                   shareFrame(CToplevelExportFrame* frame);
    bool                                   copyFrameDmabuf(CToplevelExportFrame* frame, const Time::steady_tp& now);
    bool                                   copyFrameShm(CToplevelExportFrame* frame, const Time::steady_tp& now);
    void                                   sendDamage(CToplevelExportFrame* frame);

    friend class CToplevelExportClient;
    friend class CToplevelExportFrame;
};

namespace PROTO {
    inline UP<CToplevelExportProtocol> toplevelExport;
};
