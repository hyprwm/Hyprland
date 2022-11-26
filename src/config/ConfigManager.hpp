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
#include "ConfigDataValues.hpp"

#define STRVAL_EMPTY "[[EMPTY]]"

#define INITANIMCFG(name)  animationConfig[name] = {}
#define CREATEANIMCFG(name, parent) animationConfig[name] = {false, "", "", 0.f, -1, &animationConfig["global"], &animationConfig[parent]}

struct SConfigValue {
    int64_t intValue = -INT64_MAX;
    float floatValue = -__FLT_MAX__;
    std::string strValue = "";
    Vector2D vecValue = Vector2D(-__FLT_MAX__, -__FLT_MAX__);
    std::shared_ptr<ICustomConfigValueData> data;

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
    std::string mirrorOf = "";
    bool        enable10bit = false;
};

struct SMonitorAdditionalReservedArea {
    int         top = 0;
    int         bottom = 0;
    int         left = 0;
    int         right = 0;
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

struct SExecRequestedRule {
    std::string     szRule = "";
    uint64_t        iPid = 0;
};

class CVarList {
public:
    CVarList(const std::string& in, long unsigned int lastArgNo = 0, const char separator = ',') {
        std::string curitem = "";
        std::string argZ = in;

        auto nextItem = [&]() {
            auto idx = lastArgNo != 0 && m_vArgs.size() >= lastArgNo - 1 ? std::string::npos : argZ.find_first_of(separator);

            if (idx != std::string::npos) {
                curitem = argZ.substr(0, idx);
                argZ = argZ.substr(idx + 1);
            } else {
                curitem = argZ;
                argZ = STRVAL_EMPTY;
            }
        };

        nextItem();

        while (curitem != STRVAL_EMPTY) {
            m_vArgs.push_back(removeBeginEndSpacesTabs(curitem));
            nextItem();
        }
    };

    ~CVarList() = default;

    int size() const {
        return m_vArgs.size();
    }

    std::string operator[](const long unsigned int& idx) const {
        if (idx >= m_vArgs.size())
            return "";
        return m_vArgs[idx];
    }

    // for range-based loops
    std::vector<std::string>::iterator begin() { return m_vArgs.begin(); }
    std::vector<std::string>::const_iterator begin() const { return m_vArgs.begin(); }
    std::vector<std::string>::iterator end() { return m_vArgs.end(); }
    std::vector<std::string>::const_iterator end() const { return m_vArgs.end(); }

  private:
    std::vector<std::string> m_vArgs;
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
    SConfigValue*       getConfigValuePtrSafe(std::string);

    SMonitorRule        getMonitorRuleFor(std::string, std::string displayName = "");

    CMonitor*           getBoundMonitorForWS(std::string);

    std::vector<SWindowRule> getMatchingRules(CWindow*);

    std::unordered_map<std::string, SMonitorAdditionalReservedArea> m_mAdditionalReservedAreas;

    // no-op when done.
    void                dispatchExecOnce();

    void                performMonitorReload();
    bool                m_bWantsMonitorReload = false;
    bool                m_bForceReload = false;
    bool                m_bNoMonitorReload = false;
    void                ensureDPMS();
    void                ensureVRR(CMonitor* pMonitor = nullptr);

    std::string         parseKeyword(const std::string&, const std::string&, bool dynamic = false);

    void                addParseError(const std::string&);

    SAnimationPropertyConfig* getAnimationPropertyConfig(const std::string&);

    void                addExecRule(SExecRequestedRule);

    std::string                                   configCurrentPath;

private:
    std::deque<std::string>                       configPaths; // stores all the config paths
    std::unordered_map<std::string, time_t>       configModifyTimes; // stores modify times
    std::unordered_map<std::string, std::string>  configDynamicVars; // stores dynamic vars declared by the user
    std::unordered_map<std::string, SConfigValue> configValues;
    std::unordered_map<std::string, std::unordered_map<std::string, SConfigValue>> deviceConfigs; // stores device configs

    std::unordered_map<std::string, SAnimationPropertyConfig> animationConfig; // stores all the animations with their set values

    std::string currentCategory = "";  // For storing the category of the current item

    std::string parseError = "";  // For storing a parse error to display later

    std::string m_szCurrentSubmap = ""; // For storing the current keybind submap

    std::vector<std::pair<std::string, std::string>> boundWorkspaces;

    std::vector<SExecRequestedRule>               execRequestedRules; // rules requested with exec, e.g. [workspace 2] kitty

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
    void                handleWindowRuleV2(const std::string&, const std::string&);
    void                handleDefaultWorkspace(const std::string&, const std::string&);
    void                handleBezier(const std::string&, const std::string&);
    void                handleAnimation(const std::string&, const std::string&);
    void                handleSource(const std::string&, const std::string&);
    void                handleSubmap(const std::string&, const std::string&);
    void                handleBlurLS(const std::string&, const std::string&);
    void                handleBindWS(const std::string&, const std::string&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;
