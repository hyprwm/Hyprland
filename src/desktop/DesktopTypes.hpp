#pragma once
#include "../macros.hpp"
class CWorkspace;
class CWindow;
class CLayerSurface;

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
