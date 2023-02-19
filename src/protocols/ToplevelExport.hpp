#pragma once

#include "../defines.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include "hyprland-toplevel-export-v1-protocol.h"

#include <list>
#include <vector>

class CMonitor;
class CWindow;

struct SToplevelClient {
    int          ref      = 0;
    wl_resource* resource = nullptr;

    bool         operator==(const SToplevelClient& other) const {
        return resource == other.resource;
    }
};

struct SToplevelFrame {
    wl_resource*     resource = nullptr;
    SToplevelClient* client   = nullptr;

    uint32_t         shmFormat    = 0;
    uint32_t         dmabufFormat = 0;
    wlr_box          box          = {0};
    int              shmStride    = 0;

    bool             overlayCursor = false;

    wlr_buffer_cap   bufferCap = WLR_BUFFER_CAP_SHM;

    wlr_buffer*      buffer = nullptr;

    CWindow*         pWindow = nullptr;

    bool             operator==(const SToplevelFrame& other) const {
        return resource == other.resource && client == other.client;
    }
};

class CToplevelExportProtocolManager {
  public:
    CToplevelExportProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void captureToplevel(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, CWindow* handle);
    void removeClient(SToplevelClient* client, bool force = false);
    void removeFrame(SToplevelFrame* frame, bool force = false);
    void copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer, int32_t ignore_damage);
    void displayDestroy();
    void onWindowUnmap(CWindow* pWindow);

  private:
    wl_global*                   m_pGlobal = nullptr;
    std::list<SToplevelFrame>    m_lFrames;
    std::list<SToplevelClient>   m_lClients;

    wl_listener                  m_liDisplayDestroy;

    std::vector<SToplevelFrame*> m_vFramesAwaitingWrite;

    void                         shareFrame(SToplevelFrame* frame);
    bool                         copyFrameDmabuf(SToplevelFrame* frame);
    bool                         copyFrameShm(SToplevelFrame* frame, timespec* now);

    void                         onMonitorRender(CMonitor* pMonitor);
};