#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <cstdint>

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
        // Returns a subscription handle, or std::nullopt if the event name is unknown.
        std::optional<uint64_t>                       registerEvent(const std::string& name, int luaRef);
        bool                                          unregisterEvent(uint64_t handle);

        void                                          clearEvents();

        static const std::unordered_set<std::string>& knownEvents();

      private:
        struct SSubscription {
            std::string eventName;
            int         luaRef = -1;
        };

        lua_State*                                             m_lua = nullptr;
        std::unordered_map<std::string, std::vector<uint64_t>> m_callbacks;
        std::unordered_map<uint64_t, SSubscription>            m_subscriptions;
        std::unordered_set<uint64_t>                           m_activeHandles;
        std::unordered_set<uint64_t>                           m_reentrancyWarnedHandles;
        uint64_t                                               m_nextHandle    = 1;
        size_t                                                 m_dispatchDepth = 0;
        std::vector<CHyprSignalListener>                       m_listeners;

        static constexpr size_t                                MAX_DISPATCH_DEPTH = 32;

        void                                                   dispatch(const std::string& name, int nargs, const std::function<void()>& pushArgs);
    };

}
