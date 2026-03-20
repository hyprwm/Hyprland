#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../desktop/DesktopTypes.hpp"

extern "C" {
#include <lua.h>
}

namespace Config::Lua {

    // Manages hl.on() event subscriptions for a single Lua state lifetime.
    // Destroyed (and recreated) on every config reload so callbacks never double-up.
    //
    // Hyprland objects (window, workspace, monitor, layer surface) are exposed to Lua
    // as typed userdata holding a weak pointer.  Field accesses read live C++ state via
    // __index; accessing a field on a destroyed object raises a Lua error.
    class CLuaEventHandler {
      public:
        explicit CLuaEventHandler(lua_State* L);
        ~CLuaEventHandler();

        // Store a Lua function (as a registry ref) to be called when `name` fires.
        // Returns false and leaves the ref unclaimed if the event name is unknown.
        bool                                          registerEvent(const std::string& name, int luaRef);

        static const std::unordered_set<std::string>& knownEvents();

      private:
        lua_State*                                        m_lua = nullptr;
        std::unordered_map<std::string, std::vector<int>> m_callbacks;
        std::vector<CHyprSignalListener>                  m_listeners;

        void                                              dispatch(const std::string& name, int nargs, const std::function<void()>& pushArgs);
    };

} // namespace Config::Lua
