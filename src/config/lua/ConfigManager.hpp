#pragma once

#include <hyprutils/animation/AnimationConfig.hpp>

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <expected>

#include "../../helpers/memory/Memory.hpp"
#include "../ConfigManager.hpp"
#include "../../managers/eventLoop/EventLoopTimer.hpp"

#include "./types/LuaConfigValue.hpp"
#include "./LuaEventHandler.hpp"
#include "./LuaRef.hpp"

#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"

#include "../../SharedDefs.hpp"
#include "../../keybinds/Manager.hpp"
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
    class CLuaCaller;
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
        virtual ~CConfigManager() override;

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

        virtual bool                             configLoaded() override;
        virtual bool                             configVerifPassed() override;


        virtual void                             warnDeprecated(std::string what, std::string why) override;
        virtual void                             clearDeprecations() override;
        virtual std::vector<std::string>         deprecationNotices() const override;

        void                                     addError(std::string&& str);
        void                                     addEvalIssue(const Config::SConfigError& err);

        std::expected<void, std::string>         registerLuaLayoutProvider(std::string name, lua_State* L, int providerTableIdx);
        SP<SLuaStateLifetime>                    luaStateLifetime() const;
        CLuaCaller&                              caller();

        // execute an arbitrary lua string on the current state.
        std::optional<std::string>                           eval(const std::string& code, bool repl = false);

        static CConfigManager*                               fromLuaState(lua_State* L);

        bool                                                 isFirstLaunch() const;
        bool                                                 isDynamicParse() const;
        bool                                                 isREPL() const;

        std::string                                          m_currentSubmap;
        std::string                                          m_currentSubmapReset;

        UP<CLuaEventHandler>                                 m_eventHandler;

        std::vector<SP<CEventLoopTimer>>                     m_luaTimers;

        std::unordered_map<std::string, UP<ILuaConfigValue>> m_configValues;

        struct SDeviceConfig {
            SDeviceConfig();

            std::unordered_map<std::string, UP<ILuaConfigValue>> values;
        };

        std::unordered_map<std::string, SDeviceConfig> m_deviceConfigs;
        std::vector<std::string>                       m_errors, m_configPaths, m_prints;
        std::vector<Config::SConfigError>              m_evalIssues;

        // named window/layer rules for merge-on-redeclaration
        std::unordered_map<std::string, SP<Desktop::Rule::CWindowRule>> m_luaWindowRules;
        std::unordered_map<std::string, SP<Desktop::Rule::CLayerRule>>  m_luaLayerRules;

      private:
        void                                         reinitLuaState();
        void                                         postConfigReload();
        void                                         registerValue(const char* name, UP<ILuaConfigValue>&& val);
        void                                         cleanTimers();
        void                                         clearLuaLayoutProviders();
        std::string                                  luaConfigValueName(const std::string& s);

        lua_State*                                   m_lua          = nullptr;
        bool                                         m_ownsLuaState = false;
        SP<SLuaStateLifetime>                        m_luaStateLifetime;
        UP<CLuaCaller>                               m_caller;

        bool                                         m_lastConfigVerificationWasSuccessful = true;
        bool                                         m_isFirstLaunch                       = true;
        bool                                         m_manualCrashInitiated                = false;
        bool                                         m_isParsingConfig                     = false;
        bool                                         m_isEvaluating                        = false;
        bool                                         m_isREPL                              = false;

        std::string                                  m_mainConfigPath;

        std::vector<SP<Layouts::SLuaLayoutProvider>> m_luaLayoutProviders;

        // additional deprecation notices, added at runtime
        std::vector<std::pair<std::string, std::string>> m_extraDeprecations;

        // this is here for legacy reasons.
        std::unordered_map<std::string, const void*> m_configPtrMap;

        ILuaConfigValue*                findDeviceValue(const std::string& dev, const std::string& field);

        friend class CConfigManagerPluginLuaTestAccessor;
    };

    WP<CConfigManager> mgr();
}
