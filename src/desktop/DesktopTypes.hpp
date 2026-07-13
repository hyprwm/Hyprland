#pragma once
#include "../helpers/memory/Memory.hpp"

#include <cstdint>

class CWorkspace;

namespace Desktop::View {
    enum eViewType : uint8_t;
    class IView;
    class CWindow;
    class CLayerSurface;
    class CPopup;
}

namespace Monitor {
    class CMonitor;
}

/* Shared pointer to a workspace */
using PHLWORKSPACE = SP<CWorkspace>;
/* Weak pointer to a workspace */
using PHLWORKSPACEREF = WP<CWorkspace>;

/* Shared pointer to a window */
using PHLWINDOW = SP<Desktop::View::CWindow>;
/* Weak pointer to a window */
using PHLWINDOWREF = WP<Desktop::View::CWindow>;

/* Shared pointer to a layer surface */
using PHLLS = SP<Desktop::View::CLayerSurface>;
/* Weak pointer to a layer surface */
using PHLLSREF = WP<Desktop::View::CLayerSurface>;

/* Shared pointer to a view */
using PHLVIEW = SP<Desktop::View::IView>;
/* Weak pointer to a view */
using PHLVIEWREF = WP<Desktop::View::IView>;

/* Shared pointer to a monitor */
using PHLMONITOR = SP<Monitor::CMonitor>;
/* Weak pointer to a monitor */
using PHLMONITORREF = WP<Monitor::CMonitor>;
