#pragma once

#include "../defines.hpp"
#include "hyprland-toplevel-export-v1.hpp"
#include "WaylandProtocol.hpp"
#include "Screencopy.hpp"

#include <vector>

class CMonitor;
class CWindow;

class CToplevelExportClient {
  public:
    CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_);

    bool                      good();

    WP<CToplevelExportClient> self;
    eClientOwners             clientOwner = CLIENT_TOPLEVEL_EXPORT;

    CTimer                    lastFrame;
    int                       frameCounter = 0;

  private:
    SP<CHyprlandToplevelExportManagerV1> resource;

    int                                  framesInLastHalfSecond = 0;
    CTimer                               lastMeasure;
    bool                                 sentScreencast = false;

    SP<HOOK_CALLBACK_FN>                 tickCallback;
    void                                 onTick();

    void                                 captureToplevel(CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, PHLWINDOW handle);

    friend class CToplevelExportProtocol;
};

class CToplevelExportFrame {
  public:
    CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource_, int32_t overlayCursor, PHLWINDOW pWindow);
    ~CToplevelExportFrame();

    bool                      good();

    WP<CToplevelExportFrame>  self;
    WP<CToplevelExportClient> client;

  private:
    SP<CHyprlandToplevelExportFrameV1> resource;

    PHLWINDOW                          pWindow;
    bool                               overlayCursor   = false;
    bool                               ignoreDamage    = false;
    bool                               lockedSWCursors = false;

    WP<IHLBuffer>                      buffer;
    bool                               bufferDMA    = false;
    uint32_t                           shmFormat    = 0;
    uint32_t                           dmabufFormat = 0;
    int                                shmStride    = 0;
    CBox                               box          = {};

    void                               copy(CHyprlandToplevelExportFrameV1* pFrame, wl_resource* buffer, int32_t ignoreDamage);
    bool                               copyDmabuf(timespec* now);
    bool                               copyShm(timespec* now);
    void                               share();

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
    std::vector<SP<CToplevelExportClient>> m_vClients;
    std::vector<SP<CToplevelExportFrame>>  m_vFrames;
    std::vector<WP<CToplevelExportFrame>>  m_vFramesAwaitingWrite;

    void                                   shareFrame(CToplevelExportFrame* frame);
    bool                                   copyFrameDmabuf(CToplevelExportFrame* frame, timespec* now);
    bool                                   copyFrameShm(CToplevelExportFrame* frame, timespec* now);
    void                                   sendDamage(CToplevelExportFrame* frame);

    friend class CToplevelExportClient;
    friend class CToplevelExportFrame;
};

namespace PROTO {
    inline UP<CToplevelExportProtocol> toplevelExport;
};
