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

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                arrangeLayersForMonitor(const int&);
    void                damageSurface(wlr_surface*, double, double);
    void                damageWindow(CWindow*);
    void                damageBox(wlr_box*);
    void                damageBox(const int& x, const int& y, const int& w, const int& h);
    void                damageMonitor(SMonitor*);
    void                applyMonitorRule(SMonitor*, SMonitorRule*, bool force = false);

    DAMAGETRACKINGMODES damageTrackingModeFromStr(const std::string&);

private:
    void                arrangeLayerArray(SMonitor*, const std::list<SLayerSurface*>&, bool, wlr_box*);
    void                drawBorderForWindow(CWindow*, SMonitor*, float a = 255.f);
    void                renderWorkspaceWithFullscreenWindow(SMonitor*, CWorkspace*, timespec*);
    void                renderWindow(CWindow*, SMonitor*, timespec*, bool);
    void                renderDragIcon(SMonitor*, timespec*);


    friend class CHyprOpenGLImpl;
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
