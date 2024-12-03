#pragma once

#define CONFIG_MANAGER_H

#include <map>
#include "../debug/Log.hpp"
#include <unordered_map>
#include "../defines.hpp"
#include <variant>
#include <vector>
#include <deque>
#include <algorithm>
#include <regex>
#include <optional>
#include <functional>
#include <xf86drmMode.h>
#include "../helpers/WLClasses.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../desktop/Window.hpp"
#include "../desktop/LayerSurface.hpp"

#include "defaultConfig.hpp"
#include "ConfigDataValues.hpp"

#include <hyprlang.hpp>

#define INITANIMCFG(name)           animationConfig[name] = {}
#define CREATEANIMCFG(name, parent) animationConfig[name] = {false, "", "", 0.f, -1, &animationConfig["global"], &animationConfig[parent]}

#define HANDLE void*

struct SWorkspaceRule {
    std::string                        monitor         = "";
    std::string                        workspaceString = "";
    std::string                        workspaceName   = "";
    WORKSPACEID                        workspaceId     = -1;
    bool                               isDefault       = false;
    bool                               isPersistent    = false;
    std::optional<CCssGapData>         gapsIn;
    std::optional<CCssGapData>         gapsOut;
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

struct SAnimationPropertyConfig {
    bool                      overridden = true;

    std::string               internalBezier  = "";
    std::string               internalStyle   = "";
    float                     internalSpeed   = 0.f;
    int                       internalEnabled = -1;

    SAnimationPropertyConfig* pValues          = nullptr;
    SAnimationPropertyConfig* pParentAnimation = nullptr;
};

struct SPluginKeyword {
    HANDLE                       handle = 0;
    std::string                  name   = "";
    Hyprlang::PCONFIGHANDLERFUNC fn     = nullptr;
};

struct SPluginVariable {
    HANDLE      handle = 0;
    std::string name   = "";
};

struct SExecRequestedRule {
    std::string szRule = "";
    uint64_t    iPid   = 0;
};

enum eConfigOptionType : uint16_t {
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

enum eConfigOptionFlags : uint32_t {
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

class CConfigManager {
  public:
    CConfigManager();

    void                                                            tick();
    void                                                            init();

    int                                                             getDeviceInt(const std::string&, const std::string&, const std::string& fallback = "");
    float                                                           getDeviceFloat(const std::string&, const std::string&, const std::string& fallback = "");
    Vector2D                                                        getDeviceVec(const std::string&, const std::string&, const std::string& fallback = "");
    std::string                                                     getDeviceString(const std::string&, const std::string&, const std::string& fallback = "");
    bool                                                            deviceConfigExists(const std::string&);
    Hyprlang::CConfigValue*                                         getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback);
    bool                                                            shouldBlurLS(const std::string&);

    void* const*                                                    getConfigValuePtr(const std::string&);
    Hyprlang::CConfigValue*                                         getHyprlangConfigValuePtr(const std::string& name, const std::string& specialCat = "");
    void                                                            onPluginLoadUnload(const std::string& name, bool load);
    static std::string                                              getMainConfigPath();
    const std::string                                               getConfigString();

    SMonitorRule                                                    getMonitorRuleFor(const PHLMONITOR);
    SWorkspaceRule                                                  getWorkspaceRuleFor(PHLWORKSPACE workspace);
    std::string                                                     getDefaultWorkspaceFor(const std::string&);

    PHLMONITOR                                                      getBoundMonitorForWS(const std::string&);
    std::string                                                     getBoundMonitorStringForWS(const std::string&);
    const std::deque<SWorkspaceRule>&                               getAllWorkspaceRules();

    std::vector<SWindowRule>                                        getMatchingRules(PHLWINDOW, bool dynamic = true, bool shadowExec = false);
    std::vector<SLayerRule>                                         getMatchingRules(PHLLS);

    const std::vector<SConfigOptionDescription>&                    getAllDescriptions();

    std::unordered_map<std::string, SMonitorAdditionalReservedArea> m_mAdditionalReservedAreas;

    std::unordered_map<std::string, SAnimationPropertyConfig>       getAnimationConfig();

    void                                                            addPluginConfigVar(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value);
    void addPluginKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fun, Hyprlang::SHandlerOptions opts = {});
    void removePluginConfig(HANDLE handle);

    // no-op when done.
    void                      dispatchExecOnce();
    void                      dispatchExecShutdown();

    void                      performMonitorReload();
    void                      appendMonitorRule(const SMonitorRule&);
    bool                      replaceMonitorRule(const SMonitorRule&);
    void                      ensureMonitorStatus();
    void                      ensureVRR(PHLMONITOR pMonitor = nullptr);

    bool                      shouldUseSoftwareCursors();

