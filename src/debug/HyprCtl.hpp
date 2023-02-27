#pragma once

#include "../Compositor.hpp"
#include <fstream>
#include "../helpers/MiscFunctions.hpp"

namespace HyprCtl {
    void        startHyprCtlSocket();
    std::string makeDynamicCall(const std::string& input);

    // very simple thread-safe request method
    inline bool             requestMade  = false;
    inline bool             requestReady = false;
    inline std::string      request      = "";

    inline std::ifstream    requestStream;

    inline wl_event_source* hyprCtlTickSource = nullptr;

    inline int              iSocketFD = -1;

    enum eHyprCtlOutputFormat {
        FORMAT_NORMAL = 0,
        FORMAT_JSON
    };
};