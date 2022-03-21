#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../Window.hpp"

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                arrangeLayersForMonitor(const int&);
    void                damageSurface(SMonitor*, double, double, wlr_surface*, void*);

private:
    void                arrangeLayerArray(SMonitor*, const std::list<SLayerSurface*>&);
    void                drawBorderForWindow(CWindow*, SMonitor*);
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
