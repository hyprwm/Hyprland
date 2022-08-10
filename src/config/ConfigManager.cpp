#include "ConfigManager.hpp"
#include "../managers/KeybindManager.hpp"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>

CConfigManager::CConfigManager() {
    setDefaultVars();
    setDefaultAnimationVars();

    std::string CONFIGPATH;
    if (g_pCompositor->explicitConfigPath == "") {
        static const char* const ENVHOME = getenv("HOME");
        CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");
    } else {
        CONFIGPATH = g_pCompositor->explicitConfigPath;
    }

    configPaths.emplace_back(CONFIGPATH);

    Debug::disableLogs = &configValues["debug:disable_logs"].intValue;
    Debug::disableTime = &configValues["debug:disable_time"].intValue;
}

void CConfigManager::setDefaultVars() {
    configValues["general:max_fps"].intValue = 60;
    configValues["general:sensitivity"].floatValue = 1.0f;
    configValues["general:apply_sens_to_raw"].intValue = 0;
    configValues["general:main_mod"].strValue = "SUPER";                                               // exposed to the user for easier configuring
    configValues["general:main_mod_internal"].intValue = g_pKeybindManager->stringToModMask("SUPER");  // actually used and automatically calculated

    configValues["general:damage_tracking"].strValue = "full";
    configValues["general:damage_tracking_internal"].intValue = DAMAGE_TRACKING_FULL;

    configValues["general:border_size"].intValue = 1;
    configValues["general:no_border_on_floating"].intValue = 0;
    configValues["general:gaps_in"].intValue = 5;
    configValues["general:gaps_out"].intValue = 20;
    configValues["general:col.active_border"].intValue = 0xffffffff;
    configValues["general:col.inactive_border"].intValue = 0xff444444;
    configValues["general:cursor_inactive_timeout"].intValue = 0;
    configValues["general:no_cursor_warps"].intValue = 0;

    configValues["general:layout"].strValue = "dwindle";
    
    configValues["misc:disable_hyprland_logo"].intValue = 0;
    configValues["misc:disable_splash_rendering"].intValue = 0;
    configValues["misc:no_vfr"].intValue = 1;
    configValues["misc:damage_entire_on_snapshot"].intValue = 0;
    configValues["misc:mouse_move_enables_dpms"].intValue = 0;
    configValues["misc:always_follow_on_dnd"].intValue = 1;
    configValues["misc:layers_hog_keyboard_focus"].intValue = 0;

    configValues["debug:int"].intValue = 0;
    configValues["debug:log_damage"].intValue = 0;
    configValues["debug:overlay"].intValue = 0;
    configValues["debug:damage_blink"].intValue = 0;
    configValues["debug:disable_logs"].intValue = 0;
    configValues["debug:disable_time"].intValue = 1;

    configValues["decoration:rounding"].intValue = 1;
    configValues["decoration:blur"].intValue = 1;
    configValues["decoration:blur_size"].intValue = 8;
    configValues["decoration:blur_passes"].intValue = 1;
    configValues["decoration:blur_ignore_opacity"].intValue = 0;
    configValues["decoration:blur_new_optimizations"].intValue = 0;
    configValues["decoration:active_opacity"].floatValue = 1;
    configValues["decoration:inactive_opacity"].floatValue = 1;
    configValues["decoration:fullscreen_opacity"].floatValue = 1;
    configValues["decoration:multisample_edges"].intValue = 1;
    configValues["decoration:no_blur_on_oversized"].intValue = 0;
    configValues["decoration:drop_shadow"].intValue = 1;
    configValues["decoration:shadow_range"].intValue = 4;
    configValues["decoration:shadow_render_power"].intValue = 3;
    configValues["decoration:shadow_ignore_window"].intValue = 1;
    configValues["decoration:shadow_offset"].strValue = "0 0";
    configValues["decoration:col.shadow"].intValue = 0xee1a1a1a;
    configValues["decoration:col.shadow_inactive"].intValue = INT_MAX;

    configValues["dwindle:pseudotile"].intValue = 0;
    configValues["dwindle:col.group_border"].intValue = 0x66777700;
    configValues["dwindle:col.group_border_active"].intValue = 0x66ffff00;
    configValues["dwindle:force_split"].intValue = 0;
    configValues["dwindle:preserve_split"].intValue = 0;
    configValues["dwindle:special_scale_factor"].floatValue = 0.8f;
    configValues["dwindle:split_width_multiplier"].floatValue = 1.0f;
    configValues["dwindle:no_gaps_when_only"].intValue = 0;

    configValues["master:special_scale_factor"].floatValue = 0.8f;
    configValues["master:new_is_master"].intValue = 1;
    configValues["master:new_on_top"].intValue = 0;
    configValues["master:no_gaps_when_only"].intValue = 0;

    configValues["animations:enabled"].intValue = 1;
    configValues["animations:speed"].floatValue = 7.f;
    configValues["animations:curve"].strValue = "default";
    configValues["animations:windows_style"].strValue = STRVAL_EMPTY;
    configValues["animations:windows_curve"].strValue = "[[f]]";
    configValues["animations:windows_speed"].floatValue = 0.f;
    configValues["animations:windows"].intValue = 1;
    configValues["animations:borders_style"].strValue = STRVAL_EMPTY;
    configValues["animations:borders_curve"].strValue = "[[f]]";
    configValues["animations:borders_speed"].floatValue = 0.f;
    configValues["animations:borders"].intValue = 1;
    configValues["animations:fadein_style"].strValue = STRVAL_EMPTY;
    configValues["animations:fadein_curve"].strValue = "[[f]]";
    configValues["animations:fadein_speed"].floatValue = 0.f;
    configValues["animations:fadein"].intValue = 1;
    configValues["animations:workspaces_style"].strValue = STRVAL_EMPTY;
    configValues["animations:workspaces_curve"].strValue = "[[f]]";
    configValues["animations:workspaces_speed"].floatValue = 0.f;
    configValues["animations:workspaces"].intValue = 1;

    configValues["input:sensitivity"].floatValue = 0.f;
    configValues["input:kb_layout"].strValue = "us";
    configValues["input:kb_variant"].strValue = STRVAL_EMPTY;
    configValues["input:kb_options"].strValue = STRVAL_EMPTY;
    configValues["input:kb_rules"].strValue = STRVAL_EMPTY;
    configValues["input:kb_model"].strValue = STRVAL_EMPTY;
    configValues["input:repeat_rate"].intValue = 25;
    configValues["input:repeat_delay"].intValue = 600;
    configValues["input:natural_scroll"].intValue = 0;
    configValues["input:numlock_by_default"].intValue = 0;
    configValues["input:force_no_accel"].intValue = 0;
    configValues["input:touchpad:natural_scroll"].intValue = 0;
    configValues["input:touchpad:disable_while_typing"].intValue = 1;
    configValues["input:touchpad:clickfinger_behavior"].intValue = 0;
    configValues["input:touchpad:middle_button_emulation"].intValue = 0;
    configValues["input:touchpad:tap-to-click"].intValue = 1;
    configValues["input:touchpad:drag_lock"].intValue = 0;

    configValues["binds:pass_mouse_when_bound"].intValue = 1;
    configValues["binds:scroll_event_delay"].intValue = 300;

    configValues["gestures:workspace_swipe"].intValue = 0;
    configValues["gestures:workspace_swipe_fingers"].intValue = 3;
    configValues["gestures:workspace_swipe_distance"].intValue = 300;
    configValues["gestures:workspace_swipe_invert"].intValue = 1;
    configValues["gestures:workspace_swipe_min_speed_to_force"].intValue = 30;
    configValues["gestures:workspace_swipe_cancel_ratio"].floatValue = 0.5f;

    configValues["input:follow_mouse"].intValue = 1;

    configValues["autogenerated"].intValue = 0;
}

