#pragma once

#include "LuaRef.hpp"

#include <chrono>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Config::Lua {
    class CLuaCaller {
      public:
        using FErrorCallback = std::function<void(std::string&&)>;

        CLuaCaller(lua_State* L, FErrorCallback&& onError);
        ~CLuaCaller();

        CLuaCaller(const CLuaCaller&)                                 = delete;
        CLuaCaller&                      operator=(const CLuaCaller&) = delete;

        int                              guardedPCall(lua_State* L, int nargs, int nresults, int errfunc, int timeoutMs, std::string_view context);

        std::expected<void, std::string> call(const CLuaRef& fn, int timeoutMs, std::string_view context, const std::function<int(lua_State*)>& pushArgs = {},
                                              const std::function<void(lua_State*)>& readResult = {});
        std::expected<void, std::string> callPushed(int nargs, int timeoutMs, std::string_view context, const std::function<void(lua_State*)>& readResult = {});

        std::string                      errorToString(lua_State* L, int idx);

        lua_State*                       state() const;

        static CLuaCaller*               fromLuaState(lua_State* L);

        static constexpr int             LUA_WATCHDOG_INSTRUCTION_INTERVAL = 10000;
        static constexpr int             LUA_TIMEOUT_CONFIG_RELOAD_MS      = 1500;
        static constexpr int             LUA_TIMEOUT_EVENT_CALLBACK_MS     = 50;
        static constexpr int             LUA_TIMEOUT_KEYBIND_CALLBACK_MS   = 100;
        static constexpr int             LUA_TIMEOUT_TIMER_CALLBACK_MS     = 50;
        static constexpr int             LUA_TIMEOUT_LAYOUT_CALLBACK_MS    = 50;
        static constexpr int             LUA_TIMEOUT_EVAL_MS               = 250;
        static constexpr int             LUA_TIMEOUT_DISPATCH_MS           = 100;

        static constexpr size_t          MAX_CALL_DEPTH = 32;

      private:
        static void                           watchdogHook(lua_State* L, lua_Debug* ar);

        bool                                  markReported(int fnIdx, const std::string& message);

        lua_State*                            m_lua = nullptr;
        FErrorCallback                        m_onError;

        bool                                  m_watchdogActive = false;
        std::chrono::steady_clock::time_point m_watchdogDeadline;
        std::string_view                      m_watchdogContext;

        std::vector<const void*>              m_running;
    };
}
