#pragma once
#include "../helpers/memory/Memory.hpp"
class CWorkspace;
class CWindow;
class CLayerSurface;
class CMonitor;

/* Shared pointer to a workspace */
using PHLWORKSPACE = SP<CWorkspace>;
/* Weak pointer to a workspace */
using PHLWORKSPACEREF = WP<CWorkspace>;

/* Shared pointer to a window */
using PHLWINDOW = SP<CWindow>;
/* Weak pointer to a window */
using PHLWINDOWREF = WP<CWindow>;

/* Shared pointer to a layer surface */
using PHLLS = SP<CLayerSurface>;
/* Weak pointer to a layer surface */
using PHLLSREF = WP<CLayerSurface>;

/* Shared pointer to a monitor */
using PHLMONITOR = SP<CMonitor>;
/* Weak pointer to a monitor */
using PHLMONITORREF = WP<CMonitor>;
