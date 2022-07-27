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
    DAMAGE_TRACKING_NONE = 0,
    DAMAGE_TRACKING_MONITOR,
    DAMAGE_TRACKING_FULL
};

enum eRenderPassMode {
    RENDER_PASS_ALL = 0,
    RENDER_PASS_MAIN,
    RENDER_PASS_POPUP
};

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                arrangeLayersForMonitor(const int&);
    void                damageSurface(wlr_surface*, double, double);
    void                damageWindow(CWindow*);
    void                damageBox(wlr_box*);
    void                damageBox(const int& x, const int& y, const int& w, const int& h);
    void                damageRegion(pixman_region32_t*);
    void                damageMonitor(CMonitor*);
    bool                applyMonitorRule(CMonitor*, SMonitorRule*, bool force = false);
    bool                shouldRenderWindow(CWindow*, CMonitor*);
    bool                shouldRenderWindow(CWindow*);
    void                ensureCursorRenderingMode();
    bool                shouldRenderCursor();

    bool                m_bWindowRequestedCursorHide = false;

    DAMAGETRACKINGMODES damageTrackingModeFromStr(const std::string&);

private:
    void                arrangeLayerArray(CMonitor*, const std::vector<std::unique_ptr<SLayerSurface>>&, bool, wlr_box*);
    void                renderWorkspaceWithFullscreenWindow(CMonitor*, CWorkspace*, timespec*);
    void                renderWindow(CWindow*, CMonitor*, timespec*, bool, eRenderPassMode);
    void                renderLayer(SLayerSurface*, CMonitor*, timespec*);
    void                renderDragIcon(CMonitor*, timespec*);

    bool                m_bHasARenderedCursor = true;


    friend class CHyprOpenGLImpl;
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
