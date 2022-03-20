#pragma once

#include "../defines.hpp"

struct SWorkspace {
    int ID = -1;
    uint64_t monitorID = -1;
    bool hasFullscreenWindow = false;
};