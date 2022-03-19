#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);
    void                arrangeLayersForMonitor(const int&);

private:
    void                arrangeLayerArray(SMonitor*, const std::list<SLayerSurface*>&);
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