void CConfigManager::setDeviceDefaultVars(const std::string& dev) {
    auto& cfgValues = deviceConfigs[dev];

    cfgValues["sensitivity"].floatValue = 0.f;
    cfgValues["kb_layout"].strValue = "us";
    cfgValues["kb_variant"].strValue = STRVAL_EMPTY;
    cfgValues["kb_options"].strValue = STRVAL_EMPTY;
    cfgValues["kb_rules"].strValue = STRVAL_EMPTY;
    cfgValues["kb_model"].strValue = STRVAL_EMPTY;
    cfgValues["repeat_rate"].intValue = 25;
    cfgValues["repeat_delay"].intValue = 600;
    cfgValues["natural_scroll"].intValue = 0;
    cfgValues["numlock_by_default"].intValue = 0;
    cfgValues["disable_while_typing"].intValue = 1;
    cfgValues["clickfinger_behavior"].intValue = 0;
    cfgValues["middle_button_emulation"].intValue = 0;
    cfgValues["tap-to-click"].intValue = 1;
    cfgValues["drag_lock"].intValue = 0;
}

void CConfigManager::setDefaultAnimationVars() {
    if (isFirstLaunch) {
        INITANIMCFG("global");
        INITANIMCFG("windows");
        INITANIMCFG("fade");
        INITANIMCFG("border");
        INITANIMCFG("workspaces");

        // windows
        INITANIMCFG("windowsIn");
        INITANIMCFG("windowsOut");
        INITANIMCFG("windowsMove");

        // fade
        INITANIMCFG("fadeIn");
        INITANIMCFG("fadeOut");
        INITANIMCFG("fadeSwitch");
        INITANIMCFG("fadeShadow");

        // border

        // workspaces
    }
    
    // init the values
    animationConfig["global"] = {
        false,
        "default",
        "",
        8.f,
        1,
        &animationConfig["general"],
        nullptr
    };

    CREATEANIMCFG("windows", "global");
    CREATEANIMCFG("fade", "global");
    CREATEANIMCFG("border", "global");
    CREATEANIMCFG("workspaces", "global");

    CREATEANIMCFG("windowsIn", "windows");
    CREATEANIMCFG("windowsOut", "windows");
    CREATEANIMCFG("windowsMove", "windows");

    CREATEANIMCFG("fadeIn", "fade");
    CREATEANIMCFG("fadeOut", "fade");
    CREATEANIMCFG("fadeSwitch", "fade");
    CREATEANIMCFG("fadeShadow", "fade");
}

