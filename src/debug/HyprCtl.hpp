#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"

namespace HyprCtl {
    void            startHyprCtlSocket();
    void            tickHyprCtl();

    inline std::ifstream requestStream;
};