#include "includes.hpp"
#include "debug/Log.hpp"
#include "helpers/WLListener.hpp"
#include "helpers/Color.hpp"
#include "macros.hpp"

class CWindow;
class CLayerSurface;

/* Shared pointer to a window */
typedef SP<CWindow> PHLWINDOW;
/* Weak pointer to a window */
typedef WP<CWindow> PHLWINDOWREF;

/* Shared pointer to a layer surface */
typedef SP<CLayerSurface> PHLLS;
/* Weak pointer to a layer surface */
typedef WP<CLayerSurface> PHLLSREF;
