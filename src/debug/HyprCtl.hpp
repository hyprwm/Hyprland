#pragma once

#include "../Compositor.hpp"
#include <fstream>

namespace HyprCtl {
    void            tickHyprCtl();

    inline std::ifstream requestStream;
};