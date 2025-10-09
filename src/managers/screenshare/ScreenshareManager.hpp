#pragma once

#include <vector>
#include "../../helpers/memory/Memory.hpp"
#include "../../protocols/types/Buffer.hpp"
#include "../../render/Framebuffer.hpp"
#include "../../helpers/time/Timer.hpp"
#include "../eventLoop/EventLoopTimer.hpp"

// TODO: do screenshare damage

enum eScreenshareType : uint8_t {
    SHARE_MONITOR,
    SHARE_WINDOW,
    SHARE_REGION
};

enum eScreenshareError : uint8_t {
    ERROR_NONE,
    ERROR_UNKNOWN,
    ERROR_STOPPED,
    ERROR_NO_BUFFER,
    ERROR_BUFFER_SIZE,
    ERROR_BUFFER_FORMAT
};

enum eScreenshareResult : uint8_t {
    RESULT_COPIED,
    RESULT_NOT_COPIED,
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

class CScreenshareFrame;

class CScreenshareSession {
  public:
    CScreenshareSession(const CScreenshareSession&) = delete;
    CScreenshareSession(CScreenshareSession&&)      = delete;
    ~CScreenshareSession();

    UP<CScreenshareFrame> nextFrame(bool overlayCursor);
    void                  stop();

    // constraints
    const std::vector<DRMFormat>& allowedFormats() const;
    Vector2D                      bufferSize() const;
    PHLMONITOR                    monitor() const; // this will return the correct monitor based on type

    struct {
        CSignalT<> stopped;
        CSignalT<> constraintsChanged;
    } m_events;

  private:
    CScreenshareSession(PHLMONITOR monitor, wl_client* client, bool managed);
    CScreenshareSession(PHLMONITOR monitor, CBox captureRegion, wl_client* client, bool managed);
    CScreenshareSession(PHLWINDOW window, wl_client* client, bool managed);

    WP<CScreenshareSession> m_self;
    bool                    m_stopped = false;
    bool                    m_managed = false;

    eScreenshareType        m_type;
    PHLMONITORREF           m_monitor;
    PHLWINDOWREF            m_window;
    CBox                    m_captureBox; // given capture area in logical coordinates (see xdg_output)

    wl_client*              m_client;
    std::string             m_name;

    std::vector<DRMFormat>  m_formats;
    Vector2D                m_bufferSize;

    CFramebuffer            m_tempFB;

    CTimer                  m_lastFrame;
    int                     m_frameCounter           = 0;
    int                     m_framesInLastHalfSecond = 0;

    struct {
        CHyprSignalListener monitorDestroyed;
        CHyprSignalListener monitorModeChanged;
        CHyprSignalListener windowDestroyed;
        CHyprSignalListener windowSizeChanged;
        CHyprSignalListener windowMonitorChanged;
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
    CScreenshareFrame(WP<CScreenshareSession> session, bool overlayCursor, bool isFirst);
    ~CScreenshareFrame();

    bool                done() const;
    eScreenshareError   share(SP<IHLBuffer> buffer, const CRegion& damage, FScreenshareCallback callback);

    Vector2D            bufferSize() const;
    wl_output_transform transform() const; // returns the transform applied by compositor on the buffer
    const CRegion&      damage() const;

  private:
    WP<CScreenshareFrame>   m_self;
    WP<CScreenshareSession> m_session;
    FScreenshareCallback    m_callback;
    SP<IHLBuffer>           m_buffer;
    Vector2D                m_bufferSize;
    CRegion                 m_damage; // damage in buffer coords
    bool                    m_shared = false, m_copied = false, m_failed = false;
    bool                    m_overlayCursor;
    bool                    m_isFirst = false;

    //
    void copy();
    bool copyDmabuf();
    bool copyShm();

    void render();
    void renderMonitor();
    void renderMonitorRegion();
    void renderWindow();

    void storeTempFB();

    friend class CScreenshareManager;
    friend class CScreenshareSession;
};

class CScreenshareManager {
  public:
    CScreenshareManager();

    UP<CScreenshareSession> newSession(wl_client* client, PHLMONITOR monitor);
    UP<CScreenshareSession> newSession(wl_client* client, PHLMONITOR monitor, CBox captureRegion);
    UP<CScreenshareSession> newSession(wl_client* client, PHLWINDOW window);

    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLMONITOR monitor);
    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLMONITOR monitor, CBox captureBox);
    WP<CScreenshareSession> getManagedSession(wl_client* client, PHLWINDOW window);

    void                    destroyClientSessions(wl_client* client);

    void                    onOutputCommit(PHLMONITOR monitor);

  private:
    std::vector<WP<CScreenshareSession>> m_sessions;
    std::vector<WP<CScreenshareFrame>>   m_frames;

    struct SManagedSession {
        SManagedSession(UP<CScreenshareSession> session);

        UP<CScreenshareSession> m_session;
        CHyprSignalListener     stoppedListener;
        CTimer                  m_lastMeasure;
        bool                    m_sentScreencast = false;
    };

    std::vector<UP<SManagedSession>> m_managedSessions;
    WP<CScreenshareSession>          getManagedSession(eScreenshareType type, wl_client* client, PHLMONITOR monitor, PHLWINDOW window, CBox captureBox);

    SP<CEventLoopTimer>              m_tickTimer;
    void                             onTick();

    void                             screenshareEvents(WP<CScreenshareSession> session, bool started);

    friend class CScreenshareSession;
};

inline UP<CScreenshareManager> g_pScreenshareManager;
