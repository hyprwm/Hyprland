#pragma once

#include "../defines.hpp"

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);
    void                outputMgrApplyTest(wlr_output_configuration_v1*, bool);

private:
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