    std::string               parseKeyword(const std::string&, const std::string&);

    void                      addParseError(const std::string&);

    SAnimationPropertyConfig* getAnimationPropertyConfig(const std::string&);

    void                      addExecRule(const SExecRequestedRule&);

    void                      handlePluginLoads();
    std::string               getErrors();

    // keywords
    std::optional<std::string>                                                              handleRawExec(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleExecOnce(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleExecShutdown(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleMonitor(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleBind(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleUnbind(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleWindowRule(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleLayerRule(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleWindowRuleV2(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleWorkspaceRules(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleBezier(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleAnimation(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleSource(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleSubmap(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleBlurLS(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleBindWS(const std::string&, const std::string&);
    std::optional<std::string>                                                              handleEnv(const std::string&, const std::string&);
    std::optional<std::string>                                                              handlePlugin(const std::string&, const std::string&);

    std::string                                                                             configCurrentPath;

    std::unordered_map<std::string, std::function<CWindowOverridableVar<bool>*(PHLWINDOW)>> mbWindowProperties = {
        {"allowsinput", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.allowsInput; }},
        {"dimaround", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.dimAround; }},
        {"decorate", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.decorate; }},
        {"focusonactivate", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.focusOnActivate; }},
        {"keepaspectratio", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.keepAspectRatio; }},
        {"nearestneighbor", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.nearestNeighbor; }},
        {"noanim", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noAnim; }},
        {"noblur", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noBlur; }},
        {"noborder", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noBorder; }},
        {"nodim", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noDim; }},
        {"nofocus", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noFocus; }},
        {"nomaxsize", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noMaxSize; }},
        {"norounding", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noRounding; }},
        {"noshadow", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noShadow; }},
        {"noshortcutsinhibit", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.noShortcutsInhibit; }},
        {"opaque", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.opaque; }},
        {"forcergbx", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.RGBX; }},
        {"syncfullscreen", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.syncFullscreen; }},
        {"immediate", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.tearing; }},
        {"xray", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.xray; }},
    };

    std::unordered_map<std::string, std::function<CWindowOverridableVar<int>*(PHLWINDOW)>> miWindowProperties = {
        {"rounding", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.rounding; }}, {"bordersize", [](PHLWINDOW pWindow) { return &pWindow->m_sWindowData.borderSize; }}};

    bool m_bWantsMonitorReload = false;
    bool m_bForceReload        = false;
    bool m_bNoMonitorReload    = false;
    bool isLaunchingExecOnce   = false; // For exec-once to skip initial ws tracking

  private:
    std::unique_ptr<Hyprlang::CConfig>                        m_pConfig;

    std::deque<std::string>                                   configPaths;       // stores all the config paths
    std::unordered_map<std::string, time_t>                   configModifyTimes; // stores modify times

    std::unordered_map<std::string, SAnimationPropertyConfig> animationConfig; // stores all the animations with their set values

    std::string                                               m_szCurrentSubmap = ""; // For storing the current keybind submap

    std::vector<SExecRequestedRule>                           execRequestedRules; // rules requested with exec, e.g. [workspace 2] kitty

    std::vector<std::string>                                  m_vDeclaredPlugins;
    std::vector<SPluginKeyword>                               pluginKeywords;
    std::vector<SPluginVariable>                              pluginVariables;

    bool                                                      isFirstLaunch = true; // For exec-once

    std::deque<SMonitorRule>                                  m_dMonitorRules;
    std::deque<SWorkspaceRule>                                m_dWorkspaceRules;
    std::deque<SWindowRule>                                   m_dWindowRules;
    std::deque<SLayerRule>                                    m_dLayerRules;
    std::deque<std::string>                                   m_dBlurLSNamespaces;

    bool                                                      firstExecDispatched     = false;
    bool                                                      m_bManualCrashInitiated = false;
    std::deque<std::string>                                   firstExecRequests;
    std::deque<std::string>                                   finalExecRequests;

    std::vector<std::pair<std::string, std::string>>          m_vFailedPluginConfigValues; // for plugin values of unloaded plugins
    std::string                                               m_szConfigErrors = "";

    // internal methods
    void                              setAnimForChildren(SAnimationPropertyConfig* const);
    void                              updateBlurredLS(const std::string&, const bool);
    void                              setDefaultAnimationVars();
    std::optional<std::string>        resetHLConfig();
    static std::optional<std::string> generateConfig(std::string configPath);
    static std::optional<std::string> verifyConfigExists();
    void                              postConfigReload(const Hyprlang::CParseResult& result);
    void                              reload();
    SWorkspaceRule                    mergeWorkspaceRules(const SWorkspaceRule&, const SWorkspaceRule&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;
