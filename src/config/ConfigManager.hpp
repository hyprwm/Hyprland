#pragma once

#include <map>
#include "../debug/Log.hpp"
#include <unordered_map>
#include "../defines.hpp"
#include <vector>
#include <deque>

struct SConfigValue {
    int64_t intValue = -1;
    float floatValue = -1;
    std::string strValue = "";
};

struct SMonitorRule {
    std::string name = "";
    Vector2D    resolution = Vector2D(1280,720);
    Vector2D    offset = Vector2D(0,0);
    float       mfact = 0.5;
    float       scale = 1;
};

class CConfigManager {
public:
    CConfigManager();

    void                tick();
    void                init();

    int                 getInt(std::string);
    float               getFloat(std::string);
    std::string         getString(std::string);

    SMonitorRule        getMonitorRuleFor(std::string);

private:
    std::unordered_map<std::string, SConfigValue> configValues;
    time_t lastModifyTime = 0;  // for reloading the config if changed

    std::string currentCategory = "";  // For storing the category of the current item

    std::string parseError = "";  // For storing a parse error to display later

    bool isFirstLaunch = true;  // For exec-once

    std::deque<SMonitorRule> m_dMonitorRules;

    // internal methods
    void                loadConfigLoadVars();
    SConfigValue        getConfigValueSafe(std::string);
    void                parseLine(std::string&);
    void                configSetValueSafe(const std::string&, const std::string&);
    void                handleRawExec(const std::string&, const std::string&);
    void                handleMonitor(const std::string&, const std::string&);
    void                handleBind(const std::string&, const std::string&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;