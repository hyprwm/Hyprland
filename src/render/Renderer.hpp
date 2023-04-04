#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../helpers/Workspace.hpp"
#include "../Window.hpp"
#include "OpenGL.hpp"

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

class CToplevelExportProtocolManager;
class CInputManager;
struct SSessionLockSurface;

class CHyprRenderer {
  public:
    void                            renderMonitor(CMonitor* pMonitor);
    void                            renderAllClientsForMonitor(const int&, timespec*);
    void                            outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                            arrangeLayersForMonitor(const int&);
    void                            damageSurface(wlr_surface*, double, double);
    void                            damageWindow(CWindow*);
    void                            damageBox(wlr_box*);
    void                            damageBox(const int& x, const int& y, const int& w, const int& h);
    void                            damageRegion(pixman_region32_t*);
    void                            damageMonitor(CMonitor*);
    void                            damageMirrorsWith(CMonitor*, pixman_region32_t*);
    bool                            applyMonitorRule(CMonitor*, SMonitorRule*, bool force = false);
    bool                            shouldRenderWindow(CWindow*, CMonitor*);
    bool                            shouldRenderWindow(CWindow*);
    void                            ensureCursorRenderingMode();
    bool                            shouldRenderCursor();
    void                            calculateUVForSurface(CWindow*, wlr_surface*, bool main = false);
    std::tuple<float, float, float> getRenderTimes(CMonitor* pMonitor); // avg max min

    bool                            m_bWindowRequestedCursorHide = false;
    bool                            m_bBlockSurfaceFeedback      = false;
    bool                            m_bRenderingSnapshot         = false;
    CWindow*                        m_pLastScanout               = nullptr;
    CMonitor*                       m_pMostHzMonitor             = nullptr;
    bool                            m_bDirectScanoutBlocked      = false;
    bool                            m_bSoftwareCursorsLocked     = false;

    DAMAGETRACKINGMODES             damageTrackingModeFromStr(const std::string&);

    bool                            attemptDirectScanout(CMonitor*);
    void                            setWindowScanoutMode(CWindow*);
    void                            initiateManualCrash();

    bool                            m_bCrashingInProgress = false;
    float                           m_fCrashingDistort    = 0.5f;
    wl_event_source*                m_pCrashingLoop       = nullptr;

  private:
    void arrangeLayerArray(CMonitor*, const std::vector<std::unique_ptr<SLayerSurface>>&, bool, wlr_box*);
    void renderWorkspaceWithFullscreenWindow(CMonitor*, CWorkspace*, timespec*);
    void renderWindow(CWindow*, CMonitor*, timespec*, bool, eRenderPassMode, bool ignorePosition = false, bool ignoreAllGeometry = false);
    void renderLayer(SLayerSurface*, CMonitor*, timespec*);
    void renderSessionLockSurface(SSessionLockSurface*, CMonitor*, timespec*);
    void renderDragIcon(CMonitor*, timespec*);
    void renderIMEPopup(SIMEPopup*, CMonitor*, timespec*);

    bool m_bHasARenderedCursor = true;

    friend class CHyprOpenGLImpl;
    friend class CToplevelExportProtocolManager;
    friend class CInputManager;
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
