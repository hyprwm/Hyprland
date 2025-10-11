#pragma once

#include <vector>
#include "../../helpers/memory/Memory.hpp"
#include "../../protocols/types/Buffer.hpp"
#include "../../render/Framebuffer.hpp"
#include "../../helpers/time/Timer.hpp"
#include "../eventLoop/EventLoopTimer.hpp"

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
        return formatter<string>::format("error", ctx);
    }
};

using FScreenshareCallback = std::function<void(eScreenshareResult result)>;

class CScreenshareSession {
  public:
    CScreenshareSession(const CScreenshareSession&) = delete;
    CScreenshareSession(CScreenshareSession&&)      = delete;
    ~CScreenshareSession();

    eScreenshareError shareNextFrame(SP<IHLBuffer> buffer, bool overlayCursor, FScreenshareCallback callback);
    void              stop();

    // constraints
    const std::vector<SDRMFormat>& allowedFormats() const;
    Vector2D                       bufferSize() const;
    PHLMONITOR                     monitor() const;
    CBox                           captureBox() const;

    struct {
        CSignalT<> stopped;
        CSignalT<> constraintsChanged;
    } m_events;

  private:
    CScreenshareSession(PHLMONITOR monitor, wl_client* client);
    CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client);
    CScreenshareSession(PHLWINDOW window, wl_client* client);

    WP<CScreenshareSession> m_self;
    bool                    m_stopped = false;

    eScreenshareType        m_type;
    PHLMONITORREF           m_monitor; // monitor should always be valid, if its SHARE_WINDOW we take m_window->m_monitor
    PHLWINDOWREF            m_window;
    CBox                    m_captureBox; // given capture area in logical coordinates (see xdg_output)

    wl_client*              m_client;
    std::string             m_name;

    std::vector<SDRMFormat> m_formats;
    CBox                    m_box; // area of m_monitor to capture

    CFramebuffer            m_tempFB;

    CTimer                  m_lastFrame;
    int                     m_frameCounter = 0;

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
    CScreenshareFrame(WP<CScreenshareSession> session, SP<IHLBuffer> buffer, bool overlayCursor, FScreenshareCallback callback);
    ~CScreenshareFrame();

    bool done() const;
    void share();

  private:
    WP<CScreenshareSession> m_session;
    FScreenshareCallback    m_callback;
    SP<IHLBuffer>           m_buffer;
    bool                    m_shared = false;
    bool                    m_overlayCursor;

    //
    bool copyDmabuf();
    bool copyShm();

    void render();
    void renderMonitor();
    void renderMonitorRegion();
    void renderWindow();

    void storeTempFB();

    friend class CScreenshareManager;
};

class CScreenshareManager {
  public:
    CScreenshareManager();

    UP<CScreenshareSession> newSession(PHLMONITOR monitor, wl_client* client);
    UP<CScreenshareSession> newSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client);
    UP<CScreenshareSession> newSession(PHLWINDOW window, wl_client* client);

    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLMONITOR monitor);
    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLMONITOR monitor, CBox captureBox);
    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLWINDOW window);

    void                    destroyClientSessions(wl_client* client);

    void                    onOutputCommit(PHLMONITOR monitor);

  private:
    std::vector<WP<CScreenshareSession>> m_sessions;
    std::vector<UP<CScreenshareFrame>>   m_frames;

    struct SManagedSession {
        SManagedSession(UP<CScreenshareSession> session);

        UP<CScreenshareSession> m_session;
        CHyprSignalListener     stoppedListener;
        CTimer                  m_lastMeasure;
        int                     m_framesInLastHalfSecond = 0;
        bool                    m_sentScreencast         = false;
    };

    std::vector<UP<SManagedSession>> m_managedSessions;
    WP<CScreenshareSession>          getManagedSession(eScreenshareType type, wl_client* client, PHLMONITOR monitor, PHLWINDOW window, CBox captureBox);

    SP<CEventLoopTimer>              m_tickTimer;
    void                             onTick();

    void                             screenshareStarted(WP<CScreenshareSession> session);

    friend class CScreenshareSession;
};

inline UP<CScreenshareManager> g_pScreenshareManager;
