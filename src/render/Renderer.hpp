#pragma once

#include "../defines.hpp"

class CHyprRenderer {
public:

    void                renderAllClientsForMonitor(const int&, timespec*);

private:
};

inline std::unique_ptr<CHyprRenderer> g_pHyprRenderer;
