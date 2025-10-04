#pragma once

#include <hyprutils/animation/AnimationConfig.hpp>
#define CONFIG_MANAGER_H

#include <map>
#include <unordered_map>
#include "../defines.hpp"
#include <variant>
#include <vector>
#include <optional>
#include <functional>
#include <xf86drmMode.h>
#include "../helpers/Monitor.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/LayerRule.hpp"

#include "ConfigDataValues.hpp"
#include "../SharedDefs.hpp"
#include "../helpers/Color.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../desktop/WindowRule.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../managers/KeybindManager.hpp"

#include <hyprlang.hpp>

#define HANDLE void*

class CConfigManager;

struct SWorkspaceRule {
    std::string                        monitor         = "";
    std::string                        workspaceString = "";
    std::string                        workspaceName   = "";
    WORKSPACEID                        workspaceId     = -1;
    bool                               isDefault       = false;
    bool                               isPersistent    = false;
    std::optional<CCssGapData>         gapsIn;
    std::optional<CCssGapData>         gapsOut;
    std::optional<CCssGapData>         floatGaps = gapsOut;
    std::optional<int64_t>             borderSize;
    std::optional<bool>                decorate;
    std::optional<bool>                noRounding;
    std::optional<bool>                noBorder;
    std::optional<bool>                noShadow;
    std::optional<std::string>         onCreatedEmptyRunCmd;
    std::optional<std::string>         defaultName;
    std::map<std::string, std::string> layoutopts;
};

struct SMonitorAdditionalReservedArea {
    int top    = 0;
    int bottom = 0;
    int left   = 0;
    int right  = 0;
};

struct SPluginKeyword {
    HANDLE                       handle = nullptr;
    std::string                  name   = "";
    Hyprlang::PCONFIGHANDLERFUNC fn     = nullptr;
};

struct SPluginVariable {
    HANDLE      handle = nullptr;
    std::string name   = "";
};

struct SExecRequestedRule {
    std::string szRule = "";
    uint64_t    iPid   = 0;
};

enum eConfigOptionType : uint8_t {
    CONFIG_OPTION_BOOL         = 0,
    CONFIG_OPTION_INT          = 1, /* e.g. 0/1/2*/
    CONFIG_OPTION_FLOAT        = 2,
    CONFIG_OPTION_STRING_SHORT = 3, /* e.g. "auto" */
    CONFIG_OPTION_STRING_LONG  = 4, /* e.g. a command */
    CONFIG_OPTION_COLOR        = 5,
    CONFIG_OPTION_CHOICE       = 6, /* e.g. "one", "two", "three" */
    CONFIG_OPTION_GRADIENT     = 7,
    CONFIG_OPTION_VECTOR       = 8,
};

enum eConfigOptionFlags : uint8_t {
    CONFIG_OPTION_FLAG_PERCENTAGE = (1 << 0),
};

struct SConfigOptionDescription {

    struct SBoolData {
        bool value = false;
    };

    struct SRangeData {
        int value = 0, min = 0, max = 2;
    };

    struct SFloatData {
        float value = 0, min = 0, max = 100;
    };

    struct SStringData {
        std::string value;
    };

    struct SColorData {
        CHyprColor color;
    };

    struct SChoiceData {
        int         firstIndex = 0;
        std::string choices; // comma-separated
    };

    struct SGradientData {
        std::string gradient;
    };

    struct SVectorData {
        Vector2D vec, min, max;
    };

    std::string       value; // e.g. general:gaps_in
    std::string       description;
    std::string       specialCategory; // if value is special (e.g. device:abc) value will be abc and special device
    bool              specialKey = false;
    eConfigOptionType type       = CONFIG_OPTION_BOOL;
    uint32_t          flags      = 0; // eConfigOptionFlags

    std::string       jsonify() const;

    //
    std::variant<SBoolData, SRangeData, SFloatData, SStringData, SColorData, SChoiceData, SGradientData, SVectorData> data;
};

struct SFirstExecRequest {
    std::string exec      = "";
    bool        withRules = false;
};

struct SFloatCache {
    size_t hash;

    SFloatCache(PHLWINDOW window, bool initial) {
        // Base hash from class/title
        size_t baseHash = initial ? (std::hash<std::string>{}(window->m_initialClass) ^ (std::hash<std::string>{}(window->m_initialTitle) << 1)) :
                                    (std::hash<std::string>{}(window->m_class) ^ (std::hash<std::string>{}(window->m_title) << 1));

        // Use empty string as default tag value
        std::string tagValue = "";
        if (auto xdgTag = window->xdgTag())
            tagValue = xdgTag.value();

        // Combine hashes
        hash = baseHash ^ (std::hash<std::string>{}(tagValue) << 2);
    }

