#pragma once

#include "../defines.hpp"
#include "./types/Buffer.hpp"
#include "wlr-screencopy-unstable-v1.hpp"
#include "WaylandProtocol.hpp"

#include <list>
#include <vector>
#include "../managers/HookSystemManager.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/time/Time.hpp"
#include "../render/Framebuffer.hpp"
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
    wl_client*            client();

    WP<CScreencopyClient> m_self;
    eClientOwners         m_clientOwner = CLIENT_SCREENCOPY;

    CTimer                m_lastFrame;
    int                   m_frameCounter = 0;

  private:
    SP<CZwlrScreencopyManagerV1> m_resource;

    int                          m_framesInLastHalfSecond = 0;
    CTimer                       m_lastMeasure;
    bool                         m_sentScreencast = false;

    SP<HOOK_CALLBACK_FN>         m_tickCallback;
    void                         onTick();

    void                         captureOutput(uint32_t frame, int32_t overlayCursor, wl_resource* output, CBox box);

    friend class CScreencopyProtocol;
};

class CScreencopyFrame {
  public:
    CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource, int32_t overlay_cursor, wl_resource* output, CBox box);

    bool                  good();

    WP<CScreencopyFrame>  m_self;
    WP<CScreencopyClient> m_client;

  private:
    SP<CZwlrScreencopyFrameV1> m_resource;

    PHLMONITORREF              m_monitor;
    bool                       m_overlayCursor = false;
    bool                       m_withDamage    = false;

    CHLBufferReference         m_buffer;
    bool                       m_bufferDMA    = false;
    uint32_t                   m_shmFormat    = 0;
    uint32_t                   m_dmabufFormat = 0;
    int                        m_shmStride    = 0;
    CBox                       m_box          = {};

    // if we have a pending perm, hold the buffer.
    CFramebuffer m_tempFb;

    void         copy(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer);
    void         copyDmabuf(std::function<void(bool)> callback);
    bool         copyShm();
    void         renderMon();
    void         storeTempFB();
    void         share();

    friend class CScreencopyProtocol;
};

class CScreencopyProtocol : public IWaylandProtocol {
  public:
    CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void         destroyResource(CScreencopyClient* resource);
    void         destroyResource(CScreencopyFrame* resource);

    void         onOutputCommit(PHLMONITOR pMonitor);

  private:
    std::vector<SP<CScreencopyFrame>>  m_frames;
    std::vector<WP<CScreencopyFrame>>  m_framesAwaitingWrite;
    std::vector<SP<CScreencopyClient>> m_clients;

    void                               shareAllFrames(PHLMONITOR pMonitor);
    void                               shareFrame(CScreencopyFrame* frame);
    void                               sendFrameDamage(CScreencopyFrame* frame);
    bool                               copyFrameDmabuf(CScreencopyFrame* frame);
    bool                               copyFrameShm(CScreencopyFrame* frame, const Time::steady_tp& now);

    uint32_t                           drmFormatForMonitor(PHLMONITOR pMonitor);

    friend class CScreencopyFrame;
    friend class CScreencopyClient;
};

namespace PROTO {
    inline UP<CScreencopyProtocol> screencopy;
};
