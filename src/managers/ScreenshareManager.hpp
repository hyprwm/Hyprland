#pragma once

#include <vector>
#include "../helpers/memory/Memory.hpp"
#include "../protocols/types/Buffer.hpp"
#include "../render/Framebuffer.hpp"

// TODO: better/move screencast event here, see CScreencopyClient::onTick()
// TODO: integrate into screencopy, toplevel export, and image copy capture
// TODO: maybe keep track of sessions/clients

using FScreenshareCallback = std::function<void(bool success)>;

struct SScreenshareFrame {
    SScreenshareFrame(PHLMONITOR monitor, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback);
    SScreenshareFrame(PHLWINDOW window, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback);

    PHLMONITORREF        m_monitor;
    PHLWINDOWREF         m_window;
    CHLBufferReference   m_buffer;
    CBox                 m_box;

    wl_client*           m_client;
    bool                 m_overlayCursor;
    FScreenshareCallback m_callback;

    CFramebuffer         m_tempFB;
    void                 storeTempFB();

    void                 render();
    void                 renderMonitor();
    void                 renderWindow();

    bool                 m_shared = false;
    void                 share();
    bool                 copyDmabuf();
    bool                 copyShm();
};

class CScreenshareManager {
  public:
    CScreenshareManager();

    void shareNextFrame(PHLMONITOR monitor, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback = nullptr);
    void shareNextFrame(PHLWINDOW monitor, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback = nullptr);
    void onOutputCommit(PHLMONITOR monitor);

  private:
    std::vector<SScreenshareFrame> m_frames;

    friend struct SScreenshareFrame;
};

inline UP<CScreenshareManager> g_pScreenshareManager;
