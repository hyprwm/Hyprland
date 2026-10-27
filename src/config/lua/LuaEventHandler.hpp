#pragma once

#include <expected>
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
#include "event/EventBus.hpp"
#include "LuaRef.hpp"

extern "C" {
#include <lua.h>
}

namespace Config::Lua {
    class CLuaCaller;

    // Manages hl.on() event subscriptions for a single Lua state lifetime.
    // Destroyed (and recreated) on every config reload so callbacks never double-up.
    //
    // Hyprland objects (window, workspace, monitor, layer surface) are exposed to Lua
    // as typed userdata holding a weak pointer.  Field accesses read live C++ state via
    // __index; accessing a field on a destroyed object raises a Lua error.
    class CLuaEventHandler {
      public:
        explicit CLuaEventHandler(CLuaCaller& caller);
        ~CLuaEventHandler();

        // Store a Lua function to be called when `name` fires.
        // Returns a subscription handle, or std::nullopt if the event name is unknown.
        std::optional<uint64_t>                registerEvent(const std::string& name, CLuaRef&& fn);
        bool                                   unregisterEvent(uint64_t handle);

        void                                   clearEvents();

        static std::unordered_set<std::string> knownEvents();

      private:
        struct SSubscription {
            std::string eventName;
            CLuaRef     fn;
        };

        CLuaCaller&                                            m_caller;
        std::unordered_map<std::string, std::vector<uint64_t>> m_callbacks;
        std::unordered_map<uint64_t, SSubscription>            m_subscriptions;
        uint64_t                                               m_nextHandle = 1;
        std::vector<CHyprSignalListener>                       m_listeners;

        void                                                   dispatch(const std::string& name, int nargs, const std::function<void(lua_State*)>& pushArgs);
    };

}