void CConfigManager::init() {
    
    loadConfigLoadVars();

    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at statting config, error %i", errno);
    }

    configModifyTimes[CONFIGPATH] = fileStat.st_mtime;

    isFirstLaunch = false;
}

void CConfigManager::configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (configValues.find(COMMAND) == configValues.end()) {
        if (COMMAND.find("device:") != 0 /* devices parsed later */) {
            if (COMMAND[0] == '$') {
                // register a dynamic var
                Debug::log(LOG, "Registered dynamic var \"%s\" -> %s", COMMAND.c_str(), VALUE.c_str());
                configDynamicVars[COMMAND.substr(1)] = VALUE;
            } else {
                parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
            }

            return;
        }
    }

    SConfigValue* CONFIGENTRY = nullptr;

    if (COMMAND.find("device:") == 0) {
        const auto DEVICE = COMMAND.substr(7).substr(0, COMMAND.find_last_of(':') - 7);
        const auto CONFIGVAR = COMMAND.substr(COMMAND.find_last_of(':') + 1);

        if (!deviceConfigExists(DEVICE))
            setDeviceDefaultVars(DEVICE);

        auto it = deviceConfigs.find(DEVICE);

        if (it->second.find(CONFIGVAR) == it->second.end()) {
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
            return;
        }

        CONFIGENTRY = &it->second.at(CONFIGVAR);
    } else {
        CONFIGENTRY = &configValues.at(COMMAND);
    }

    CONFIGENTRY->set = true;

    if (CONFIGENTRY->intValue != -1) {
        try {
            if (VALUE.find("0x") == 0) {
                // Values with 0x are hex
                const auto VALUEWITHOUTHEX = VALUE.substr(2);
                CONFIGENTRY->intValue = stol(VALUEWITHOUTHEX, nullptr, 16);
            } else if (VALUE.find("true") == 0 || VALUE.find("on") == 0 || VALUE.find("yes") == 0) {
                CONFIGENTRY->intValue = 1;
            } else if (VALUE.find("false") == 0 || VALUE.find("off") == 0 || VALUE.find("no") == 0) {
                CONFIGENTRY->intValue = 0;
            }
            else
                CONFIGENTRY->intValue = stol(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY->floatValue != -1) {
        try {
            CONFIGENTRY->floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY->strValue != "") {
        try {
            CONFIGENTRY->strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    }
}

void CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.

    std::string toExec = args;

    if (g_pXWaylandManager->m_sWLRXWayland)
        toExec = std::string("WAYLAND_DISPLAY=") + std::string(g_pCompositor->m_szWLDisplaySocket) + " DISPLAY=" + std::string(g_pXWaylandManager->m_sWLRXWayland->display_name) + " " + toExec;
    else
        toExec = std::string("WAYLAND_DISPLAY=") + std::string(g_pCompositor->m_szWLDisplaySocket) + " " + toExec;

    Debug::log(LOG, "Config executing %s", toExec.c_str());

    int socket[2];
    if (pipe(socket) != 0) {
        Debug::log(LOG, "Unable to create pipe for fork");
    }

    pid_t child, grandchild;
    child = fork();
    if (child < 0) {
        close(socket[0]);
        close(socket[1]);
        Debug::log(LOG, "Fail to create the first fork");
        return;
    }
    if (child == 0) {
        // run in child
        grandchild = fork();
        if (grandchild == 0) {
            // run in grandchild
            close(socket[0]);
            close(socket[1]);
            execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
            // exit grandchild
            _exit(0);
        }
        close(socket[0]);
        write(socket[1], &grandchild, sizeof(grandchild));
        close(socket[1]);
        // exit child
        _exit(0);
    }
    // run in parent
    close(socket[1]);
    read(socket[0], &grandchild, sizeof(grandchild));
    close(socket[0]);
    // clear child and leave child to init
    waitpid(child, NULL, 0);
    if (child < 0) {
        Debug::log(LOG, "Fail to create the second fork");
        return;
    }
    Debug::log(LOG, "Process created with pid %d", grandchild);
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
            curitem = argZ;
            argZ = "";
        }
    };

    nextItem();

    newrule.name = curitem;

    nextItem();

    if (curitem == "disable" || curitem == "disabled" || curitem == "addreserved" || curitem == "transform") {
        if (curitem == "disable" || curitem == "disabled")
            newrule.disabled = true;
        else if (curitem == "transform") {
            nextItem();

            wl_output_transform transform = (wl_output_transform)std::stoi(curitem);

            // overwrite if exists
            for (auto& r : m_dMonitorRules) {
                if (r.name == newrule.name) {
                    r.transform = transform;
                    return;
                }
            }

            return;
        } else if (curitem == "addreserved") {
            nextItem();

            int top = std::stoi(curitem);

            nextItem();

            int bottom = std::stoi(curitem);

            nextItem();

            int left = std::stoi(curitem);

            nextItem();

            int right = std::stoi(curitem);

            m_mAdditionalReservedAreas[newrule.name] = {top, bottom, left, right};

            return; // this is not a rule, ignore
        } else {
            Debug::log(ERR, "ConfigManager parseMonitor, curitem bogus???");
            return;
        }

        if (std::find_if(m_dMonitorRules.begin(), m_dMonitorRules.end(), [&](const auto& other) { return other.name == newrule.name; }) != m_dMonitorRules.end()) 
            m_dMonitorRules.erase(std::remove_if(m_dMonitorRules.begin(), m_dMonitorRules.end(), [&](const auto& other) { return other.name == newrule.name; }));

        m_dMonitorRules.push_back(newrule);

        return;
    }

    if (curitem.find("pref") == 0) {
        newrule.resolution = Vector2D();
    } else {
        newrule.resolution.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
        newrule.resolution.y = stoi(curitem.substr(curitem.find_first_of('x') + 1, curitem.find_first_of('@')));

        if (curitem.contains("@"))
            newrule.refreshRate = stof(curitem.substr(curitem.find_first_of('@') + 1));
    }

    nextItem();

    if (curitem.find("auto") == 0) {
        newrule.offset = Vector2D(-1, -1);
    } else {
        newrule.offset.x = stoi(curitem.substr(0, curitem.find_first_of('x')));
        newrule.offset.y = stoi(curitem.substr(curitem.find_first_of('x') + 1));

        if (newrule.offset.x < 0 || newrule.offset.y < 0) {
            parseError = "invalid offset. Offset cannot be negative.";
            newrule.offset = Vector2D();
        }
    }

    nextItem();

    newrule.scale = stof(curitem);

    if (newrule.scale < 0.25f) {
        parseError = "not a valid scale.";
        newrule.scale = 1;
    }

    nextItem();

    if (curitem != "") {
        // warning for old cfg
        Debug::log(ERR, "Error in parsing rule for %s, possibly old config!", newrule.name.c_str());
        parseError = "Error in setting monitor rule. Are you using the old syntax? Confront the wiki.";
        return;
    }

    if (std::find_if(m_dMonitorRules.begin(), m_dMonitorRules.end(), [&](const auto& other) { return other.name == newrule.name; }) != m_dMonitorRules.end())
        m_dMonitorRules.erase(std::remove_if(m_dMonitorRules.begin(), m_dMonitorRules.end(), [&](const auto& other) { return other.name == newrule.name; }));

    m_dMonitorRules.push_back(newrule);
}

