#pragma once

#include "../defines.hpp"
#include "wlr-screencopy-unstable-v1-protocol.h"

#include <list>
#include <vector>

class CMonitor;

struct SScreencopyClient {
    int          ref      = 0;
    wl_resource* resource = nullptr;

    bool         operator==(const SScreencopyClient& other) const {
        return resource == other.resource;
    }
};

struct SScreencopyFrame {
    wl_resource*       resource = nullptr;
    SScreencopyClient* client   = nullptr;

    uint32_t           shmFormat    = 0;
    uint32_t           dmabufFormat = 0;
    wlr_box            box          = {0};
    int                shmStride    = 0;

    bool               overlayCursor = false;
    bool               withDamage    = false;

    wlr_buffer_cap     bufferCap = WLR_BUFFER_CAP_SHM;

    wlr_buffer*        buffer = nullptr;

    CMonitor*          pMonitor = nullptr;

    bool               operator==(const SScreencopyFrame& other) const {
        return resource == other.resource && client == other.client;
    }
};

class CScreencopyProtocolManager {
  public:
    CScreencopyProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void removeClient(SScreencopyClient* client, bool force = false);
    void removeFrame(SScreencopyFrame* frame, bool force = false);
    void displayDestroy();

    void captureOutput(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* output, wlr_box box = {0, 0, 0, 0});

    void copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer);

    void onRenderEnd(CMonitor* pMonitor);

  private:
    wl_global*                     m_pGlobal = nullptr;
    std::list<SScreencopyFrame>    m_lFrames;
    std::list<SScreencopyClient>   m_lClients;

    wl_listener                    m_liDisplayDestroy;

    std::vector<SScreencopyFrame*> m_vFramesAwaitingWrite;

    void                           shareFrame(SScreencopyFrame* frame);
    void                           sendFrameDamage(SScreencopyFrame* frame);
    bool                           copyFrameDmabuf(SScreencopyFrame* frame);
    bool                           copyFrameShm(SScreencopyFrame* frame, timespec* now);
};