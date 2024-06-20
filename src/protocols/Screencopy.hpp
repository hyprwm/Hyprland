#pragma once

#include "../defines.hpp"
#include "wlr-screencopy-unstable-v1-protocol.h"

#include <list>
#include <vector>
#include "../managers/HookSystemManager.hpp"
#include "../helpers/Timer.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

class CMonitor;
class IHLBuffer;

enum eClientOwners {
    CLIENT_SCREENCOPY = 0,
    CLIENT_TOPLEVEL_EXPORT
};

class CScreencopyClient {
  public:
    CScreencopyClient();
    ~CScreencopyClient();

    int                  ref      = 0;
    wl_resource*         resource = nullptr;

    eClientOwners        clientOwner = CLIENT_SCREENCOPY;

    int                  frameCounter           = 0;
    int                  framesInLastHalfSecond = 0;
    CTimer               lastMeasure;
    CTimer               lastFrame;
    bool                 sentScreencast = false;

    void                 onTick();
    SP<HOOK_CALLBACK_FN> tickCallback;

    bool                 operator==(const CScreencopyClient& other) const {
        return resource == other.resource;
    }
};

struct SScreencopyFrame {
    wl_resource*       resource = nullptr;
    CScreencopyClient* client   = nullptr;

    uint32_t           shmFormat    = 0;
    uint32_t           dmabufFormat = 0;
    CBox               box          = {};
    int                shmStride    = 0;

    bool               overlayCursor   = false;
    bool               withDamage      = false;
    bool               lockedSWCursors = false;

    bool               bufferDMA = false;

    WP<IHLBuffer>      buffer;

    CMonitor*          pMonitor = nullptr;
    PHLWINDOWREF       pWindow;

    bool               operator==(const SScreencopyFrame& other) const {
        return resource == other.resource && client == other.client;
    }
};

class CScreencopyProtocolManager {
  public:
    CScreencopyProtocolManager();

    void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void removeClient(CScreencopyClient* client, bool force = false);
    void removeFrame(SScreencopyFrame* frame, bool force = false);
    void displayDestroy();

    void captureOutput(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* output, CBox box = {0, 0, 0, 0});

    void copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer);

    void onOutputCommit(CMonitor* pMonitor);

  private:
    wl_global*                     m_pGlobal = nullptr;
    std::list<SScreencopyFrame>    m_lFrames;
    std::list<CScreencopyClient>   m_lClients;

    SP<CEventLoopTimer>            m_pSoftwareCursorTimer;
    bool                           m_bTimerArmed = false;

    wl_listener                    m_liDisplayDestroy;

    std::vector<SScreencopyFrame*> m_vFramesAwaitingWrite;

    SP<Aquamarine::IBuffer>        m_pLastMonitorBackBuffer;

    void                           shareAllFrames(CMonitor* pMonitor);
    void                           shareFrame(SScreencopyFrame* frame);
    void                           sendFrameDamage(SScreencopyFrame* frame);
    bool                           copyFrameDmabuf(SScreencopyFrame* frame);
    bool                           copyFrameShm(SScreencopyFrame* frame, timespec* now);

    friend class CScreencopyClient;
};