void CConfigManager::handleBezier(const std::string& command, const std::string& args) {
    std::string curitem = "";

    std::string argZ = args;

    auto nextItem = [&]() {
        auto idx = argZ.find_first_of(',');

        if (idx != std::string::npos) {
            curitem = argZ.substr(0, idx);
            argZ = argZ.substr(idx + 1);
        } else {
            curitem = argZ;
            argZ = "";
        }
    };

    nextItem();

    std::string bezierName = curitem;

    nextItem();
    if (curitem == "")
        parseError = "too few arguments";
    float p1x = std::stof(curitem);
    nextItem();
    if (curitem == "")
        parseError = "too few arguments";
    float p1y = std::stof(curitem);
    nextItem();
    if (curitem == "")
        parseError = "too few arguments";
    float p2x = std::stof(curitem);
    nextItem();
    if (curitem == "")
        parseError = "too few arguments";
    float p2y = std::stof(curitem);
    nextItem();
    if (curitem != "")
        parseError = "too many arguments";

    g_pAnimationManager->addBezierWithName(bezierName, Vector2D(p1x, p1y), Vector2D(p2x, p2y));
}

void CConfigManager::setAnimForChildren(SAnimationPropertyConfig *const ANIM) {
    for (auto& [name, anim] : animationConfig) {
        if (anim.pParentAnimation == ANIM && !anim.overriden) {
            // if a child isnt overriden, set the values of the parent
            anim.pValues = ANIM->pValues;

            setAnimForChildren(&anim);
        }
    }
};

