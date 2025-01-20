#pragma once
#include "../helpers/memory/Memory.hpp"
class CWorkspace;
class CWindow;
class CLayerSurface;
class CMonitor;

/* Shared pointer to a workspace */
typedef SP<CWorkspace> PHLWORKSPACE;
/* Weak pointer to a workspace */
typedef WP<CWorkspace> PHLWORKSPACEREF;

/* Shared pointer to a window */
typedef SP<CWindow> PHLWINDOW;
/* Weak pointer to a window */
typedef WP<CWindow> PHLWINDOWREF;

/* Shared pointer to a layer surface */
typedef SP<CLayerSurface> PHLLS;
/* Weak pointer to a layer surface */
typedef WP<CLayerSurface> PHLLSREF;

/* Shared pointer to a monitor */
typedef SP<CMonitor> PHLMONITOR;
/* Weak pointer to a monitor */
typedef WP<CMonitor> PHLMONITORREF;
