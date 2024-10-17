#pragma once

#include "../defines.hpp"
#include "wlr-screencopy-unstable-v1.hpp"
#include "WaylandProtocol.hpp"

#include <list>
#include <vector>
#include "../managers/HookSystemManager.hpp"
#include "../helpers/Timer.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"
#include <aquamarine/buffer/Buffer.hpp>

class CMonitor;
class IHLBuffer;

enum eClientOwners {
    CLIENT_SCREENCOPY = 0,
    CLIENT_TOPLEVEL_EXPORT
};

class CScreencopyClient {
  public:
    CScreencopyClient(SP<CZwlrScreencopyManagerV1> resource_);
    ~CScreencopyClient();

    bool                  good();

    WP<CScreencopyClient> self;
    eClientOwners         clientOwner = CLIENT_SCREENCOPY;

    CTimer                lastFrame;
    int                   frameCounter = 0;

  private:
    SP<CZwlrScreencopyManagerV1> resource;

    int                          framesInLastHalfSecond = 0;
    CTimer                       lastMeasure;
    bool                         sentScreencast = false;

    SP<HOOK_CALLBACK_FN>         tickCallback;
    void                         onTick();

    void                         captureOutput(uint32_t frame, int32_t overlayCursor, wl_resource* output, CBox box);

    friend class CScreencopyProtocol;
};

class CScreencopyFrame {
  public:
    CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource, int32_t overlay_cursor, wl_resource* output, CBox box);
    ~CScreencopyFrame();

    bool                  good();

    WP<CScreencopyFrame>  self;
    WP<CScreencopyClient> client;

  private:
    SP<CZwlrScreencopyFrameV1> resource;

    CMonitor*                  pMonitor        = nullptr;
    bool                       overlayCursor   = false;
    bool                       withDamage      = false;
    bool                       lockedSWCursors = false;

    WP<IHLBuffer>              buffer;
    bool                       bufferDMA    = false;
    uint32_t                   shmFormat    = 0;
    uint32_t                   dmabufFormat = 0;
    int                        shmStride    = 0;
    CBox                       box          = {};

    void                       copy(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer);
    bool                       copyDmabuf();
    bool                       copyShm();
    void                       share();

    friend class CScreencopyProtocol;
};

class CScreencopyProtocol : public IWaylandProtocol {
  public:
    CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void         destroyResource(CScreencopyClient* resource);
    void         destroyResource(CScreencopyFrame* resource);

    void         onOutputCommit(CMonitor* pMonitor);

  private:
    std::vector<SP<CScreencopyFrame>>  m_vFrames;
    std::vector<WP<CScreencopyFrame>>  m_vFramesAwaitingWrite;
    std::vector<SP<CScreencopyClient>> m_vClients;

    SP<CEventLoopTimer>                m_pSoftwareCursorTimer;
    bool                               m_bTimerArmed = false;

    void                               shareAllFrames(CMonitor* pMonitor);
    void                               shareFrame(CScreencopyFrame* frame);
    void                               sendFrameDamage(CScreencopyFrame* frame);
    bool                               copyFrameDmabuf(CScreencopyFrame* frame);
    bool                               copyFrameShm(CScreencopyFrame* frame, timespec* now);

    friend class CScreencopyFrame;
    friend class CScreencopyClient;
};

namespace PROTO {
    inline UP<CScreencopyProtocol> screencopy;
};