void CConfigManager::handleAnimation(const std::string& command, const std::string& args) {
    std::string curitem = "";

    std::string argZ = args;

    auto nextItem = [&]() {
        auto idx = argZ.find_first_of(',');

        if (idx != std::string::npos) {
            curitem = argZ.substr(0, idx);
            argZ = argZ.substr(idx + 1);
        } else {
            curitem = argZ;
            argZ = "";
        }
    };

    nextItem();

    // Master on/off

    // anim name
    const auto ANIMNAME = curitem;
    
    const auto PANIM = animationConfig.find(ANIMNAME);

    if (PANIM == animationConfig.end()) {
        parseError = "no such animation";
        return;
    }

    PANIM->second.overriden = true;
    PANIM->second.pValues = &PANIM->second;

    nextItem();

    // on/off
    PANIM->second.internalEnabled = curitem == "1";

    if (curitem != "0" && curitem != "1") {
        parseError = "invalid animation on/off state";
    }

    nextItem();

    // speed
    if (isNumber(curitem, true)) {
        PANIM->second.internalSpeed = std::stof(curitem);

        if (PANIM->second.internalSpeed <= 0) {
            parseError = "invalid speed";
            PANIM->second.internalSpeed = 1.f;
        }
    } else {
        PANIM->second.internalSpeed = 10.f;
        parseError = "invalid speed";
    }

    nextItem();

    // curve
    PANIM->second.internalBezier = curitem;

    if (!g_pAnimationManager->bezierExists(curitem)) {
        parseError = "no such bezier";
        PANIM->second.internalBezier = "default";
    }

    nextItem();

    // style
    PANIM->second.internalStyle = curitem;

    if (curitem != "") {
        const auto ERR = g_pAnimationManager->styleValidInConfigVar(ANIMNAME, curitem);

        if (ERR != "")
            parseError = ERR;
    }

    // now, check for children, recursively
    setAnimForChildren(&PANIM->second);
}

void CConfigManager::handleBind(const std::string& command, const std::string& value) {
    // example:
    // bind[fl]=SUPER,G,exec,dmenu_run <args>

    // flags
    bool locked = false;
    bool release = false;
    bool repeat = false;
    const auto ARGS = command.substr(4);

    for (auto& arg : ARGS) {
        if (arg == 'l') {
            locked = true;
        } else if (arg == 'r') {
            release = true;
        } else if (arg == 'e') {
            repeat = true;
        } else {
            parseError = "bind: invalid flag";
            return;
        }
    }

    if (release && repeat) {
        parseError = "flags r and e are mutually exclusive";
        return;
    }

    auto valueCopy = value;

    const auto MOD = g_pKeybindManager->stringToModMask(valueCopy.substr(0, valueCopy.find_first_of(",")));
    const auto MODSTR = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto KEY = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto HANDLER = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COMMAND = valueCopy;

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(HANDLER);

    if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end()) {
        Debug::log(ERR, "Invalid dispatcher!");
        parseError = "Invalid dispatcher, requested \"" + HANDLER + "\" does not exist";
        return;
    }

    if (MOD == 0 && MODSTR != "") {
        Debug::log(ERR, "Invalid mod!");
        parseError = "Invalid mod, requested mod \"" + MODSTR + "\" is not a valid mod.";
        return;
    }

    if (KEY != "") {
        if (isNumber(KEY) && std::stoi(KEY) > 9)
            g_pKeybindManager->addKeybind(SKeybind{"", std::stoi(KEY), MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat});
        else
            g_pKeybindManager->addKeybind(SKeybind{KEY, -1, MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat});
    }
        
}

void CConfigManager::handleUnbind(const std::string& command, const std::string& value) {
    auto valueCopy = value;

    const auto MOD = g_pKeybindManager->stringToModMask(valueCopy.substr(0, valueCopy.find_first_of(",")));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto KEY = valueCopy;

    g_pKeybindManager->removeKeybind(MOD, KEY);
}

void CConfigManager::handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE = value.substr(0, value.find_first_of(","));
    const auto VALUE = value.substr(value.find_first_of(",") + 1);

    // check rule and value
    if (RULE == "" || VALUE == "") {
        return;
    }

    // verify we support a rule
    if (RULE != "float" 
        && RULE != "tile"
        && RULE.find("opacity") != 0
        && RULE.find("move") != 0
        && RULE.find("size") != 0
        && RULE.find("pseudo") != 0
        && RULE.find("monitor") != 0
        && RULE != "nofocus"
        && RULE != "noblur"
        && RULE != "center"
        && RULE != "opaque"
        && RULE != "fullscreen"
        && RULE.find("animation") != 0
        && RULE.find("rounding") != 0
        && RULE.find("workspace") != 0) {
            Debug::log(ERR, "Invalid rule found: %s", RULE.c_str());
            parseError = "Invalid rule found: " + RULE;
            return;
        }

    m_dWindowRules.push_back({RULE, VALUE});

}

void CConfigManager::handleBlurLS(const std::string& command, const std::string& value) {
    if (value.find("remove,") == 0) {
        const auto TOREMOVE = value.substr(7);
        m_dBlurLSNamespaces.erase(std::remove(m_dBlurLSNamespaces.begin(), m_dBlurLSNamespaces.end(), TOREMOVE));
        return;
    }

    m_dBlurLSNamespaces.emplace_back(value);
}

void CConfigManager::handleDefaultWorkspace(const std::string& command, const std::string& value) {

    const auto DISPLAY = value.substr(0, value.find_first_of(','));
    const auto WORKSPACE = value.substr(value.find_first_of(',') + 1);

    for (auto& mr : m_dMonitorRules) {
        if (mr.name == DISPLAY) {
            mr.defaultWorkspace = WORKSPACE;
            break;
        }
    }
}

