#pragma once

#include "WaylandProtocol.hpp"
#include "wlr-screencopy-unstable-v1.hpp"

#include "../helpers/time/Time.hpp"
#include "./types/Buffer.hpp"
#include <aquamarine/buffer/Buffer.hpp>

#include <vector>

class CMonitor;
class IHLBuffer;
namespace Screenshare {
    class CScreenshareSession;
    class CScreenshareFrame;
};

class CScreencopyClient {
  public:
    CScreencopyClient(SP<CZwlrScreencopyManagerV1> resource_);
    ~CScreencopyClient();

    bool good();

  private:
    SP<CZwlrScreencopyManagerV1> m_resource;
    WP<CScreencopyClient>        m_self;

    wl_client*                   m_savedClient = nullptr;

    void                         captureOutput(uint32_t frame, int32_t overlayCursor, wl_resource* output, CBox box);

    friend class CScreencopyProtocol;
};

class CScreencopyFrame {
  public:
    CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource, WP<Screenshare::CScreenshareSession> session, bool overlayCursor);

    bool good();

  private:
    SP<CZwlrScreencopyFrameV1>           m_resource;
    WP<CScreencopyFrame>                 m_self;
    WP<CScreencopyClient>                m_client;

    WP<Screenshare::CScreenshareSession> m_session;
    UP<Screenshare::CScreenshareFrame>   m_frame;

    CHLBufferReference                   m_buffer;
    Time::steady_tp                      m_timestamp;
    bool                                 m_overlayCursor = true;

    struct {
        CHyprSignalListener stopped;
    } m_listeners;

    void shareFrame(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer, bool withDamage);

    friend class CScreencopyProtocol;
    friend class CScreencopyClient;
};

class CScreencopyProtocol : public IWaylandProtocol {
  public:
    CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void         destroyResource(CScreencopyClient* resource);
    void         destroyResource(CScreencopyFrame* resource);

  private:
    std::vector<SP<CScreencopyFrame>>  m_frames;
    std::vector<SP<CScreencopyClient>> m_clients;

    friend class CScreencopyFrame;
    friend class CScreencopyClient;
};

namespace PROTO {
    inline UP<CScreencopyProtocol> screencopy;
};
