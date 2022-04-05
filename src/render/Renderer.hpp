#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../helpers/Workspace.hpp"
#include "../Window.hpp"

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                arrangeLayersForMonitor(const int&);
    void                damageSurface(SMonitor*, double, double, wlr_surface*, void*);

private:
    void                arrangeLayerArray(SMonitor*, const std::list<SLayerSurface*>&, bool, wlr_box*);
    void                drawBorderForWindow(CWindow*, SMonitor*, float a = 255.f);
    void                renderWorkspaceWithFullscreenWindow(SMonitor*, SWorkspace*, timespec*);
    void                renderWindow(CWindow*, SMonitor*, timespec*, bool);
    void                renderDragIcon(SMonitor*, timespec*);
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
