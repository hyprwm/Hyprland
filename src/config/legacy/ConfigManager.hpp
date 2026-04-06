#pragma once

#include <hyprutils/animation/AnimationConfig.hpp>

#include <vector>
#include <optional>
#include <xf86drmMode.h>

#include "../../desktop/DesktopTypes.hpp"
#include "../../desktop/rule/Rule.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../managers/KeybindManager.hpp"

#include "../ConfigManager.hpp"

#include <hyprlang.hpp>

#define HANDLE void*

namespace Config::Supplementary {
    struct SConfigOptionDescription;
};

namespace Config::Legacy {

    class CConfigManager;

    struct SPluginKeyword {
        HANDLE                       handle = nullptr;
        std::string                  name   = "";
        Hyprlang::PCONFIGHANDLERFUNC fn     = nullptr;
    };

    struct SPluginVariable {
        HANDLE      handle = nullptr;
        std::string name   = "";
    };

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

        void                                     addPluginConfigVar(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value);
        void                                     addPluginKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fun, Hyprlang::SHandlerOptions opts = {});
        void                                     removePluginConfig(HANDLE handle);

        std::string                              parseKeyword(const std::string&, const std::string&);

        Hyprlang::CConfigValue*                  getHyprlangConfigValuePtr(const std::string& name, const std::string& specialCat = "");

        // keywords
        std::optional<std::string> handleRawExec(const std::string&, const std::string&);
        std::optional<std::string> handleExec(const std::string&, const std::string&);
        std::optional<std::string> handleExecOnce(const std::string&, const std::string&);
        std::optional<std::string> handleExecRawOnce(const std::string&, const std::string&);
        std::optional<std::string> handleExecShutdown(const std::string&, const std::string&);
        std::optional<std::string> handleMonitor(const std::string&, const std::string&);
        std::optional<std::string> handleBind(const std::string&, const std::string&);
        std::optional<std::string> handleUnbind(const std::string&, const std::string&);
        std::optional<std::string> handleWorkspaceRules(const std::string&, const std::string&);
        std::optional<std::string> handleBezier(const std::string&, const std::string&);
        std::optional<std::string> handleAnimation(const std::string&, const std::string&);
        std::optional<std::string> handleSource(const std::string&, const std::string&);
        std::optional<std::string> handleSubmap(const std::string&, const std::string&);
        std::optional<std::string> handleBindWS(const std::string&, const std::string&);
        std::optional<std::string> handleEnv(const std::string&, const std::string&);
        std::optional<std::string> handlePlugin(const std::string&, const std::string&);
        std::optional<std::string> handlePermission(const std::string&, const std::string&);
        std::optional<std::string> handleGesture(const std::string&, const std::string&);
        std::optional<std::string> handleWindowrule(const std::string&, const std::string&);
        std::optional<std::string> handleLayerrule(const std::string&, const std::string&);

        std::optional<std::string> handleMonitorv2(const std::string& output);
        Hyprlang::CParseResult     handleMonitorv2();
        std::optional<std::string> addRuleFromConfigKey(const std::string& name);
        std::optional<std::string> addLayerRuleFromConfigKey(const std::string& name);
        Hyprlang::CParseResult     reloadRules();

        std::string                m_configCurrentPath;

        bool                       m_isLaunchingExecOnce                 = false; // For exec-once to skip initial ws tracking
        bool                       m_lastConfigVerificationWasSuccessful = true;

      private:
        // internal methods
        std::optional<std::string>                       resetHLConfig();
        std::optional<std::string>                       verifyConfigExists();
        void                                             reloadRuleConfigs();

        void                                             postConfigReload(const Hyprlang::CParseResult& result);

        void                                             registerConfigVar(const char* name, const Hyprlang::INT& val);
        void                                             registerConfigVar(const char* name, const Hyprlang::FLOAT& val);
        void                                             registerConfigVar(const char* name, const Hyprlang::VEC2& val);
        void                                             registerConfigVar(const char* name, const Hyprlang::STRING& val);
        void                                             registerConfigVar(const char* name, Hyprlang::CUSTOMTYPE&& val);
        Hyprlang::CConfigValue*                          getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback);

        UP<Hyprlang::CConfig>                            m_config;

        std::vector<std::string>                         m_configPaths;
        std::string                                      m_mainConfigPath;

        SSubmap                                          m_currentSubmap;

        std::vector<std::string>                         m_declaredPlugins;
        std::vector<SPluginKeyword>                      m_pluginKeywords;
        std::vector<SPluginVariable>                     m_pluginVariables;

        std::vector<SP<Desktop::Rule::IRule>>            m_keywordRules;

        bool                                             m_isFirstLaunch = true; // For exec-once

        bool                                             m_firstExecDispatched  = false;
        bool                                             m_manualCrashInitiated = false;

        std::vector<std::pair<std::string, std::string>> m_failedPluginConfigValues; // for plugin values of unloaded plugins
        std::string                                      m_configErrors = "";

        uint32_t                                         m_configValueNumber = 0;

        friend struct Config::Supplementary::SConfigOptionDescription;
        friend class CMonitorRuleParser;
    };

    WP<CConfigManager> mgr();
}
