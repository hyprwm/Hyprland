#include "XWaylandPointer.hpp"
#include "globals.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <src/includes.hpp>

// Read the seat's last pointer motion and the X11 surface without adding a production test API.
// This has to come before any header that pulls those classes in.
#define private public
#include <src/managers/SeatManager.hpp>
#include <src/desktop/view/window/X11Backend.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/window/Window.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorState.hpp>
#include <src/xwayland/XSurface.hpp>
#undef private

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace XWaylandPointer {
    // The X server places the pointer at the X11 window's origin plus the surface-local coords we sent it.
    // That must be where the cursor is in XWayland's own layout of the monitors, on whichever monitor it is.
    static std::string runCheck(const std::string& cls) {
#ifdef NO_XWAYLAND
        return {};
#else
        static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

        const auto  WINDOWS = Desktop::windowState()->windows();
        const auto  IT      = std::ranges::find_if(WINDOWS, [&](const auto& w) { return w->metadata().appID() == cls; });
        if (IT == WINDOWS.end())
            return std::format("no window with class '{}'", cls);

        const auto WINDOW = *IT;
        const auto X11    = dynamic_cast<const Desktop::View::CX11Backend*>(&WINDOW->backend());
        if (!X11)
            return std::format("'{}' is not an X11 window", cls);

        const auto XSURF = X11->m_xwaylandSurface.lock();
        if (!XSURF)
            return "the X11 window has no xwayland surface";

        if (g_pSeatManager->m_state.pointerFocus.lock() != WINDOW->backend().surface())
            return "the pointer focus is not on the X11 window";

        const auto CURSOR  = g_pInputManager->getMouseCoordsInternal();
        const auto MONITOR = State::monitorState()->query().vec(CURSOR).run();
        if (!MONITOR)
            return std::format("no monitor under the cursor at {}", CURSOR);

        const auto SCALE    = *PXWLFORCESCALEZERO ? MONITOR->m_scale : 1.0;
        const auto EXPECTED = MONITOR->m_xwaylandPosition + ((CURSOR - MONITOR->m_position) * SCALE);
        const auto ACTUAL   = XSURF->m_geometry.pos() + g_pSeatManager->m_lastLocalCoords;

        if (ACTUAL.distance(EXPECTED) > 1.5)
            return std::format("X11 pointer at {} (window at {} + local {}), expected {} on {} (cursor {})", ACTUAL, XSURF->m_geometry.pos(), g_pSeatManager->m_lastLocalCoords,
                               EXPECTED, MONITOR->m_name, CURSOR);

        return {};
#endif
    }

    static int luaExpectXWaylandPointer(lua_State* L) {
        const auto CLS    = luaL_checkstring(L, 1);
        bool       failed = false;
        {
            const auto ERROR = runCheck(CLS);
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        // All C++ state is gone before Lua can longjmp.
        return failed ? lua_error(L) : 0;
    }

    void registerFunctions() {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", "expect_xwayland_pointer", luaExpectXWaylandPointer))
            LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.expect_xwayland_pointer");
    }
}
