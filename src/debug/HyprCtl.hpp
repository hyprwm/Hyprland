#pragma once

#include "../Compositor.hpp"
#include <fstream>

namespace HyprCtl {
    void            startHyprCtlSocket();
    void            tickHyprCtl();

    inline std::ifstream requestStream;
};