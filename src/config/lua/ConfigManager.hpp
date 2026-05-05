#pragma once

#include <hyprutils/animation/AnimationConfig.hpp>

#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <string_view>
#include <unordered_map>
#include <expected>

#include "../../helpers/memory/Memory.hpp"
#include "../ConfigManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"

#include "./types/LuaConfigValue.hpp"
#include "./LuaEventHandler.hpp"

#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"

#include "../../SharedDefs.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../shared/ConfigErrors.hpp"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace Config::Supplementary {
    struct SConfigOptionDescription;
};

namespace Config::Lua {
    class CConfigManager;
    class CConfigManagerPluginLuaTestAccessor;
}

namespace Config::Lua::Layouts {
    struct SLuaLayoutProvider;
}

namespace Config::Lua::Bindings {
    void registerBindings(lua_State* L, CConfigManager* mgr);
}

namespace Config::Lua {

    class CConfigManager : public Config::IConfigManager {
      public:
        CConfigManager();

        virtual eConfigManagerType               type() override;

        virtual void                             init() override;
        virtual void                             reload() override;
        virtual std::string                      verify() override;

        virtual int                              getDeviceInt(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual float                            getDeviceFloat(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual Vector2D                         getDeviceVec(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual std::string                      getDeviceString(const std::string&, const std::string&, const std::string& fallback = "") override;
        virtual bool                             deviceConfigExplicitlySet(const std::string&, const std::string&) override;
        virtual bool                             deviceConfigExists(const std::string&) override;

        virtual SConfigOptionReply               getConfigValue(const std::string&) override;

        virtual std::string                      getMainConfigPath() override;
        virtual std::string                      getErrors() override;
        virtual std::string                      getConfigString() override;
        virtual std::string                      currentConfigPath() override;
        virtual const std::vector<std::string>&  getConfigPaths() override;

        virtual std::expected<void, std::string> generateDefaultConfig(const std::filesystem::path&, bool safeMode) override;

        virtual void                             handlePluginLoads() override;
        virtual bool                             configVerifPassed() override;

        virtual std::expected<void, std::string> registerPluginValue(void* handle, SP<Config::Values::IValue> value) override;
        virtual void                             onPluginUnload(void* handle) override;

        int                                      invokePluginLuaFunctionByID(uint64_t id, lua_State* L);

        std::expected<void, std::string>         registerPluginLuaFunction(void* handle, const std::string& namespace_, const std::string& name, PLUGIN_LUA_FN fn);
        std::expected<void, std::string>         unregisterPluginLuaFunction(void* handle, const std::string& namespace_, const std::string& name);

        void                                     addError(std::string&& str);
        void                                     addEvalIssue(const Config::SConfigError& err);

        void                                     registerLuaRef(int ref);
        void                                     callLuaFn(int ref);
        std::expected<void, std::string>         registerLuaLayoutProvider(std::string name, lua_State* L, int providerTableIdx);

        // execute an arbitrary lua string on the current state.
        std::optional<std::string> eval(const std::string& code);

        int                        guardedPCall(int nargs, int nresults, int errfunc, int timeoutMs, std::string_view context);

        static CConfigManager*     fromLuaState(lua_State* L);

        static constexpr int       LUA_WATCHDOG_INSTRUCTION_INTERVAL = 10000;
        static constexpr int       LUA_TIMEOUT_CONFIG_RELOAD_MS      = 1500;
        static constexpr int       LUA_TIMEOUT_EVENT_CALLBACK_MS     = 50;
        static constexpr int       LUA_TIMEOUT_KEYBIND_CALLBACK_MS   = 100;
        static constexpr int       LUA_TIMEOUT_TIMER_CALLBACK_MS     = 50;
        static constexpr int       LUA_TIMEOUT_LAYOUT_CALLBACK_MS    = 50;
        static constexpr int       LUA_TIMEOUT_EVAL_MS               = 250;
        static constexpr int       LUA_TIMEOUT_DISPATCH_MS           = 100;

        bool                       isFirstLaunch() const;
        bool                       isDynamicParse() const;

        std::string                m_currentSubmap;
        std::string                m_currentSubmapReset;

        UP<CLuaEventHandler>       m_eventHandler;

        struct SLuaTimer {
            SP<CEventLoopTimer> timer;
            int                 luaRef = LUA_NOREF; // registry ref to the lua callback
        };
        std::vector<SLuaTimer>                               m_luaTimers;

        std::vector<std::string>                             m_registeredPlugins;

        std::unordered_map<std::string, UP<ILuaConfigValue>> m_configValues;

        struct SDeviceConfig {
            SDeviceConfig();

            std::unordered_map<std::string, UP<ILuaConfigValue>> values;
        };

        std::unordered_map<std::string, SDeviceConfig> m_deviceConfigs;
        std::vector<std::string>                       m_errors, m_configPaths;
        std::vector<Config::SConfigError>              m_evalIssues;

        // named window/layer rules for merge-on-redeclaration
        std::unordered_map<std::string, SP<Desktop::Rule::CWindowRule>> m_luaWindowRules;
        std::unordered_map<std::string, SP<Desktop::Rule::CLayerRule>>  m_luaLayerRules;

      private:
        void                                         reinitLuaState();
        void                                         postConfigReload();
        void                                         registerValue(const char* name, ILuaConfigValue* val);
        void                                         cleanTimers();
        void                                         clearLuaLayoutProviders();
        void                                         clearHeldLuaRefs();
        std::string                                  luaConfigValueName(const std::string& s);
        std::expected<void, std::string>             registerPluginLuaFunctionInState(uint64_t id, const std::string& namespace_, const std::string& name);
        std::expected<void, std::string>             unregisterPluginLuaFunctionInState(const std::string& namespace_, const std::string& name);
        void                                         erasePluginLuaFunction(uint64_t id);
        void                                         reregisterLuaPluginFns();

        static void                                  watchdogHook(lua_State* L, lua_Debug* ar);

        lua_State*                                   m_lua = nullptr;

        bool                                         m_lastConfigVerificationWasSuccessful = true;
        bool                                         m_isFirstLaunch                       = true;
        bool                                         m_manualCrashInitiated                = false;
        bool                                         m_watchdogActive                      = false;
        bool                                         m_isParsingConfig                     = false;
        bool                                         m_isEvaluating                        = false;

        std::chrono::steady_clock::time_point        m_watchdogDeadline;
        std::string                                  m_watchdogContext;

        std::string                                  m_mainConfigPath;

        std::vector<int>                             m_heldLuaRefs;
        std::vector<SP<Layouts::SLuaLayoutProvider>> m_luaLayoutProviders;

        // this is here for legacy reasons.
        std::unordered_map<std::string, const void*> m_configPtrMap;

        // this is here for plugin reasons.
        std::unordered_map<void* /* HANDLE */, std::vector<std::string>> m_pluginValues;

        struct SPluginLuaFunction {
            uint64_t              id     = 0;
            void*                 handle = nullptr;
            std::string           namespace_;
            std::string           name;
            Config::PLUGIN_LUA_FN fn = nullptr;
        };
        std::vector<SPluginLuaFunction> m_pluginLuaFunctions;

        ILuaConfigValue*                findDeviceValue(const std::string& dev, const std::string& field);

        friend class CConfigManagerPluginLuaTestAccessor;
    };

    WP<CConfigManager> mgr();
}