void CConfigManager::handleSubmap(const std::string& command, const std::string& submap) {
    if (submap == "reset") 
        m_szCurrentSubmap = "";
    else
        m_szCurrentSubmap = submap;
}

void CConfigManager::handleSource(const std::string& command, const std::string& rawpath) {
    static const char* const ENVHOME = getenv("HOME");

    auto value = rawpath;

    if (value.length() < 2) {
        Debug::log(ERR, "source= path garbage");
        parseError = "source path " + value + " bogus!";
        return;
    }

    if (value[0] == '.') {
        auto currentDir = configCurrentPath.substr(0, configCurrentPath.find_last_of('/'));

        if (value[1] == '.') {
            auto parentDir = currentDir.substr(0, currentDir.find_last_of('/'));
            value.replace(0, 2, parentDir);
        } else {
            value.replace(0, 1, currentDir);
        }
    }

    if (value[0] == '~') {
        value.replace(0, 1, std::string(ENVHOME));
    }

    if (!std::filesystem::exists(value)) {
        Debug::log(ERR, "source= file doesnt exist");
        parseError = "source file " + value + " doesn't exist!";
        return;
    }

    configPaths.push_back(value);

    struct stat fileStat;
    int err = stat(value.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config at %s, error %i: %s", value.c_str(), err, strerror(err));
        return;
    }

    configModifyTimes[value] = fileStat.st_mtime;

    std::ifstream ifs;
    ifs.open(value);
    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                configCurrentPath = value;
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError += "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }
}

std::string CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE, bool dynamic) {
    if (dynamic) {
        parseError = "";
        currentCategory = "";
    }

    if (COMMAND == "exec") {
        if (isFirstLaunch) {
            firstExecRequests.push_back(VALUE);
        } else {
            handleRawExec(COMMAND, VALUE);
        }
    } else if (COMMAND == "exec-once") {
        if (isFirstLaunch) {
            firstExecRequests.push_back(VALUE);
        }
    }
    else if (COMMAND == "monitor") handleMonitor(COMMAND, VALUE);
    else if (COMMAND.find("bind") == 0) handleBind(COMMAND, VALUE);
    else if (COMMAND == "unbind") handleUnbind(COMMAND, VALUE);
    else if (COMMAND == "workspace") handleDefaultWorkspace(COMMAND, VALUE);
    else if (COMMAND == "windowrule") handleWindowRule(COMMAND, VALUE);
    else if (COMMAND == "bezier") handleBezier(COMMAND, VALUE);
    else if (COMMAND == "animation") handleAnimation(COMMAND, VALUE);
    else if (COMMAND == "source") handleSource(COMMAND, VALUE);
    else if (COMMAND == "submap") handleSubmap(COMMAND, VALUE);
    else if (COMMAND == "blurls") handleBlurLS(COMMAND, VALUE);
    else
        configSetValueSafe(currentCategory + (currentCategory == "" ? "" : ":") + COMMAND, VALUE);

    if (dynamic) {
        std::string retval = parseError;
        parseError = "";

        // invalidate layouts jic
        for (auto& m : g_pCompositor->m_vMonitors)
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

        // Update window border colors
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        return retval;
    }

    return parseError;
}

void CConfigManager::applyUserDefinedVars(std::string& line, const size_t equalsPlace) {
    auto dollarPlace = line.find_first_of('$', equalsPlace);

    while (dollarPlace != std::string::npos) {

        const auto STRAFTERDOLLAR = line.substr(dollarPlace + 1);
        for (auto&[var, value] : configDynamicVars) {
            if (STRAFTERDOLLAR.find(var) == 0) {
                line.replace(dollarPlace, var.length() + 1, value);
                break;
            }
        }

        dollarPlace = line.find_first_of('$', dollarPlace + 1);
    }
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

    if (line.contains(" {")) {
        auto cat = line.substr(0, line.find(" {"));
        transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
        if (currentCategory.length() != 0) {
            currentCategory.push_back(':');
            currentCategory.append(cat);
        }
        else {
            currentCategory = cat;
        }

        return;
    }

    if (line.contains("}") && currentCategory != "") {
        currentCategory = "";
        return;
    }

    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    // apply vars
    applyUserDefinedVars(line, EQUALSPLACE);

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = removeBeginEndSpacesTabs(line.substr(0, EQUALSPLACE));
    const auto VALUE = removeBeginEndSpacesTabs(line.substr(EQUALSPLACE + 1));
    //

    parseKeyword(COMMAND, VALUE);
}

