#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"

namespace HyprCtl {
    void            startHyprCtlSocket();
    void            tickHyprCtl();

    // very simple thread-safe request method
    inline  bool    requestMade = false;
    inline  bool    requestReady = false;
    inline  std::string request = "";

    inline std::ifstream requestStream;

    enum eHyprCtlOutputFormat {
        FORMAT_NORMAL = 0,
        FORMAT_JSON
    };
};