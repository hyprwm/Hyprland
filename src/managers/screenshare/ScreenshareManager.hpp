#pragma once

#include <vector>
#include "../../helpers/memory/Memory.hpp"
#include "../../protocols/types/Buffer.hpp"
#include "../../render/Framebuffer.hpp"

// TODO: integrate into screencopy, toplevel export, and image copy capture
// TODO: do screenshare damage
// TODO: verify that transforms are correct
// TODO: format bit flip for BGRA?

enum eScreenshareType : uint8_t {
    SHARE_MONITOR,
    SHARE_WINDOW,
    SHARE_REGION
};

enum eScreenshareError : uint8_t {
    ERROR_NONE,
    ERROR_STOPPED,
    ERROR_MONITOR,
    ERROR_WINDOW,
    ERROR_BUFFER,
    ERROR_BUFFER_SIZE,
    ERROR_BUFFER_FORMAT
};

enum eScreenshareResult : uint8_t {
    RESULT_SHARED,
    RESULT_NOT_SHARED,
    RESULT_TIMESTAMP,
};

template <>
struct std::formatter<eScreenshareType> : std::formatter<std::string> {
    auto format(const eScreenshareType& res, std::format_context& ctx) const {
        switch (res) {
            case SHARE_MONITOR: return formatter<string>::format("monitor", ctx);
            case SHARE_WINDOW: return formatter<string>::format("window", ctx);
            case SHARE_REGION: return formatter<string>::format("region", ctx);
        }
    }
};

using FScreenshareCallback = std::function<void(eScreenshareResult result)>;

class CScreenshareSession {
  public:
    CScreenshareSession(const CScreenshareSession&) = delete;
    CScreenshareSession(CScreenshareSession&&)      = delete;
    ~CScreenshareSession();

    eScreenshareError shareNextFrame(SP<IHLBuffer> buffer, FScreenshareCallback callback);
    void              stop();

    // constraints
    const std::vector<SDRMFormat>& allowedFormats();
    Vector2D                       getBufferSize();

    struct {
        CSignalT<> stopped;
        CSignalT<> constraintsChanged;
    } m_events;

  private:
    CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool overlayCursor);
    CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool overlayCursor);
    CScreenshareSession(PHLWINDOW window, wl_client* client, bool overlayCursor);

    WP<CScreenshareSession> m_self;
    bool                    m_stopped = false;

    eScreenshareType        m_type;
    PHLMONITORREF           m_monitor; // monitor should always be valid, if its SHARE_WINDOW we take m_window->m_monitor
    PHLWINDOWREF            m_window;
    CBox                    m_captureRegion; // given capture area in logical coordinates (see xdg_output)

    wl_client*              m_client;
    std::string             m_name;

    bool                    m_overlayCursor;

    std::vector<SDRMFormat> m_formats;
    CBox                    m_box; // area of m_monitor to capture

    CFramebuffer            m_tempFB;

    struct {
        CHyprSignalListener monitorDestroyed;
        CHyprSignalListener monitorModeChanged;
        CHyprSignalListener windowDestroyed;
        CHyprSignalListener windowSizeChanged;
    } m_listeners;

    void calculateConstraints();
    void init();

    friend class CScreenshareFrame;
    friend class CScreenshareManager;
};

class CScreenshareFrame {
  public:
    CScreenshareFrame(const CScreenshareFrame&) = delete;
    CScreenshareFrame(CScreenshareFrame&&)      = delete;
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
    std::vector<UP<CScreenshareFrame>>   m_frames;

    friend class CScreenshareSession;
};

inline UP<CScreenshareManager> g_pScreenshareManager;
