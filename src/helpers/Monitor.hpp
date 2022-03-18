#pragma once

#include "../defines.hpp"
#include <deque>
#include "WLClasses.hpp"
#include <list>

struct SMonitor {
    Vector2D    vecPosition     = Vector2D(0,0);
    Vector2D    vecSize         = Vector2D(0,0);

    bool        primary         = false;

    int         ID              = -1;

    std::string szName          = "";

    Vector2D    vecReservedTopLeft = Vector2D(0,0);
    Vector2D    vecReservedBottomRight = Vector2D(0,0);

    // WLR stuff
    wlr_output* output          = nullptr;
    
    // Double-linked list because we need to have constant mem addresses for signals
    std::list<SLayerSurface>   m_lLayerSurfaces;

    DYNLISTENER(monitorFrame);
    DYNLISTENER(monitorDestroy);


    // For the list lookup

    bool operator==(const SMonitor& rhs) {
        return vecPosition == rhs.vecPosition && vecSize == rhs.vecSize && szName == rhs.szName;
    }
};