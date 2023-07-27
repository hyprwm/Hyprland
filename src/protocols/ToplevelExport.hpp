#pragma once

#include "../defines.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1-protocol.h"
#include "hyprland-toplevel-export-v1-protocol.h"
#include "Screencopy.hpp"

#include <list>
#include <vector>

class CMonitor;
class CWindow;

class CToplevelExportProtocolManager {
  public:
    CToplevelExportProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void captureToplevel(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, CWindow* handle);
    void removeClient(CScreencopyClient* client, bool force = false);
    void removeFrame(SScreencopyFrame* frame, bool force = false);
    void copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer, int32_t ignore_damage);
    void displayDestroy();
    void onWindowUnmap(CWindow* pWindow);
    void onOutputCommit(CMonitor* pMonitor, wlr_output_event_commit* e);

  private:
    wl_global*                     m_pGlobal = nullptr;
    std::list<SScreencopyFrame>    m_lFrames;
    std::list<CScreencopyClient>   m_lClients;

    wl_listener                    m_liDisplayDestroy;

    std::vector<SScreencopyFrame*> m_vFramesAwaitingWrite;

    void                           shareFrame(SScreencopyFrame* frame);
    bool                           copyFrameDmabuf(SScreencopyFrame* frame, timespec* now);
    bool                           copyFrameShm(SScreencopyFrame* frame, timespec* now);
    void                           sendDamage(SScreencopyFrame* frame);

    friend class CScreencopyClient;
};