#pragma once

#define CONFIG_MANAGER_H

#include <map>
#include "../debug/Log.hpp"
#include <unordered_map>
#include "../defines.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <regex>
#include "../Window.hpp"

#include "defaultConfig.hpp"

#define STRVAL_EMPTY "[[EMPTY]]"

#define INITANIMCFG(name)  animationConfig[name] = {}
#define CREATEANIMCFG(name, parent) animationConfig[name] = {false, "", "", 0.f, -1, &animationConfig["global"], &animationConfig[parent]}

struct SConfigValue {
    int64_t intValue = -1;
    float floatValue = -1;
    std::string strValue = "";

    bool set = false; // used for device configs
};

struct SMonitorRule {
    std::string name = "";
    Vector2D    resolution = Vector2D(1280,720);
    Vector2D    offset = Vector2D(0,0);
    float       scale = 1;
    float       refreshRate = 60;
    std::string defaultWorkspace = "";
    bool        disabled = false;
    wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;
};

struct SMonitorAdditionalReservedArea {
    int         top = 0;
    int         bottom = 0;
    int         left = 0;
    int         right = 0;
};

struct SWindowRule {
    std::string szRule;
    std::string szValue;
};

struct SAnimationPropertyConfig {
    bool            overriden = true;

    std::string     internalBezier = "";
    std::string     internalStyle = "";
    float           internalSpeed = 0.f;
    int             internalEnabled = -1;

    SAnimationPropertyConfig* pValues = nullptr;
    SAnimationPropertyConfig* pParentAnimation = nullptr;
};

class CConfigManager {
public:
    CConfigManager();

    void                tick();
    void                init();

    int                 getInt(const std::string&);
    float               getFloat(const std::string&);
    std::string         getString(const std::string&);
    void                setFloat(std::string, float);
    void                setInt(std::string, int);
    void                setString(std::string, std::string);

    int                 getDeviceInt(const std::string&, const std::string&);
    float               getDeviceFloat(const std::string&, const std::string&);
    std::string         getDeviceString(const std::string&, const std::string&);
    bool                deviceConfigExists(const std::string&);
    bool                shouldBlurLS(const std::string&);

    SConfigValue*       getConfigValuePtr(std::string);

    SMonitorRule        getMonitorRuleFor(std::string);

    std::vector<SWindowRule> getMatchingRules(CWindow*);

    std::unordered_map<std::string, SMonitorAdditionalReservedArea> m_mAdditionalReservedAreas;

    // no-op when done.
    void                dispatchExecOnce();

    void                performMonitorReload();
    bool                m_bWantsMonitorReload = false;
    bool                m_bForceReload = false;
    void                ensureDPMS();

    std::string         parseKeyword(const std::string&, const std::string&, bool dynamic = false);

    void                addParseError(const std::string&);

    SAnimationPropertyConfig* getAnimationPropertyConfig(const std::string&);

private:
    std::deque<std::string>                       configPaths; // stores all the config paths
    std::unordered_map<std::string, time_t>       configModifyTimes; // stores modify times
    std::unordered_map<std::string, std::string>  configDynamicVars; // stores dynamic vars declared by the user
    std::unordered_map<std::string, SConfigValue> configValues;
    std::unordered_map<std::string, std::unordered_map<std::string, SConfigValue>> deviceConfigs; // stores device configs

    std::unordered_map<std::string, SAnimationPropertyConfig> animationConfig; // stores all the animations with their set values

    std::string                                   configCurrentPath;

    std::string currentCategory = "";  // For storing the category of the current item

    std::string parseError = "";  // For storing a parse error to display later

    std::string m_szCurrentSubmap = ""; // For storing the current keybind submap

    bool isFirstLaunch = true;  // For exec-once

    std::deque<SMonitorRule> m_dMonitorRules;
    std::deque<SWindowRule> m_dWindowRules;
    std::deque<std::string> m_dBlurLSNamespaces;

    bool firstExecDispatched = false;
    std::deque<std::string> firstExecRequests;

    // internal methods
    void                setDefaultVars();
    void                setDefaultAnimationVars();
    void                setDeviceDefaultVars(const std::string&);

    void                setAnimForChildren(SAnimationPropertyConfig *const);

    void                applyUserDefinedVars(std::string&, const size_t);
    void                loadConfigLoadVars();
    SConfigValue        getConfigValueSafe(const std::string&);
    SConfigValue        getConfigValueSafeDevice(const std::string&, const std::string&);
    void                parseLine(std::string&);
    void                configSetValueSafe(const std::string&, const std::string&);
    void                handleDeviceConfig(const std::string&, const std::string&);
    void                handleRawExec(const std::string&, const std::string&);
    void                handleMonitor(const std::string&, const std::string&);
    void                handleBind(const std::string&, const std::string&);
    void                handleUnbind(const std::string&, const std::string&);
    void                handleWindowRule(const std::string&, const std::string&);
    void                handleDefaultWorkspace(const std::string&, const std::string&);
    void                handleBezier(const std::string&, const std::string&);
    void                handleAnimation(const std::string&, const std::string&);
    void                handleSource(const std::string&, const std::string&);
    void                handleSubmap(const std::string&, const std::string&);
    void                handleBlurLS(const std::string&, const std::string&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;