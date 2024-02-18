#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../helpers/Workspace.hpp"
#include "../Window.hpp"
#include "OpenGL.hpp"
#include "Renderbuffer.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/Region.hpp"

struct SMonitorRule;

// TODO: add fuller damage tracking for updating only parts of a window
enum DAMAGETRACKINGMODES {
    DAMAGE_TRACKING_INVALID = -1,
    DAMAGE_TRACKING_NONE    = 0,
    DAMAGE_TRACKING_MONITOR,
    DAMAGE_TRACKING_FULL
};

enum eRenderPassMode {
    RENDER_PASS_ALL = 0,
    RENDER_PASS_MAIN,
    RENDER_PASS_POPUP
};

enum eRenderMode {
    RENDER_MODE_NORMAL              = 0,
    RENDER_MODE_FULL_FAKE           = 1,
    RENDER_MODE_TO_BUFFER           = 2,
    RENDER_MODE_TO_BUFFER_READ_ONLY = 3,
};

class CToplevelExportProtocolManager;
class CInputManager;
struct SSessionLockSurface;

class CHyprRenderer {
  public:
    CHyprRenderer();

    void                            renderMonitor(CMonitor* pMonitor);
    void                            outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                            arrangeLayersForMonitor(const int&);
    void                            damageSurface(wlr_surface*, double, double, double scale = 1.0);
    void                            damageWindow(CWindow*);
    void                            damageBox(CBox*);
    void                            damageBox(const int& x, const int& y, const int& w, const int& h);
    void                            damageRegion(const CRegion&);
    void                            damageMonitor(CMonitor*);
    void                            damageMirrorsWith(CMonitor*, const CRegion&);
    bool                            applyMonitorRule(CMonitor*, SMonitorRule*, bool force = false);
    bool                            shouldRenderWindow(CWindow*, CMonitor*, CWorkspace*);
    bool                            shouldRenderWindow(CWindow*);
    void                            ensureCursorRenderingMode();
    bool                            shouldRenderCursor();
    void                            setCursorHidden(bool hide);
    void                            calculateUVForSurface(CWindow*, wlr_surface*, bool main = false, const Vector2D& projSize = {}, bool fixMisalignedFSV1 = false);
    std::tuple<float, float, float> getRenderTimes(CMonitor* pMonitor); // avg max min
    void                            renderLockscreen(CMonitor* pMonitor, timespec* now);
    void                            setOccludedForBackLayers(CRegion& region, CWorkspace* pWorkspace);
    void                            setOccludedForMainWorkspace(CRegion& region, CWorkspace* pWorkspace); // TODO: merge occlusion methods
    bool                            canSkipBackBufferClear(CMonitor* pMonitor);
    void                            recheckSolitaryForMonitor(CMonitor* pMonitor);
    void                            setCursorSurface(wlr_surface* surf, int hotspotX, int hotspotY, bool force = false);
    void                            setCursorFromName(const std::string& name, bool force = false);
    void                            renderSoftwareCursors(CMonitor* pMonitor, const CRegion& damage, std::optional<Vector2D> overridePos = {});
    void                            onRenderbufferDestroy(CRenderbuffer* rb);
    CRenderbuffer*                  getCurrentRBO();
    bool                            isNvidia();
    void                            makeEGLCurrent();
    void                            unsetEGL();

    bool      beginRender(CMonitor* pMonitor, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, wlr_buffer* buffer = nullptr, CFramebuffer* fb = nullptr);
    void      endRender();

    bool      m_bBlockSurfaceFeedback  = false;
    bool      m_bRenderingSnapshot     = false;
    CWindow*  m_pLastScanout           = nullptr;
    CMonitor* m_pMostHzMonitor         = nullptr;
    bool      m_bDirectScanoutBlocked  = false;
    bool      m_bSoftwareCursorsLocked = false;

    DAMAGETRACKINGMODES
    damageTrackingModeFromStr(const std::string&);

    bool                                             attemptDirectScanout(CMonitor*);
    void                                             setWindowScanoutMode(CWindow*);
    void                                             initiateManualCrash();

    bool                                             m_bCrashingInProgress = false;
    float                                            m_fCrashingDistort    = 0.5f;
    wl_event_source*                                 m_pCrashingLoop       = nullptr;

    std::vector<std::unique_ptr<STearingController>> m_vTearingControllers;

    CTimer                                           m_tRenderTimer;

    struct {
        int                         hotspotX;
        int                         hotspotY;
        std::optional<wlr_surface*> surf = nullptr;
        std::string                 name;
    } m_sLastCursorData;

  private:
    void           arrangeLayerArray(CMonitor*, const std::vector<std::unique_ptr<SLayerSurface>>&, bool, CBox*);
    void           renderWorkspaceWindowsFullscreen(CMonitor*, CWorkspace*, timespec*); // renders workspace windows (fullscreen) (tiled, floating, pinned, but no special)
    void           renderWorkspaceWindows(CMonitor*, CWorkspace*, timespec*);           // renders workspace windows (no fullscreen) (tiled, floating, pinned, but no special)
    void           renderWindow(CWindow*, CMonitor*, timespec*, bool, eRenderPassMode, bool ignorePosition = false, bool ignoreAllGeometry = false);
    void           renderLayer(SLayerSurface*, CMonitor*, timespec*);
    void           renderSessionLockSurface(SSessionLockSurface*, CMonitor*, timespec*);
    void           renderDragIcon(CMonitor*, timespec*);
    void           renderIMEPopup(SIMEPopup*, CMonitor*, timespec*);
    void           renderWorkspace(CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now, const CBox& geometry);
    void           renderAllClientsForWorkspace(CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* now, const Vector2D& translate = {0, 0}, const float& scale = 1.f);

    bool           m_bCursorHidden        = false;
    bool           m_bCursorHasSurface    = false;
    CRenderbuffer* m_pCurrentRenderbuffer = nullptr;
    wlr_buffer*    m_pCurrentWlrBuffer    = nullptr;
    eRenderMode    m_eRenderMode          = RENDER_MODE_NORMAL;

    bool           m_bNvidia = false;

    CRenderbuffer* getOrCreateRenderbuffer(wlr_buffer* buffer, uint32_t fmt);
    std::vector<std::unique_ptr<CRenderbuffer>> m_vRenderbuffers;

    friend class CHyprOpenGLImpl;
    friend class CToplevelExportProtocolManager;
    friend class CInputManager;
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
