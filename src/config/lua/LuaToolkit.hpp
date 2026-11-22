#pragma once

#include "../../helpers/signal/Signal.hpp"

#include <functional>
#include <string>
#include <unordered_set>

struct lua_State;

namespace Config::Lua {
    class CLuaCaller;

    // Exposes hyprtoolkit-lua as require("hyprtoolkit"), bound to the compositor's embedded backend.
    // The config is parsed before the renderer exists, so the backend is attached once it's ready.
    class CLuaToolkit {
      public:
        using FErrorCallback = std::function<void(std::string&&)>;

        CLuaToolkit(lua_State* L, CLuaCaller& caller, FErrorCallback&& onError);
        ~CLuaToolkit();

        CLuaToolkit(const CLuaToolkit&)            = delete;
        CLuaToolkit& operator=(const CLuaToolkit&) = delete;

      private:
        void           attach();
        void           retire();
        void           reportError(std::string_view error);

        lua_State*     m_lua    = nullptr;
        bool           m_active = false;
        FErrorCallback m_onError;

        // callbacks can run every frame, report each error once
        std::unordered_set<std::string> m_reportedErrors;

        struct {
            CHyprSignalListener hostReady;
            CHyprSignalListener hostDestroy;
        } m_listeners;
    };
}
