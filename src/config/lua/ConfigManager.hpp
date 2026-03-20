#pragma once

#include <hyprutils/animation/AnimationConfig.hpp>

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "../../helpers/memory/Memory.hpp"
#include "../ConfigManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"

#include "./types/LuaConfigValue.hpp"
#include "./LuaEventHandler.hpp"

#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"

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

        void                                     addError(std::string&& str);

        // execute an arbitrary lua string on the current state.
        std::optional<std::string> eval(const std::string& code);

        bool                       isFirstLaunch() const;

        // submap context tracked during config parsing (used by LuaBindings)
        std::string m_currentSubmap;

        // event subscriptions for hl.on(); reset on every reinitLuaState()
        UP<CLuaEventHandler> m_eventHandler;

        // timers created via hl.timer(); cleared on reload / reinitLuaState()
        struct SLuaTimer {
            SP<CEventLoopTimer> timer;
            int                 luaRef = LUA_NOREF; // registry ref to the lua callback
        };
        std::vector<SLuaTimer>                               m_luaTimers;

        std::vector<std::string>                             m_registeredPlugins;

        std::unordered_map<std::string, UP<ILuaConfigValue>> m_configValues;

        struct SDeviceConfig {
            std::unordered_map<std::string, UP<ILuaConfigValue>> values;
        };

        std::unordered_map<std::string, SDeviceConfig> m_deviceConfigs;
        std::vector<std::string>                       m_errors, m_configPaths;

        // named window/layer rules for merge-on-redeclaration
        std::unordered_map<std::string, SP<Desktop::Rule::CWindowRule>> m_luaWindowRules;
        std::unordered_map<std::string, SP<Desktop::Rule::CLayerRule>>  m_luaLayerRules;

      private:
        void        reinitLuaState();
        void        postConfigReload();
        void        registerValue(const char* name, ILuaConfigValue* val);
        void        cleanTimers();
        std::string luaConfigValueName(const std::string& s);

        lua_State*  m_lua = nullptr;

        bool        m_lastConfigVerificationWasSuccessful = true;
        bool        m_isFirstLaunch                       = true;
        bool        m_manualCrashInitiated                = false;

        std::string m_mainConfigPath;

        // this is here for legacy reasons.
        std::unordered_map<std::string, const void*> m_configPtrMap;

        // this is here for plugin reasons.
        std::unordered_map<void* /* HANDLE */, std::vector<WP<ILuaConfigValue>>> m_pluginValues;

        ILuaConfigValue*                                                         findDeviceValue(const std::string& dev, const std::string& field);
    };

    WP<CConfigManager> mgr();
}
