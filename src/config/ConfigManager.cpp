#include "ConfigManager.hpp"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>

CConfigManager::CConfigManager() {
    configValues["general:max_fps"].intValue = 240;

    configValues["general:border_size"].intValue = 1;
    configValues["general:gaps_in"].intValue = 5;
    configValues["general:gaps_out"].intValue = 20;
    configValues["general:col.active_border"].intValue = 0xffffffff;
    configValues["general:col.inactive_border"].intValue = 0xff444444;
}

void CConfigManager::init() {
    loadConfigLoadVars();

    isFirstLaunch = false;
}

void CConfigManager::configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (configValues.find(COMMAND) == configValues.end()) {
        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
        return;
    }

    auto& CONFIGENTRY = configValues.at(COMMAND);
    if (CONFIGENTRY.intValue != -1) {
        try {
            if (VALUE.find("0x") == 0) {
                // Values with 0x are hex
                const auto VALUEWITHOUTHEX = VALUE.substr(2);
                CONFIGENTRY.intValue = stol(VALUEWITHOUTHEX, nullptr, 16);
            } else
                CONFIGENTRY.intValue = stol(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.floatValue != -1) {
        try {
            CONFIGENTRY.floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.strValue != "") {
        try {
            CONFIGENTRY.strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    }
}

void CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
}

void CConfigManager::handleMonitor(const std::string& command, const std::string& args) {

    // get the monitor config
    SMonitorRule newrule;

    std::string curitem = "";

    std::string argZ = args;

    auto nextItem = [&]() {
        auto idx = argZ.find_first_of(',');

        if (idx != std::string::npos) {
            curitem = argZ.substr(0, idx);
            argZ = argZ.substr(idx + 1);
        } else {
            argZ = "";
            curitem = argZ;
        }
    };

    nextItem();

    newrule.name = curitem;

    nextItem();

    newrule.resolution.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
    newrule.resolution.y = stoi(curitem.substr(curitem.find_first_of('x') + 1));

    nextItem();

    newrule.offset.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
    newrule.offset.y = stoi(curitem.substr(curitem.find_first_of('x') + 1));

    nextItem();

    newrule.mfact = stof(curitem);

    nextItem();

    newrule.scale = stof(curitem);

    m_dMonitorRules.push_back(newrule);
}

void CConfigManager::parseLine(std::string& line) {
    // first check if its not a comment
    const auto COMMENTSTART = line.find_first_of('#');
    if (COMMENTSTART == 0)
        return;

    // now, cut the comment off
    if (COMMENTSTART != std::string::npos)
        line = line.substr(0, COMMENTSTART);

    // remove shit at the beginning
    while (line[0] == ' ' || line[0] == '\t') {
        line = line.substr(1);
    }

    if (line.find(" {") != std::string::npos) {
        auto cat = line.substr(0, line.find(" {"));
        transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
        currentCategory = cat;
        return;
    }

    if (line.find("}") != std::string::npos && currentCategory != "") {
        currentCategory = "";
        return;
    }

    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = line.substr(0, EQUALSPLACE);
    const auto VALUE = line.substr(EQUALSPLACE + 1);

    if (COMMAND == "exec") {
        handleRawExec(COMMAND, VALUE);
        return;
    } else if (COMMAND == "exec-once") {
        if (isFirstLaunch) {
            handleRawExec(COMMAND, VALUE);
            return;
        }
    } else if (COMMAND == "monitor") {
        handleMonitor(COMMAND, VALUE);
        return;
    }

    configSetValueSafe(currentCategory + (currentCategory == "" ? "" : ":") + COMMAND, VALUE);
}

void CConfigManager::loadConfigLoadVars() {
    Debug::log(LOG, "Reloading the config!");
    parseError = "";       // reset the error
    currentCategory = "";  // reset the category

    m_dMonitorRules.clear();

    const char* const ENVHOME = getenv("HOME");
    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    std::ifstream ifs;
    ifs.open(CONFIGPATH.c_str());

    if (!ifs.good()) {
        Debug::log(WARN, "Config reading error. (No file?)");
        parseError = "The config could not be read. (No file?)";

        ifs.close();
        return;
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError = "Config error at line " + std::to_string(linenum) + ": Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + ": " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }
}

void CConfigManager::tick() {
    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config, error %i", errno);
    }

    // check if we need to reload cfg
    if (fileStat.st_mtime != lastModifyTime) {
        lastModifyTime = fileStat.st_mtime;

        loadConfigLoadVars();
    }
}

std::mutex configmtx;
SConfigValue CConfigManager::getConfigValueSafe(std::string val) {
    std::lock_guard<std::mutex> lg(configmtx);

    SConfigValue copy = configValues[val];

    return copy;
}

int CConfigManager::getInt(std::string v) {
    return getConfigValueSafe(v).intValue;
}

float CConfigManager::getFloat(std::string v) {
    return getConfigValueSafe(v).floatValue;
}

std::string CConfigManager::getString(std::string v) {
    return getConfigValueSafe(v).strValue;
}

SMonitorRule CConfigManager::getMonitorRuleFor(std::string name) {
    SMonitorRule* found = nullptr;

    for (auto& r : m_dMonitorRules) {
        if (r.name == name) {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    for (auto& r : m_dMonitorRules) {
        if (r.name == "") {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    return SMonitorRule{.name = "", .resolution = Vector2D(1280, 720), .offset = Vector2D(0, 0), .mfact = 0.5f, .scale = 1};
}