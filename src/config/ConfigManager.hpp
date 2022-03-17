#pragma once

#include <map>
#include "../debug/Log.hpp"
#include <unordered_map>
#include "../defines.hpp"
#include <vector>

struct SConfigValue {
    int64_t intValue = -1;
    float floatValue = -1;
    std::string strValue = "";
};

class CConfigManager {
public:
    CConfigManager();

    void                tick();

    int                 getInt(std::string);
    float               getFloat(std::string);
    std::string         getString(std::string);

private:
    std::unordered_map<std::string, SConfigValue> configValues;
    time_t lastModifyTime = 0;  // for reloading the config if changed

    std::string currentCategory = "";  // For storing the category of the current item

    std::string parseError = "";  // For storing a parse error to display later

    bool isFirstLaunch = true;  // For exec-once

    // internal methods
    void                loadConfigLoadVars();
    SConfigValue        getConfigValueSafe(std::string);
    void                parseLine(std::string&);
    void                configSetValueSafe(const std::string&, const std::string&);
    void                handleRawExec(const std::string&, const std::string&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;