    bool operator==(const SFloatCache& other) const {
        return hash == other.hash;
    }
};

namespace std {
    template <>
    struct hash<SFloatCache> {
        size_t operator()(const SFloatCache& id) const {
            return id.hash;
        }
    };
}

class CMonitorRuleParser {
  public:
    CMonitorRuleParser(const std::string& name);

    const std::string&         name();
    SMonitorRule&              rule();
    std::optional<std::string> getError();
    bool                       parseMode(const std::string& value);
    bool                       parsePosition(const std::string& value, bool isFirst = false);
    bool                       parseScale(const std::string& value);
    bool                       parseTransform(const std::string& value);
    bool                       parseBitdepth(const std::string& value);
    bool                       parseCM(const std::string& value);
    bool                       parseSDRBrightness(const std::string& value);
    bool                       parseSDRSaturation(const std::string& value);
    bool                       parseVRR(const std::string& value);

    void                       setDisabled();
    void                       setMirror(const std::string& value);
    bool                       setReserved(const SMonitorAdditionalReservedArea& value);

  private:
    SMonitorRule m_rule;
    std::string  m_error = "";
};

class CConfigManager {
  public:
    CConfigManager();

    void                                                            init();
    void                                                            reload();
    std::string                                                     verify();

    int                                                             getDeviceInt(const std::string&, const std::string&, const std::string& fallback = "");
    float                                                           getDeviceFloat(const std::string&, const std::string&, const std::string& fallback = "");
    Vector2D                                                        getDeviceVec(const std::string&, const std::string&, const std::string& fallback = "");
    std::string                                                     getDeviceString(const std::string&, const std::string&, const std::string& fallback = "");
    bool                                                            deviceConfigExplicitlySet(const std::string&, const std::string&);
    bool                                                            deviceConfigExists(const std::string&);
    Hyprlang::CConfigValue*                                         getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback);
    bool                                                            shouldBlurLS(const std::string&);

    void* const*                                                    getConfigValuePtr(const std::string&);
    Hyprlang::CConfigValue*                                         getHyprlangConfigValuePtr(const std::string& name, const std::string& specialCat = "");
    std::string                                                     getMainConfigPath();
    std::string                                                     getConfigString();

    SMonitorRule                                                    getMonitorRuleFor(const PHLMONITOR);
    SWorkspaceRule                                                  getWorkspaceRuleFor(PHLWORKSPACE workspace);
    std::string                                                     getDefaultWorkspaceFor(const std::string&);

    PHLMONITOR                                                      getBoundMonitorForWS(const std::string&);
    std::string                                                     getBoundMonitorStringForWS(const std::string&);
    const std::vector<SWorkspaceRule>&                              getAllWorkspaceRules();

    std::vector<SP<CWindowRule>>                                    getMatchingRules(PHLWINDOW, bool dynamic = true, bool shadowExec = false);
    std::vector<SP<CLayerRule>>                                     getMatchingRules(PHLLS);
    void                                                            ensurePersistentWorkspacesPresent();

    const std::vector<SConfigOptionDescription>&                    getAllDescriptions();

    std::unordered_map<std::string, SMonitorAdditionalReservedArea> m_mAdditionalReservedAreas;

    const std::unordered_map<std::string, SP<Hyprutils::Animation::SAnimationPropertyConfig>>& getAnimationConfig();

    void addPluginConfigVar(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value);
    void addPluginKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fun, Hyprlang::SHandlerOptions opts = {});
    void removePluginConfig(HANDLE handle);

    // no-op when done.
    void                                               dispatchExecOnce();
    void                                               dispatchExecShutdown();

    void                                               performMonitorReload();
    void                                               ensureMonitorStatus();
    void                                               ensureVRR(PHLMONITOR pMonitor = nullptr);

    bool                                               shouldUseSoftwareCursors(PHLMONITOR pMonitor);
    void                                               updateWatcher();

    std::string                                        parseKeyword(const std::string&, const std::string&);

    void                                               addParseError(const std::string&);

    SP<Hyprutils::Animation::SAnimationPropertyConfig> getAnimationPropertyConfig(const std::string&);

    void                                               addExecRule(const SExecRequestedRule&);

    void                                               handlePluginLoads();
    std::string                                        getErrors();

    // keywords
    std::optional<std::string> handleRawExec(const std::string&, const std::string&);
    std::optional<std::string> handleExec(const std::string&, const std::string&);
    std::optional<std::string> handleExecOnce(const std::string&, const std::string&);
    std::optional<std::string> handleExecRawOnce(const std::string&, const std::string&);
    std::optional<std::string> handleExecShutdown(const std::string&, const std::string&);
    std::optional<std::string> handleMonitor(const std::string&, const std::string&);
    std::optional<std::string> handleBind(const std::string&, const std::string&);
    std::optional<std::string> handleUnbind(const std::string&, const std::string&);
    std::optional<std::string> handleWindowRule(const std::string&, const std::string&);
    std::optional<std::string> handleLayerRule(const std::string&, const std::string&);
    std::optional<std::string> handleWorkspaceRules(const std::string&, const std::string&);
    std::optional<std::string> handleBezier(const std::string&, const std::string&);
    std::optional<std::string> handleAnimation(const std::string&, const std::string&);
    std::optional<std::string> handleSource(const std::string&, const std::string&);
    std::optional<std::string> handleSubmap(const std::string&, const std::string&);
    std::optional<std::string> handleBlurLS(const std::string&, const std::string&);
    std::optional<std::string> handleBindWS(const std::string&, const std::string&);
    std::optional<std::string> handleEnv(const std::string&, const std::string&);
    std::optional<std::string> handlePlugin(const std::string&, const std::string&);
    std::optional<std::string> handlePermission(const std::string&, const std::string&);
    std::optional<std::string> handleGesture(const std::string&, const std::string&);

    std::optional<std::string> handleMonitorv2(const std::string& output);
    Hyprlang::CParseResult     handleMonitorv2();

    std::string                m_configCurrentPath;

    bool                       m_wantsMonitorReload                  = false;
    bool                       m_noMonitorReload                     = false;
    bool                       m_isLaunchingExecOnce                 = false; // For exec-once to skip initial ws tracking
    bool                       m_lastConfigVerificationWasSuccessful = true;

    void                       storeFloatingSize(PHLWINDOW window, const Vector2D& size);
    std::optional<Vector2D>    getStoredFloatingSize(PHLWINDOW window);

  private:
    UP<Hyprlang::CConfig>                            m_config;

    std::vector<std::string>                         m_configPaths;

    Hyprutils::Animation::CAnimationConfigTree       m_animationTree;

    SSubmap                                          m_currentSubmap;

    std::vector<SExecRequestedRule>                  m_execRequestedRules; // rules requested with exec, e.g. [workspace 2] kitty

    std::vector<std::string>                         m_declaredPlugins;
    std::vector<SPluginKeyword>                      m_pluginKeywords;
    std::vector<SPluginVariable>                     m_pluginVariables;

    bool                                             m_isFirstLaunch = true; // For exec-once

    std::vector<SMonitorRule>                        m_monitorRules;
    std::vector<SWorkspaceRule>                      m_workspaceRules;
    std::vector<SP<CWindowRule>>                     m_windowRules;
    std::vector<SP<CLayerRule>>                      m_layerRules;
    std::vector<std::string>                         m_blurLSNamespaces;

    bool                                             m_firstExecDispatched  = false;
    bool                                             m_manualCrashInitiated = false;

    std::vector<SFirstExecRequest>                   m_firstExecRequests; // bool is for if with rules
    std::vector<std::string>                         m_finalExecRequests;

    std::vector<std::pair<std::string, std::string>> m_failedPluginConfigValues; // for plugin values of unloaded plugins
    std::string                                      m_configErrors = "";

    uint32_t                                         m_configValueNumber = 0;

    // internal methods
    void                                      updateBlurredLS(const std::string&, const bool);
    void                                      setDefaultAnimationVars();
    std::optional<std::string>                resetHLConfig();
    std::optional<std::string>                generateConfig(std::string configPath);
    std::optional<std::string>                verifyConfigExists();

    void                                      postConfigReload(const Hyprlang::CParseResult& result);
    SWorkspaceRule                            mergeWorkspaceRules(const SWorkspaceRule&, const SWorkspaceRule&);

    void                                      registerConfigVar(const char* name, const Hyprlang::INT& val);
    void                                      registerConfigVar(const char* name, const Hyprlang::FLOAT& val);
    void                                      registerConfigVar(const char* name, const Hyprlang::VEC2& val);
    void                                      registerConfigVar(const char* name, const Hyprlang::STRING& val);
    void                                      registerConfigVar(const char* name, Hyprlang::CUSTOMTYPE&& val);

    std::unordered_map<SFloatCache, Vector2D> m_mStoredFloatingSizes;

    friend struct SConfigOptionDescription;
    friend class CMonitorRuleParser;
};

inline UP<CConfigManager> g_pConfigManager;
