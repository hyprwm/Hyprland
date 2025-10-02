#pragma once

#include <vector>
#include "../../helpers/memory/Memory.hpp"
#include "../../protocols/types/Buffer.hpp"
#include "../../render/Framebuffer.hpp"

// TODO: better/move screencast event here, see CScreencopyClient::onTick()
// TODO: integrate into screencopy, toplevel export, and image copy capture
// TODO: do screenshare damage
// TODO: verify that transforms are correct
// TODO: format bit flip for BGRA?

using FScreenshareCallback = std::function<void(bool success)>;

enum eScreenshareType : uint8_t {
    SHARE_MONITOR,
    SHARE_WINDOW,
    SHARE_REGION
};

class CScreenshareSession {
  public:
    ~CScreenshareSession();

    void stop();
    bool shareNextFrame(SP<IHLBuffer> buffer, FScreenshareCallback callback);

    //
    std::vector<uint32_t> allowedFormats();
    Vector2D              getBufferSize();

    struct {
        CSignalT<> stopped;
    } m_events;

  private:
    CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor);
    CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor);
    CScreenshareSession(PHLWINDOW window, wl_client* client, bool overlayCursor);

    eScreenshareType m_type;
    PHLMONITORREF    m_monitor;
    PHLWINDOWREF     m_window;
    CBox             m_captureRegion;

    wl_client*       m_client;
    bool             m_overlayCursor;
    CBox             m_box;

    CFramebuffer     m_tempFB;

    struct {
        CHyprSignalListener sourceDestroyed;
    } m_listeners;

    WP<CScreenshareSession> m_self;
    bool                    m_stopped = false;

    friend class CScreenshareFrame;
    friend class CScreenshareManager;
};

class CScreenshareFrame {
  public:
    CScreenshareFrame(WP<CScreenshareSession> session, SP<IHLBuffer> buffer, FScreenshareCallback callback);
    ~CScreenshareFrame();

    bool done() const;
    void share();

  private:
    WP<CScreenshareSession> m_session;
    FScreenshareCallback    m_callback;
    SP<IHLBuffer>           m_buffer;
    bool                    m_shared = false;

    //
    bool copyDmabuf();
    bool copyShm();

    void render();
    void renderMonitor();
    void renderMonitorRegion();
    void renderWindow();

    void storeTempFB();
};

class CScreenshareManager {
  public:
    CScreenshareManager();

    UP<CScreenshareSession> newSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor);
    UP<CScreenshareSession> newSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor);
    UP<CScreenshareSession> newSession(PHLWINDOW window, wl_client* client, bool overlayCursor);

    void                    onOutputCommit(PHLMONITOR monitor);

  private:
    std::vector<WP<CScreenshareSession>> m_sessions;
    std::vector<CScreenshareFrame>       m_frames;

    friend class CScreenshareSession;
};

inline UP<CScreenshareManager> g_pScreenshareManager;