void CConfigManager::loadConfigLoadVars() {
    Debug::log(LOG, "Reloading the config!");
    parseError = "";       // reset the error
    currentCategory = "";  // reset the category
    
    // reset all vars before loading
    setDefaultVars();
    m_dMonitorRules.clear();
    m_dWindowRules.clear();
    g_pKeybindManager->clearKeybinds();
    g_pAnimationManager->removeAllBeziers();
    m_mAdditionalReservedAreas.clear();
    configDynamicVars.clear();
    deviceConfigs.clear();
    m_dBlurLSNamespaces.clear();
    setDefaultAnimationVars(); // reset anims

    // paths
    configPaths.clear();

    std::string CONFIGPATH;

    static const char* const ENVHOME = getenv("HOME");
    const std::string CONFIGPARENTPATH = ENVHOME + (std::string) "/.config/hypr/";

    if (g_pCompositor->explicitConfigPath == "") {
        CONFIGPATH = CONFIGPARENTPATH + (ISDEBUG ? "hyprlandd.conf" : "hyprland.conf");
    } else {
        CONFIGPATH = g_pCompositor->explicitConfigPath;
    }

    configPaths.push_back(CONFIGPATH);
 
    std::ifstream ifs;
    ifs.open(CONFIGPATH);

    if (!ifs.good()) {
        if(g_pCompositor->explicitConfigPath == "") {
            Debug::log(WARN, "Config reading error. (No file? Attempting to generate, backing up old one if exists)");
            try {
                std::filesystem::rename(CONFIGPATH, CONFIGPATH + ".backup");
            } catch(...) { /* Probably doesn't exist */}

            try {
                if (!std::filesystem::is_directory(CONFIGPARENTPATH))
                    std::filesystem::create_directories(CONFIGPARENTPATH);
            }
            catch (...) {
                parseError = "Broken config file! (Could not create directory)";
                return;
            }
        }

        std::ofstream ofs;
        ofs.open(CONFIGPATH, std::ios::trunc);

        ofs << AUTOCONFIG;

        ofs.close();

        ifs.open(CONFIGPATH);

        if (!ifs.good()) {
            parseError = "Broken config file! (Could not open)";
            return;
        }
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                configCurrentPath = "~/.config/hypr/hyprland.conf";
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError += "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }

    for (auto& m : g_pCompositor->m_vMonitors)
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

    // Update the keyboard layout to the cfg'd one if this is not the first launch
    if (!isFirstLaunch) {
        g_pInputManager->setKeyboardLayout();
        g_pInputManager->setMouseConfigs();
    }

    // Calculate the internal vars
    configValues["general:main_mod_internal"].intValue = g_pKeybindManager->stringToModMask(configValues["general:main_mod"].strValue);
    const auto DAMAGETRACKINGMODE = g_pHyprRenderer->damageTrackingModeFromStr(configValues["general:damage_tracking"].strValue);
    if (DAMAGETRACKINGMODE != DAMAGE_TRACKING_INVALID)
        configValues["general:damage_tracking_internal"].intValue = DAMAGETRACKINGMODE;
    else {
        parseError = "invalid value for general:damage_tracking, supported: full, monitor, none";
        configValues["general:damage_tracking_internal"].intValue = DAMAGE_TRACKING_NONE;
    }

    // parseError will be displayed next frame
    if (parseError != "")
        g_pHyprError->queueCreate(parseError + "\nHyprland may not work correctly.", CColor(255, 50, 50, 255));
    else if (configValues["autogenerated"].intValue == 1)
        g_pHyprError->queueCreate("Warning: You're using an autogenerated config! (config file: " + CONFIGPATH + " )\nSUPER+Enter -> kitty\nSUPER+T -> Alacritty\nSUPER+M -> exit Hyprland", CColor(255, 255, 70, 255));
    else
        g_pHyprError->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    if (!isFirstLaunch) {
        m_bWantsMonitorReload = true;

        // check
        ensureDPMS();
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // update layout
    g_pLayoutManager->switchToLayout(configValues["general:layout"].strValue);

    // mark blur dirty
    for (auto& m : g_pCompositor->m_vMonitors)
        g_pHyprOpenGL->markBlurDirtyForMonitor(m.get());

    // Force the compositor to fully re-render all monitors
    for (auto& m : g_pCompositor->m_vMonitors)
        m->forceFullFrames = 2;
}

void CConfigManager::tick() {
    static const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    if (!std::filesystem::exists(CONFIGPATH)) {
        Debug::log(ERR, "Config doesn't exist??");
        return;
    }

    bool parse = false;

    for (auto& cf : configPaths) {
        struct stat fileStat;
        int err = stat(cf.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at %s, error %i: %s", cf.c_str(), err, strerror(err));
            return;
        }

        // check if we need to reload cfg
        if (fileStat.st_mtime != configModifyTimes[cf] || m_bForceReload) {
            parse = true;
            configModifyTimes[cf] = fileStat.st_mtime;
        }
    }

    if (parse) {
        m_bForceReload = false;

        loadConfigLoadVars();
    }
}

std::mutex configmtx;
SConfigValue CConfigManager::getConfigValueSafe(const std::string& val) {
    std::lock_guard<std::mutex> lg(configmtx);

    SConfigValue copy = configValues[val];

    return copy;
}

SConfigValue CConfigManager::getConfigValueSafeDevice(const std::string& dev, const std::string& val) {
    std::lock_guard<std::mutex> lg(configmtx);

    const auto it = deviceConfigs.find(dev);

    if (it == deviceConfigs.end()) {
        return SConfigValue();
    }

    SConfigValue copy = it->second[val];

    // fallback if not set explicitly
    if (!copy.set) {
        for (auto& cv : configValues) {
            auto foundIt = cv.first.find(val);
            if (foundIt == std::string::npos)
                continue;

            if (foundIt == cv.first.length() - val.length()) {
                copy = cv.second;
            }
        }
    }

    return copy;
}

int CConfigManager::getInt(const std::string& v) {
    return getConfigValueSafe(v).intValue;
}

float CConfigManager::getFloat(const std::string& v) {
    return getConfigValueSafe(v).floatValue;
}

std::string CConfigManager::getString(const std::string& v) {
    const auto VAL = getConfigValueSafe(v).strValue;

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& v) {
    return getConfigValueSafeDevice(dev, v).intValue;
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& v) {
    return getConfigValueSafeDevice(dev, v).floatValue;
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& v) {
    const auto VAL = getConfigValueSafeDevice(dev, v).strValue;

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

void CConfigManager::setInt(std::string v, int val) {
    configValues[v].intValue = val;
}

void CConfigManager::setFloat(std::string v, float val) {
    configValues[v].floatValue = val;
}

void CConfigManager::setString(std::string v, std::string val) {
    configValues[v].strValue = val;
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

    Debug::log(WARN, "No rule found for %s, trying to use the first.", name.c_str());

    for (auto& r : m_dMonitorRules) {
        if (r.name == "") {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    Debug::log(WARN, "No rules configured. Using the default hardcoded one.");

    return SMonitorRule{.name = "", .resolution = Vector2D(1280, 720), .offset = Vector2D(0, 0), .scale = 1};
}

std::vector<SWindowRule> CConfigManager::getMatchingRules(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return std::vector<SWindowRule>();

    std::vector<SWindowRule> returns;

    std::string title = g_pXWaylandManager->getTitle(pWindow);
    std::string appidclass = g_pXWaylandManager->getAppIDClass(pWindow);

    Debug::log(LOG, "Searching for matching rules for %s (title: %s)", appidclass.c_str(), title.c_str());

    for (auto& rule : m_dWindowRules) {
        // check if we have a matching rule
        try {
            if (rule.szValue.find("title:") == 0) {
                // we have a title rule.
                std::regex RULECHECK(rule.szValue.substr(6));

                if (!std::regex_search(title, RULECHECK))
                    continue;
            } else {
                std::regex classCheck(rule.szValue);

                if (!std::regex_search(appidclass, classCheck))
                    continue;
            }
        } catch (...) {
            Debug::log(ERR, "Regex error at %s", rule.szValue.c_str());
            continue;
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule %s -> %s matched %x [%s]", rule.szRule.c_str(), rule.szValue.c_str(), pWindow, pWindow->m_szTitle.c_str());

        returns.push_back(rule);
    }

    return returns;
}

void CConfigManager::dispatchExecOnce() {
    if (firstExecDispatched || isFirstLaunch)
        return;

    firstExecDispatched = true;

    for (auto& c : firstExecRequests) {
        handleRawExec("", c);
    }

    firstExecRequests.clear(); // free some kb of memory :P

    // set input, fixes some certain issues
    g_pInputManager->setKeyboardLayout();
    g_pInputManager->setMouseConfigs();

    // set ws names again
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        wlr_ext_workspace_handle_v1_set_name(ws->m_pWlrHandle, ws->m_szName.c_str());
    }
}

void CConfigManager::performMonitorReload() {

    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        auto rule = getMonitorRuleFor(m->szName);
        if (!g_pHyprRenderer->applyMonitorRule(m.get(), &rule)) {
            overAgain = true;
            break;
        }
    }

    if (overAgain)
        performMonitorReload();

    m_bWantsMonitorReload = false;
}

SConfigValue* CConfigManager::getConfigValuePtr(std::string val) {
    return &configValues[val];
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    const auto it = deviceConfigs.find(dev);

    return it != deviceConfigs.end();
}

bool CConfigManager::shouldBlurLS(const std::string& ns) {
    for (auto& bls : m_dBlurLSNamespaces) {
        if (bls == ns) {
            return true;
        }
    }

    return false;
}

void CConfigManager::ensureDPMS() {
    for (auto& rm : g_pCompositor->m_vRealMonitors) {
        auto rule = getMonitorRuleFor(rm->szName);

        if (rule.disabled == rm->m_bEnabled) {
	        rm->m_pThisWrap = &rm;
            g_pHyprRenderer->applyMonitorRule(rm.get(), &rule);
        }
    }
}

SAnimationPropertyConfig* CConfigManager::getAnimationPropertyConfig(const std::string& name) {
    return &animationConfig[name];
}

void CConfigManager::addParseError(const std::string& err) {
    if (parseError == "")
        parseError = err;
}