#include "ConfigManager.hpp"
#include "../managers/KeybindManager.hpp"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>

extern "C" char** environ;

CConfigManager::CConfigManager() {
    configValues["general:col.active_border"].data       = std::make_shared<CGradientValueData>(0xffffffff);
    configValues["general:col.inactive_border"].data     = std::make_shared<CGradientValueData>(0xff444444);
    configValues["general:col.group_border"].data        = std::make_shared<CGradientValueData>(0x66777700);
    configValues["general:col.group_border_active"].data = std::make_shared<CGradientValueData>(0x66ffff00);

    setDefaultVars();
    setDefaultAnimationVars();

    std::string CONFIGPATH;
    if (g_pCompositor->explicitConfigPath == "") {
        static const char* const ENVHOME = getenv("HOME");
        CONFIGPATH                       = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");
    } else {
        CONFIGPATH = g_pCompositor->explicitConfigPath;
    }

    configPaths.emplace_back(CONFIGPATH);

    Debug::disableLogs = &configValues["debug:disable_logs"].intValue;
    Debug::disableTime = &configValues["debug:disable_time"].intValue;

    populateEnvironment();
}

void CConfigManager::populateEnvironment() {
    environmentVariables.clear();
    for (char** env = environ; *env; ++env) {
        const std::string ENVVAR   = *env;
        const auto        VARIABLE = ENVVAR.substr(0, ENVVAR.find_first_of('='));
        const auto        VALUE    = ENVVAR.substr(ENVVAR.find_first_of('=') + 1);
        environmentVariables.emplace_back(std::make_pair<>(VARIABLE, VALUE));
    }

    std::sort(environmentVariables.begin(), environmentVariables.end(), [&](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
}

void CConfigManager::setDefaultVars() {
    configValues["general:max_fps"].intValue               = 60;
    configValues["general:sensitivity"].floatValue         = 1.0f;
    configValues["general:apply_sens_to_raw"].intValue     = 0;
    configValues["general:border_size"].intValue           = 1;
    configValues["general:no_border_on_floating"].intValue = 0;
    configValues["general:gaps_in"].intValue               = 5;
    configValues["general:gaps_out"].intValue              = 20;
    ((CGradientValueData*)configValues["general:col.active_border"].data.get())->reset(0xffffffff);
    ((CGradientValueData*)configValues["general:col.inactive_border"].data.get())->reset(0xff444444);
    ((CGradientValueData*)configValues["general:col.group_border"].data.get())->reset(0x66777700);
    ((CGradientValueData*)configValues["general:col.group_border_active"].data.get())->reset(0x66ffff00);
    configValues["general:cursor_inactive_timeout"].intValue = 0;
    configValues["general:no_cursor_warps"].intValue         = 0;
    configValues["general:resize_on_border"].intValue        = 0;
    configValues["general:extend_border_grab_area"].intValue = 15;
    configValues["general:hover_icon_on_border"].intValue    = 1;
    configValues["general:layout"].strValue                  = "dwindle";

    configValues["misc:disable_hyprland_logo"].intValue        = 0;
    configValues["misc:disable_splash_rendering"].intValue     = 0;
    configValues["misc:vfr"].intValue                          = 1;
    configValues["misc:vrr"].intValue                          = 0;
    configValues["misc:mouse_move_enables_dpms"].intValue      = 0;
    configValues["misc:key_press_enables_dpms"].intValue       = 0;
    configValues["misc:always_follow_on_dnd"].intValue         = 1;
    configValues["misc:layers_hog_keyboard_focus"].intValue    = 1;
    configValues["misc:animate_manual_resizes"].intValue       = 0;
    configValues["misc:animate_mouse_windowdragging"].intValue = 0;
    configValues["misc:disable_autoreload"].intValue           = 0;
    configValues["misc:enable_swallow"].intValue               = 0;
    configValues["misc:swallow_regex"].strValue                = STRVAL_EMPTY;
    configValues["misc:focus_on_activate"].intValue            = 0;
    configValues["misc:no_direct_scanout"].intValue            = 1;
    configValues["misc:hide_cursor_on_touch"].intValue         = 1;
    configValues["misc:mouse_move_focuses_monitor"].intValue   = 1;
    configValues["misc:suppress_portal_warnings"].intValue     = 0;
    configValues["misc:render_ahead_of_time"].intValue         = 0;
    configValues["misc:render_ahead_safezone"].intValue        = 1;

    configValues["debug:int"].intValue                = 0;
    configValues["debug:log_damage"].intValue         = 0;
    configValues["debug:overlay"].intValue            = 0;
    configValues["debug:damage_blink"].intValue       = 0;
    configValues["debug:disable_logs"].intValue       = 0;
    configValues["debug:disable_time"].intValue       = 1;
    configValues["debug:enable_stdout_logs"].intValue = 0;
    configValues["debug:damage_tracking"].intValue    = DAMAGE_TRACKING_FULL;
    configValues["debug:manual_crash"].intValue       = 0;

    configValues["decoration:rounding"].intValue               = 0;
    configValues["decoration:blur"].intValue                   = 1;
    configValues["decoration:blur_size"].intValue              = 8;
    configValues["decoration:blur_passes"].intValue            = 1;
    configValues["decoration:blur_ignore_opacity"].intValue    = 0;
    configValues["decoration:blur_new_optimizations"].intValue = 1;
    configValues["decoration:blur_xray"].intValue              = 0;
    configValues["decoration:active_opacity"].floatValue       = 1;
    configValues["decoration:inactive_opacity"].floatValue     = 1;
    configValues["decoration:fullscreen_opacity"].floatValue   = 1;
    configValues["decoration:multisample_edges"].intValue      = 1;
    configValues["decoration:no_blur_on_oversized"].intValue   = 0;
    configValues["decoration:drop_shadow"].intValue            = 1;
    configValues["decoration:shadow_range"].intValue           = 4;
    configValues["decoration:shadow_render_power"].intValue    = 3;
    configValues["decoration:shadow_ignore_window"].intValue   = 1;
    configValues["decoration:shadow_offset"].vecValue          = Vector2D();
    configValues["decoration:shadow_scale"].floatValue         = 1.f;
    configValues["decoration:col.shadow"].intValue             = 0xee1a1a1a;
    configValues["decoration:col.shadow_inactive"].intValue    = INT_MAX;
    configValues["decoration:dim_inactive"].intValue           = 0;
    configValues["decoration:dim_strength"].floatValue         = 0.5f;
    configValues["decoration:dim_special"].floatValue          = 0.2f;
    configValues["decoration:dim_around"].floatValue           = 0.4f;
    configValues["decoration:screen_shader"].strValue          = STRVAL_EMPTY;

    configValues["dwindle:pseudotile"].intValue               = 0;
    configValues["dwindle:force_split"].intValue              = 0;
    configValues["dwindle:preserve_split"].intValue           = 0;
    configValues["dwindle:special_scale_factor"].floatValue   = 0.8f;
    configValues["dwindle:split_width_multiplier"].floatValue = 1.0f;
    configValues["dwindle:no_gaps_when_only"].intValue        = 0;
    configValues["dwindle:use_active_for_splits"].intValue    = 1;
    configValues["dwindle:default_split_ratio"].floatValue    = 1.f;

    configValues["master:special_scale_factor"].floatValue = 0.8f;
    configValues["master:mfact"].floatValue                = 0.55f;
    configValues["master:new_is_master"].intValue          = 1;
    configValues["master:always_center_master"].intValue   = 0;
    configValues["master:new_on_top"].intValue             = 0;
    configValues["master:no_gaps_when_only"].intValue      = 0;
    configValues["master:orientation"].strValue            = "left";
    configValues["master:inherit_fullscreen"].intValue     = 1;

    configValues["animations:enabled"].intValue = 1;

    configValues["input:follow_mouse"].intValue                     = 1;
    configValues["input:sensitivity"].floatValue                    = 0.f;
    configValues["input:accel_profile"].strValue                    = STRVAL_EMPTY;
    configValues["input:kb_file"].strValue                          = STRVAL_EMPTY;
    configValues["input:kb_layout"].strValue                        = "us";
    configValues["input:kb_variant"].strValue                       = STRVAL_EMPTY;
    configValues["input:kb_options"].strValue                       = STRVAL_EMPTY;
    configValues["input:kb_rules"].strValue                         = STRVAL_EMPTY;
    configValues["input:kb_model"].strValue                         = STRVAL_EMPTY;
    configValues["input:repeat_rate"].intValue                      = 25;
    configValues["input:repeat_delay"].intValue                     = 600;
    configValues["input:natural_scroll"].intValue                   = 0;
    configValues["input:numlock_by_default"].intValue               = 0;
    configValues["input:force_no_accel"].intValue                   = 0;
    configValues["input:float_switch_override_focus"].intValue      = 1;
    configValues["input:left_handed"].intValue                      = 0;
    configValues["input:scroll_method"].strValue                    = STRVAL_EMPTY;
    configValues["input:scroll_button"].intValue                    = 0;
    configValues["input:touchpad:natural_scroll"].intValue          = 0;
    configValues["input:touchpad:disable_while_typing"].intValue    = 1;
    configValues["input:touchpad:clickfinger_behavior"].intValue    = 0;
    configValues["input:touchpad:tap_button_map"].strValue          = STRVAL_EMPTY;
    configValues["input:touchpad:middle_button_emulation"].intValue = 0;
    configValues["input:touchpad:tap-to-click"].intValue            = 1;
    configValues["input:touchpad:tap-and-drag"].intValue            = 1;
    configValues["input:touchpad:drag_lock"].intValue               = 0;
    configValues["input:touchpad:scroll_factor"].floatValue         = 1.f;
    configValues["input:touchdevice:transform"].intValue            = 0;
    configValues["input:touchdevice:output"].strValue               = STRVAL_EMPTY;
    configValues["input:tablet:transform"].intValue                 = 0;
    configValues["input:tablet:output"].strValue                    = STRVAL_EMPTY;

    configValues["binds:pass_mouse_when_bound"].intValue    = 0;
    configValues["binds:scroll_event_delay"].intValue       = 300;
    configValues["binds:workspace_back_and_forth"].intValue = 0;
    configValues["binds:allow_workspace_cycles"].intValue   = 0;
    configValues["binds:focus_preferred_method"].intValue   = 0;

    configValues["gestures:workspace_swipe"].intValue                    = 0;
    configValues["gestures:workspace_swipe_fingers"].intValue            = 3;
    configValues["gestures:workspace_swipe_distance"].intValue           = 300;
    configValues["gestures:workspace_swipe_invert"].intValue             = 1;
    configValues["gestures:workspace_swipe_min_speed_to_force"].intValue = 30;
    configValues["gestures:workspace_swipe_cancel_ratio"].floatValue     = 0.5f;
    configValues["gestures:workspace_swipe_create_new"].intValue         = 1;
    configValues["gestures:workspace_swipe_forever"].intValue            = 0;
    configValues["gestures:workspace_swipe_numbered"].intValue           = 0;

    configValues["autogenerated"].intValue = 0;
}

void CConfigManager::setDeviceDefaultVars(const std::string& dev) {
    auto& cfgValues = deviceConfigs[dev];

    cfgValues["sensitivity"].floatValue           = 0.f;
    cfgValues["accel_profile"].strValue           = STRVAL_EMPTY;
    cfgValues["kb_file"].strValue                 = STRVAL_EMPTY;
    cfgValues["kb_layout"].strValue               = "us";
    cfgValues["kb_variant"].strValue              = STRVAL_EMPTY;
    cfgValues["kb_options"].strValue              = STRVAL_EMPTY;
    cfgValues["kb_rules"].strValue                = STRVAL_EMPTY;
    cfgValues["kb_model"].strValue                = STRVAL_EMPTY;
    cfgValues["repeat_rate"].intValue             = 25;
    cfgValues["repeat_delay"].intValue            = 600;
    cfgValues["natural_scroll"].intValue          = 0;
    cfgValues["tap_button_map"].strValue          = STRVAL_EMPTY;
    cfgValues["numlock_by_default"].intValue      = 0;
    cfgValues["disable_while_typing"].intValue    = 1;
    cfgValues["clickfinger_behavior"].intValue    = 0;
    cfgValues["middle_button_emulation"].intValue = 0;
    cfgValues["tap-to-click"].intValue            = 1;
    cfgValues["tap-and-drag"].intValue            = 1;
    cfgValues["drag_lock"].intValue               = 0;
    cfgValues["left_handed"].intValue             = 0;
    cfgValues["scroll_method"].strValue           = STRVAL_EMPTY;
    cfgValues["scroll_button"].intValue           = 0;
    cfgValues["transform"].intValue               = 0;
    cfgValues["output"].strValue                  = STRVAL_EMPTY;
    cfgValues["enabled"].intValue                 = 1; // only for mice / touchpads
}

void CConfigManager::setDefaultAnimationVars() {
    if (isFirstLaunch) {
        INITANIMCFG("global");
        INITANIMCFG("windows");
        INITANIMCFG("fade");
        INITANIMCFG("border");
        INITANIMCFG("borderangle");
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
        INITANIMCFG("fadeDim");

        // border

        // workspaces
        INITANIMCFG("specialWorkspace");
    }

    // init the values
    animationConfig["global"] = {false, "default", "", 8.f, 1, &animationConfig["general"], nullptr};

    CREATEANIMCFG("windows", "global");
    CREATEANIMCFG("fade", "global");
    CREATEANIMCFG("border", "global");
    CREATEANIMCFG("borderangle", "global");
    CREATEANIMCFG("workspaces", "global");

    CREATEANIMCFG("windowsIn", "windows");
    CREATEANIMCFG("windowsOut", "windows");
    CREATEANIMCFG("windowsMove", "windows");

    CREATEANIMCFG("fadeIn", "fade");
    CREATEANIMCFG("fadeOut", "fade");
    CREATEANIMCFG("fadeSwitch", "fade");
    CREATEANIMCFG("fadeShadow", "fade");
    CREATEANIMCFG("fadeDim", "fade");

    CREATEANIMCFG("specialWorkspace", "workspaces");
}

void CConfigManager::init() {

    loadConfigLoadVars();

    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");

    struct stat       fileStat;
    int               err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at statting config, error %i", errno);
    }

    configModifyTimes[CONFIGPATH] = fileStat.st_mtime;

    isFirstLaunch = false;
}

void CConfigManager::configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (configValues.find(COMMAND) == configValues.end()) {
        if (COMMAND.find("device:") != 0 /* devices parsed later */ && COMMAND.find("plugin:") != 0 /* plugins parsed later */) {
            if (COMMAND[0] == '$') {
                // register a dynamic var
                Debug::log(LOG, "Registered dynamic var \"%s\" -> %s", COMMAND.c_str(), VALUE.c_str());
                configDynamicVars.emplace_back(std::make_pair<>(COMMAND.substr(1), VALUE));

                std::sort(configDynamicVars.begin(), configDynamicVars.end(), [&](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
            } else {
                parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
            }

            return;
        }
    }

    SConfigValue* CONFIGENTRY = nullptr;

    if (COMMAND.find("device:") == 0) {
        const auto DEVICE    = COMMAND.substr(7).substr(0, COMMAND.find_last_of(':') - 7);
        const auto CONFIGVAR = COMMAND.substr(COMMAND.find_last_of(':') + 1);

        if (!deviceConfigExists(DEVICE))
            setDeviceDefaultVars(DEVICE);

        auto it = deviceConfigs.find(DEVICE);

        if (it->second.find(CONFIGVAR) == it->second.end()) {
            if (it->second.contains("touch_output") || it->second.contains("touch_transform")) {
                parseError = "touch_output and touch_transform have been changed to output and transform respectively";
                return;
            }

            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
            return;
        }

        CONFIGENTRY = &it->second.at(CONFIGVAR);
    } else if (COMMAND.find("plugin:") == 0) {
        for (auto& [handle, pMap] : pluginConfigs) {
            auto it = std::find_if(pMap->begin(), pMap->end(), [&](const auto& other) { return other.first == COMMAND; });
            if (it == pMap->end()) {
                continue; // May be in another plugin
            }

            CONFIGENTRY = &it->second;
        }

        if (!CONFIGENTRY)
            return; // silent ignore
    } else {
        CONFIGENTRY = &configValues.at(COMMAND);
    }

    CONFIGENTRY->set = true;

    if (CONFIGENTRY->intValue != -INT64_MAX) {
        try {
            CONFIGENTRY->intValue = configStringToInt(VALUE);
        } catch (std::exception& e) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. " + e.what();
        }
    } else if (CONFIGENTRY->floatValue != -__FLT_MAX__) {
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
    } else if (CONFIGENTRY->vecValue != Vector2D(-__FLT_MAX__, -__FLT_MAX__)) {
        try {
            if (const auto SPACEPOS = VALUE.find(' '); SPACEPOS != std::string::npos) {
                const auto X = VALUE.substr(0, SPACEPOS);
                const auto Y = VALUE.substr(SPACEPOS + 1);

                if (isNumber(X, true) && isNumber(Y, true)) {
                    CONFIGENTRY->vecValue = Vector2D(std::stof(X), std::stof(Y));
                }
            } else {
                Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
                parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
            }
        } catch (...) {
            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY->data.get() != nullptr) {

        switch (CONFIGENTRY->data->getDataType()) {
            case CVD_TYPE_GRADIENT: {

                CVarList            varlist(VALUE, 0, ' ');

                CGradientValueData* data = (CGradientValueData*)CONFIGENTRY->data.get();
                data->m_vColors.clear();

                for (auto& var : varlist) {
                    if (var.find("deg") != std::string::npos) {
                        // last arg
                        try {
                            data->m_fAngle = std::stoi(var.substr(0, var.find("deg"))) * (PI / 180.0); // radians
                        } catch (...) {
                            Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
                            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
                        }

                        break;
                    }

                    if (data->m_vColors.size() >= 10) {
                        Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
                        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. Max colors in a gradient is 10.";
                        break;
                    }

                    try {
                        data->m_vColors.push_back(CColor(configStringToInt(var)));
                    } catch (std::exception& e) {
                        Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
                        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. " + e.what();
                    }
                }

                if (data->m_vColors.size() == 0) {
                    Debug::log(WARN, "Error reading value of %s", COMMAND.c_str());
                    parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. No colors provided.";

                    data->m_vColors.push_back(0); // transparent
                }

                break;
            }
            default: {
                UNREACHABLE();
            }
        }
    }

    if (COMMAND == "decoration:screen_shader") {
        const auto PATH = absolutePath(VALUE, configCurrentPath);

        configPaths.push_back(PATH);

        struct stat fileStat;
        int         err = stat(PATH.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at %s, error %i: %s", PATH.c_str(), err, strerror(err));
            return;
        }

        configModifyTimes[PATH] = fileStat.st_mtime;
    }
}

void CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.
    g_pKeybindManager->spawn(args);
}

void CConfigManager::handleMonitor(const std::string& command, const std::string& args) {

    // get the monitor config
    SMonitorRule newrule;

    const auto   ARGS = CVarList(args);

    newrule.name = ARGS[0];

    if (ARGS[1] == "disable" || ARGS[1] == "disabled" || ARGS[1] == "addreserved" || ARGS[1] == "transform") {
        if (ARGS[1] == "disable" || ARGS[1] == "disabled")
            newrule.disabled = true;
        else if (ARGS[1] == "transform") {
            const auto TSF = std::stoi(ARGS[2]);
            if (std::clamp(TSF, 0, 7) != TSF) {
                Debug::log(ERR, "invalid transform %i in monitor", TSF);
                parseError = "invalid transform";
                return;
            }

            wl_output_transform transform = (wl_output_transform)std::stoi(ARGS[2]);

            // overwrite if exists
            for (auto& r : m_dMonitorRules) {
                if (r.name == newrule.name) {
                    r.transform = transform;
                    return;
                }
            }

            return;
        } else if (ARGS[1] == "addreserved") {
            int top = std::stoi(ARGS[2]);

            int bottom = std::stoi(ARGS[3]);

            int left = std::stoi(ARGS[4]);

            int right = std::stoi(ARGS[5]);

            m_mAdditionalReservedAreas[newrule.name] = {top, bottom, left, right};

            return; // this is not a rule, ignore
        } else {
            Debug::log(ERR, "ConfigManager parseMonitor, curitem bogus???");
            return;
        }

        std::erase_if(m_dMonitorRules, [&](const auto& other) { return other.name == newrule.name; });

        m_dMonitorRules.push_back(newrule);

        return;
    }

    if (ARGS[1].find("pref") == 0) {
        newrule.resolution = Vector2D();
    } else if (ARGS[1].find("highrr") == 0) {
        newrule.resolution = Vector2D(-1, -1);
    } else if (ARGS[1].find("highres") == 0) {
        newrule.resolution = Vector2D(-1, -2);
    } else {
        newrule.resolution.x = stoi(ARGS[1].substr(0, ARGS[1].find_first_of('x')));
        newrule.resolution.y = stoi(ARGS[1].substr(ARGS[1].find_first_of('x') + 1, ARGS[1].find_first_of('@')));

        if (ARGS[1].contains("@"))
            newrule.refreshRate = stof(ARGS[1].substr(ARGS[1].find_first_of('@') + 1));
    }

    if (ARGS[2].find("auto") == 0) {
        newrule.offset = Vector2D(-1, -1);
    } else {
        newrule.offset.x = stoi(ARGS[2].substr(0, ARGS[2].find_first_of('x')));
        newrule.offset.y = stoi(ARGS[2].substr(ARGS[2].find_first_of('x') + 1));

        if (newrule.offset.x < 0 || newrule.offset.y < 0) {
            parseError     = "invalid offset. Offset cannot be negative.";
            newrule.offset = Vector2D();
        }
    }

    if (ARGS[3].find("auto") == 0) {
        newrule.scale = -1;
    } else {
        newrule.scale = stof(ARGS[3]);

        if (newrule.scale < 0.25f) {
            parseError    = "not a valid scale.";
            newrule.scale = 1;
        }
    }

    int argno = 4;

    while (ARGS[argno] != "") {
        if (ARGS[argno] == "mirror") {
            newrule.mirrorOf = ARGS[argno + 1];
            argno++;
        } else if (ARGS[argno] == "bitdepth") {
            newrule.enable10bit = ARGS[argno + 1] == "10";
            argno++;
        } else if (ARGS[argno] == "transform") {
            newrule.transform = (wl_output_transform)std::stoi(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "workspace") {
            m_mDefaultWorkspaces[newrule.name] = ARGS[argno + 1];
            argno++;
        } else {
            Debug::log(ERR, "Config error: invalid monitor syntax");
            parseError = "invalid syntax at \"" + ARGS[argno] + "\"";
            return;
        }

        argno++;
    }

    std::erase_if(m_dMonitorRules, [&](const auto& other) { return other.name == newrule.name; });

    m_dMonitorRules.push_back(newrule);
}

void CConfigManager::handleBezier(const std::string& command, const std::string& args) {
    const auto  ARGS = CVarList(args);

    std::string bezierName = ARGS[0];

    if (ARGS[1] == "")
        parseError = "too few arguments";
    float p1x = std::stof(ARGS[1]);

    if (ARGS[2] == "")
        parseError = "too few arguments";
    float p1y = std::stof(ARGS[2]);

    if (ARGS[3] == "")
        parseError = "too few arguments";
    float p2x = std::stof(ARGS[3]);

    if (ARGS[4] == "")
        parseError = "too few arguments";
    float p2y = std::stof(ARGS[4]);

    if (ARGS[5] != "")
        parseError = "too many arguments";

    g_pAnimationManager->addBezierWithName(bezierName, Vector2D(p1x, p1y), Vector2D(p2x, p2y));
}

void CConfigManager::setAnimForChildren(SAnimationPropertyConfig* const ANIM) {
    for (auto& [name, anim] : animationConfig) {
        if (anim.pParentAnimation == ANIM && !anim.overridden) {
            // if a child isnt overridden, set the values of the parent
            anim.pValues = ANIM->pValues;

            setAnimForChildren(&anim);
        }
    }
};

void CConfigManager::handleAnimation(const std::string& command, const std::string& args) {
    const auto ARGS = CVarList(args);

    // Master on/off

    // anim name
    const auto ANIMNAME = ARGS[0];

    const auto PANIM = animationConfig.find(ANIMNAME);

    if (PANIM == animationConfig.end()) {
        parseError = "no such animation";
        return;
    }

    PANIM->second.overridden = true;
    PANIM->second.pValues    = &PANIM->second;

    // on/off
    PANIM->second.internalEnabled = ARGS[1] == "1";

    if (ARGS[1] != "0" && ARGS[1] != "1") {
        parseError = "invalid animation on/off state";
    }

    // speed
    if (isNumber(ARGS[2], true)) {
        PANIM->second.internalSpeed = std::stof(ARGS[2]);

        if (PANIM->second.internalSpeed <= 0) {
            parseError                  = "invalid speed";
            PANIM->second.internalSpeed = 1.f;
        }
    } else {
        PANIM->second.internalSpeed = 10.f;
        parseError                  = "invalid speed";
    }

    // curve
    PANIM->second.internalBezier = ARGS[3];

    if (!g_pAnimationManager->bezierExists(ARGS[3])) {
        parseError                   = "no such bezier";
        PANIM->second.internalBezier = "default";
    }

    // style
    PANIM->second.internalStyle = ARGS[4];

    if (ARGS[4] != "") {
        const auto ERR = g_pAnimationManager->styleValidInConfigVar(ANIMNAME, ARGS[4]);

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
    bool       locked   = false;
    bool       release  = false;
    bool       repeat   = false;
    bool       mouse    = false;
    const auto BINDARGS = command.substr(4);

    for (auto& arg : BINDARGS) {
        if (arg == 'l') {
            locked = true;
        } else if (arg == 'r') {
            release = true;
        } else if (arg == 'e') {
            repeat = true;
        } else if (arg == 'm') {
            mouse = true;
        } else {
            parseError = "bind: invalid flag";
            return;
        }
    }

    if (release && repeat) {
        parseError = "flags r and e are mutually exclusive";
        return;
    }

    if (mouse && (repeat || release || locked)) {
        parseError = "flag m is exclusive";
        return;
    }

    const auto ARGS = CVarList(value, 4);

    if ((ARGS.size() < 3 && !mouse) || (ARGS.size() < 3 && mouse)) {
        parseError = "bind: too few args";
        return;
    } else if ((ARGS.size() > 4 && !mouse) || (ARGS.size() > 3 && mouse)) {
        parseError = "bind: too many args";
        return;
    }

    const auto MOD    = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto MODSTR = ARGS[0];

    const auto KEY = ARGS[1];

    auto       HANDLER = ARGS[2];

    const auto COMMAND = mouse ? HANDLER : ARGS[3];

    if (mouse)
        HANDLER = "mouse";

    // to lower
    std::transform(HANDLER.begin(), HANDLER.end(), HANDLER.begin(), ::tolower);

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
            g_pKeybindManager->addKeybind(SKeybind{"", std::stoi(KEY), MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse});
        else if (KEY.find("code:") == 0 && isNumber(KEY.substr(5)))
            g_pKeybindManager->addKeybind(SKeybind{"", std::stoi(KEY.substr(5)), MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse});
        else
            g_pKeybindManager->addKeybind(SKeybind{KEY, -1, MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse});
    }
}

void CConfigManager::handleUnbind(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    const auto MOD = g_pKeybindManager->stringToModMask(ARGS[0]);

    const auto KEY = ARGS[1];

    g_pKeybindManager->removeKeybind(MOD, KEY);
}

bool windowRuleValid(const std::string& RULE) {
    return !(RULE != "float" && RULE != "tile" && RULE.find("opacity") != 0 && RULE.find("move") != 0 && RULE.find("size") != 0 && RULE.find("minsize") != 0 &&
             RULE.find("maxsize") != 0 && RULE.find("pseudo") != 0 && RULE.find("monitor") != 0 && RULE.find("idleinhibit") != 0 && RULE != "nofocus" && RULE != "noblur" &&
             RULE != "noshadow" && RULE != "noborder" && RULE != "center" && RULE != "opaque" && RULE != "forceinput" && RULE != "fullscreen" && RULE != "nofullscreenrequest" &&
             RULE != "nomaxsize" && RULE != "pin" && RULE != "noanim" && RULE != "dimaround" && RULE != "windowdance" && RULE != "maximize" && RULE.find("animation") != 0 &&
             RULE.find("rounding") != 0 && RULE.find("workspace") != 0 && RULE.find("bordercolor") != 0 && RULE != "forcergbx");
}

bool layerRuleValid(const std::string& RULE) {
    return !(RULE != "noanim" && RULE != "blur" && RULE != "ignorezero");
}

void CConfigManager::handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = removeBeginEndSpacesTabs(value.substr(value.find_first_of(',') + 1));

    // check rule and value
    if (RULE == "" || VALUE == "") {
        return;
    }

    if (RULE == "unset") {
        std::erase_if(m_dWindowRules, [&](const SWindowRule& other) { return other.szValue == VALUE; });
        return;
    }

    // verify we support a rule
    if (!windowRuleValid(RULE)) {
        Debug::log(ERR, "Invalid rule found: %s", RULE.c_str());
        parseError = "Invalid rule found: " + RULE;
        return;
    }

    m_dWindowRules.push_back({RULE, VALUE});
}

void CConfigManager::handleLayerRule(const std::string& command, const std::string& value) {
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = removeBeginEndSpacesTabs(value.substr(value.find_first_of(',') + 1));

    // check rule and value
    if (RULE == "" || VALUE == "")
        return;

    if (RULE == "unset") {
        std::erase_if(m_dLayerRules, [&](const SLayerRule& other) { return other.targetNamespace == VALUE; });
        return;
    }

    if (!layerRuleValid(RULE)) {
        Debug::log(ERR, "Invalid rule found: %s", RULE.c_str());
        parseError = "Invalid rule found: " + RULE;
        return;
    }

    m_dLayerRules.push_back({VALUE, RULE});

    for (auto& m : g_pCompositor->m_vMonitors)
        for (auto& lsl : m->m_aLayerSurfaceLayers)
            for (auto& ls : lsl)
                ls->applyRules();
}

void CConfigManager::handleWindowRuleV2(const std::string& command, const std::string& value) {
    const auto RULE  = value.substr(0, value.find_first_of(','));
    const auto VALUE = value.substr(value.find_first_of(',') + 1);

    if (!windowRuleValid(RULE) && RULE != "unset") {
        Debug::log(ERR, "Invalid rulev2 found: %s", RULE.c_str());
        parseError = "Invalid rulev2 found: " + RULE;
        return;
    }

    // now we estract shit from the value
    SWindowRule rule;
    rule.v2      = true;
    rule.szRule  = RULE;
    rule.szValue = VALUE;

    const auto TITLEPOS      = VALUE.find("title:");
    const auto CLASSPOS      = VALUE.find("class:");
    const auto X11POS        = VALUE.find("xwayland:");
    const auto FLOATPOS      = VALUE.find("floating:");
    const auto FULLSCREENPOS = VALUE.find("fullscreen:");
    const auto PINNEDPOS     = VALUE.find("pinned:");

    if (TITLEPOS == std::string::npos && CLASSPOS == std::string::npos && X11POS == std::string::npos && FLOATPOS == std::string::npos && FULLSCREENPOS == std::string::npos &&
        PINNEDPOS == std::string::npos) {
        Debug::log(ERR, "Invalid rulev2 syntax: %s", VALUE.c_str());
        parseError = "Invalid rulev2 syntax: " + VALUE;
        return;
    }

    auto extract = [&](size_t pos) -> std::string {
        std::string result;
        result = VALUE.substr(pos);

        size_t min = 999999;
        if (TITLEPOS > pos && TITLEPOS < min)
            min = TITLEPOS;
        if (CLASSPOS > pos && CLASSPOS < min)
            min = CLASSPOS;
        if (X11POS > pos && X11POS < min)
            min = X11POS;
        if (FLOATPOS > pos && FLOATPOS < min)
            min = FLOATPOS;
        if (FULLSCREENPOS > pos && FULLSCREENPOS < min)
            min = FULLSCREENPOS;
        if (PINNEDPOS > pos && PINNEDPOS < min)
            min = PINNEDPOS;

        result = result.substr(0, min - pos);

        result = removeBeginEndSpacesTabs(result);

        if (result.back() == ',')
            result.pop_back();

        return result;
    };

    if (CLASSPOS != std::string::npos) {
        rule.szClass = extract(CLASSPOS + 6);
    }

    if (TITLEPOS != std::string::npos) {
        rule.szTitle = extract(TITLEPOS + 6);
    }

    if (X11POS != std::string::npos) {
        rule.bX11 = extract(X11POS + 9) == "1" ? 1 : 0;
    }

    if (FLOATPOS != std::string::npos) {
        rule.bFloating = extract(FLOATPOS + 9) == "1" ? 1 : 0;
    }

    if (FULLSCREENPOS != std::string::npos) {
        rule.bFullscreen = extract(FULLSCREENPOS + 11) == "1" ? 1 : 0;
    }

    if (PINNEDPOS != std::string::npos) {
        rule.bPinned = extract(PINNEDPOS + 7) == "1" ? 1 : 0;
    }

    if (RULE == "unset") {
        std::erase_if(m_dWindowRules, [&](const SWindowRule& other) {
            if (!other.v2) {
                return other.szClass == rule.szClass && !rule.szClass.empty();
            } else {
                if (!rule.szClass.empty() && rule.szClass != other.szClass) {
                    return false;
                }

                if (!rule.szTitle.empty() && rule.szTitle != other.szTitle) {
                    return false;
                }

                if (rule.bX11 != -1 && rule.bX11 != other.bX11) {
                    return false;
                }

                if (rule.bFloating != -1 && rule.bFloating != other.bFloating) {
                    return false;
                }

                if (rule.bFullscreen != -1 && rule.bFullscreen != other.bFullscreen) {
                    return false;
                }

                if (rule.bPinned != -1 && rule.bPinned != other.bPinned) {
                    return false;
                }

                return true;
            }
        });
        return;
    }

    m_dWindowRules.push_back(rule);
}

void CConfigManager::updateBlurredLS(const std::string& name, const bool forceBlur) {
    const bool  BYADDRESS = name.find("address:") == 0;
    std::string matchName = name;

    if (BYADDRESS) {
        matchName = matchName.substr(8);
    }

    for (auto& m : g_pCompositor->m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLayers) {
            for (auto& ls : lsl) {
                if (BYADDRESS) {
                    if (getFormat("0x%x", ls.get()) == matchName)
                        ls->forceBlur = forceBlur;
                } else if (ls->szNamespace == matchName)
                    ls->forceBlur = forceBlur;
            }
        }
    }
}

void CConfigManager::handleBlurLS(const std::string& command, const std::string& value) {
    if (value.find("remove,") == 0) {
        const auto TOREMOVE = removeBeginEndSpacesTabs(value.substr(7));
        if (std::erase_if(m_dBlurLSNamespaces, [&](const auto& other) { return other == TOREMOVE; }))
            updateBlurredLS(TOREMOVE, false);
        return;
    }

    m_dBlurLSNamespaces.emplace_back(value);
    updateBlurredLS(value, true);
}

void CConfigManager::handleDefaultWorkspace(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    m_mDefaultWorkspaces[ARGS[0]] = ARGS[1];
}

void CConfigManager::handleSubmap(const std::string& command, const std::string& submap) {
    if (submap == "reset")
        m_szCurrentSubmap = "";
    else
        m_szCurrentSubmap = submap;
}

void CConfigManager::handleSource(const std::string& command, const std::string& rawpath) {
    if (rawpath.length() < 2) {
        Debug::log(ERR, "source= path garbage");
        parseError = "source path " + rawpath + " bogus!";
        return;
    }

    auto value = absolutePath(rawpath, configCurrentPath);

    if (!std::filesystem::exists(value)) {
        Debug::log(ERR, "source= file doesnt exist");
        parseError = "source file " + value + " doesn't exist!";
        return;
    }

    configPaths.push_back(value);

    struct stat fileStat;
    int         err = stat(value.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config at %s, error %i: %s", value.c_str(), err, strerror(err));
        return;
    }

    configModifyTimes[value] = fileStat.st_mtime;

    std::ifstream ifs;
    ifs.open(value);
    std::string line    = "";
    int         linenum = 1;
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

void CConfigManager::handleBindWS(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    const auto FOUND = std::find_if(boundWorkspaces.begin(), boundWorkspaces.end(), [&](const auto& other) { return other.first == ARGS[0]; });

    if (FOUND != boundWorkspaces.end()) {
        FOUND->second = ARGS[1];
        return;
    }

    boundWorkspaces.push_back({ARGS[0], ARGS[1]});
}

void CConfigManager::handleEnv(const std::string& command, const std::string& value) {
    if (!isFirstLaunch)
        return;

    const auto ARGS = CVarList(value, 2);

    if (ARGS[0].empty()) {
        parseError = "env empty";
        return;
    }

    setenv(ARGS[0].c_str(), ARGS[1].c_str(), 1);

    if (command.back() == 'd') {
        // dbus
        const auto CMD = "systemctl --user import-environment " + ARGS[0] +
            " && hash dbus-update-activation-environment 2>/dev/null && "
            "dbus-update-activation-environment --systemd " +
            ARGS[0];
        handleRawExec("", CMD.c_str());
    }
}

std::string CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE, bool dynamic) {
    if (dynamic) {
        parseError      = "";
        currentCategory = "";
    }

    int needsLayoutRecalc = COMMAND == "monitor"; // 0 - no, 1 - yes, 2 - maybe

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
    } else if (COMMAND == "monitor")
        handleMonitor(COMMAND, VALUE);
    else if (COMMAND.find("bind") == 0)
        handleBind(COMMAND, VALUE);
    else if (COMMAND == "unbind")
        handleUnbind(COMMAND, VALUE);
    else if (COMMAND == "workspace")
        handleDefaultWorkspace(COMMAND, VALUE);
    else if (COMMAND == "windowrule")
        handleWindowRule(COMMAND, VALUE);
    else if (COMMAND == "windowrulev2")
        handleWindowRuleV2(COMMAND, VALUE);
    else if (COMMAND == "layerrule")
        handleLayerRule(COMMAND, VALUE);
    else if (COMMAND == "bezier")
        handleBezier(COMMAND, VALUE);
    else if (COMMAND == "animation")
        handleAnimation(COMMAND, VALUE);
    else if (COMMAND == "source")
        handleSource(COMMAND, VALUE);
    else if (COMMAND == "submap")
        handleSubmap(COMMAND, VALUE);
    else if (COMMAND == "blurls")
        handleBlurLS(COMMAND, VALUE);
    else if (COMMAND == "wsbind")
        handleBindWS(COMMAND, VALUE);
    else if (COMMAND.find("env") == 0)
        handleEnv(COMMAND, VALUE);
    else {
        configSetValueSafe(currentCategory + (currentCategory == "" ? "" : ":") + COMMAND, VALUE);
        needsLayoutRecalc = 2;
    }

    if (dynamic) {
        std::string retval = parseError;
        parseError         = "";

        // invalidate layouts if they changed
        if (needsLayoutRecalc) {
            if (needsLayoutRecalc == 1 || COMMAND.contains("gaps_") || COMMAND.find("dwindle:") == 0 || COMMAND.find("master:") == 0) {
                for (auto& m : g_pCompositor->m_vMonitors)
                    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);
            }
        }

        // Update window border colors
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        // manual crash
        if (configValues["debug:manual_crash"].intValue && !m_bManualCrashInitiated) {
            m_bManualCrashInitiated = true;
            if (g_pHyprNotificationOverlay) {
                g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CColor(0), 5000,
                                                            ICON_INFO);
            }
        } else if (m_bManualCrashInitiated && !configValues["debug:manual_crash"].intValue) {
            // cowabunga it is
            g_pHyprRenderer->initiateManualCrash();
        }

        return retval;
    }

    return parseError;
}

void CConfigManager::applyUserDefinedVars(std::string& line, const size_t equalsPlace) {
    auto dollarPlace = line.find_first_of('$', equalsPlace);

    while (dollarPlace != std::string::npos) {

        const auto STRAFTERDOLLAR = line.substr(dollarPlace + 1);
        bool       found          = false;
        for (auto& [var, value] : configDynamicVars) {
            if (STRAFTERDOLLAR.find(var) == 0) {
                line.replace(dollarPlace, var.length() + 1, value);
                found = true;
                break;
            }
        }

        if (!found) {
            // maybe env?
            for (auto& [var, value] : environmentVariables) {
                if (STRAFTERDOLLAR.find(var) == 0) {
                    line.replace(dollarPlace, var.length() + 1, value);
                    break;
                }
            }
        }

        dollarPlace = line.find_first_of('$', dollarPlace + 1);
    }
}

void CConfigManager::parseLine(std::string& line) {
    // first check if its not a comment
    if (line[0] == '#')
        return;

    // now, cut the comment off. ## is an escape.
    for (long unsigned int i = 1; i < line.length(); ++i) {
        if (line[i] == '#') {
            if (i + 1 < line.length() && line[i + 1] != '#') {
                line = line.substr(0, i);
                break; // no need to parse more
            }

            i++;
        }
    }

    size_t startPos = 0;
    while ((startPos = line.find("##", startPos)) != std::string::npos && startPos < line.length() - 1 && startPos > 0) {
        line.replace(startPos, 2, "#");
        startPos++;
    }

    line = removeBeginEndSpacesTabs(line);

    if (line.contains(" {")) {
        auto cat = line.substr(0, line.find(" {"));
        transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
        std::replace(cat.begin(), cat.end(), ' ', '-');
        if (currentCategory.length() != 0) {
            currentCategory.push_back(':');
            currentCategory.append(cat);
        } else {
            currentCategory = cat;
        }

        return;
    }

    if (line.contains("}") && currentCategory != "") {

        const auto LASTSEP = currentCategory.find_last_of(':');

        if (LASTSEP == std::string::npos || currentCategory.contains("device"))
            currentCategory = "";
        else
            currentCategory = currentCategory.substr(0, LASTSEP);

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
    const auto VALUE   = removeBeginEndSpacesTabs(line.substr(EQUALSPLACE + 1));
    //

    parseKeyword(COMMAND, VALUE);
}

void CConfigManager::loadConfigLoadVars() {
    Debug::log(LOG, "Reloading the config!");
    parseError      = ""; // reset the error
    currentCategory = ""; // reset the category

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
    boundWorkspaces.clear();
    setDefaultAnimationVars(); // reset anims

    // paths
    configPaths.clear();

    std::string              CONFIGPATH;

    static const char* const ENVHOME          = getenv("HOME");
    const std::string        CONFIGPARENTPATH = ENVHOME + (std::string) "/.config/hypr/";

    if (g_pCompositor->explicitConfigPath == "") {
        CONFIGPATH = CONFIGPARENTPATH + (ISDEBUG ? "hyprlandd.conf" : "hyprland.conf");
    } else {
        CONFIGPATH = g_pCompositor->explicitConfigPath;
    }

    configPaths.push_back(CONFIGPATH);

    std::ifstream ifs;
    ifs.open(CONFIGPATH);

    if (!ifs.good()) {
        if (g_pCompositor->explicitConfigPath == "") {
            Debug::log(WARN, "Config reading error. (No file? Attempting to generate, backing up old one if exists)");
            try {
                std::filesystem::rename(CONFIGPATH, CONFIGPATH + ".backup");
            } catch (...) { /* Probably doesn't exist */
            }

            try {
                if (!std::filesystem::is_directory(CONFIGPARENTPATH))
                    std::filesystem::create_directories(CONFIGPARENTPATH);
            } catch (...) {
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

    std::string line    = "";
    int         linenum = 1;
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
        g_pInputManager->setPointerConfigs();
        g_pInputManager->setTouchDeviceConfigs();
        g_pInputManager->setTabletConfigs();
    }

    if (!isFirstLaunch)
        g_pHyprOpenGL->m_bReloadScreenShader = true;

    // parseError will be displayed next frame
    if (parseError != "")
        g_pHyprError->queueCreate(parseError + "\nHyprland may not work correctly.", CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    else if (configValues["autogenerated"].intValue == 1)
        g_pHyprError->queueCreate("Warning: You're using an autogenerated config! (config file: " + CONFIGPATH + " )\nSUPER+Q -> kitty\nSUPER+M -> exit Hyprland",
                                  CColor(1.0, 1.0, 70.0 / 255.0, 1.0));
    else
        g_pHyprError->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    // ignore if nomonitorreload is set
    if (!isFirstLaunch && !m_bNoMonitorReload) {
        // check
        performMonitorReload();
        ensureMonitorStatus();
        ensureVRR();
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // update layout
    g_pLayoutManager->switchToLayout(configValues["general:layout"].strValue);

    // manual crash
    if (configValues["debug:manual_crash"].intValue && !m_bManualCrashInitiated) {
        m_bManualCrashInitiated = true;
        g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CColor(0), 5000, ICON_INFO);
    } else if (m_bManualCrashInitiated && !configValues["debug:manual_crash"].intValue) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    Debug::disableStdout = !configValues["debug:enable_stdout_logs"].intValue;

    for (auto& m : g_pCompositor->m_vMonitors) {
        // mark blur dirty
        g_pHyprOpenGL->markBlurDirtyForMonitor(m.get());

        // Force the compositor to fully re-render all monitors
        m->forceFullFrames = 2;
    }

    // Reset no monitor reload
    m_bNoMonitorReload = false;
}

void CConfigManager::tick() {
    std::string CONFIGPATH;
    if (g_pCompositor->explicitConfigPath.empty()) {
        static const char* const ENVHOME = getenv("HOME");
        CONFIGPATH                       = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprlandd.conf" : (std::string) "/.config/hypr/hyprland.conf");
    } else {
        CONFIGPATH = g_pCompositor->explicitConfigPath;
    }

    if (!std::filesystem::exists(CONFIGPATH)) {
        Debug::log(ERR, "Config doesn't exist??");
        return;
    }

    bool parse = false;

    for (auto& cf : configPaths) {
        struct stat fileStat;
        int         err = stat(cf.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at %s, error %i: %s", cf.c_str(), err, strerror(err));
            return;
        }

        // check if we need to reload cfg
        if (fileStat.st_mtime != configModifyTimes[cf] || m_bForceReload) {
            parse                 = true;
            configModifyTimes[cf] = fileStat.st_mtime;
        }
    }

    if (parse) {
        m_bForceReload = false;

        loadConfigLoadVars();
    }
}

std::mutex   configmtx;
SConfigValue CConfigManager::getConfigValueSafe(const std::string& val) {
    std::lock_guard<std::mutex> lg(configmtx);

    SConfigValue                copy = configValues[val];

    return copy;
}

SConfigValue CConfigManager::getConfigValueSafeDevice(const std::string& dev, const std::string& val) {
    std::lock_guard<std::mutex> lg(configmtx);

    const auto                  it = deviceConfigs.find(dev);

    if (it == deviceConfigs.end()) {
        Debug::log(ERR, "getConfigValueSafeDevice: No device config for %s found???", dev.c_str());
        return SConfigValue();
    }

    SConfigValue copy = it->second[val];

    // fallback if not set explicitly
    if (!copy.set) {
        for (auto& cv : configValues) {
            auto foundIt = cv.first.find(val);
            if (foundIt == std::string::npos)
                continue;

            if (cv.first == "input:" + val || cv.first == "input:touchpad:" + cv.first || cv.first == "input:touchdevice:" + val || cv.first == "input:tablet:" + cv.first ||
                cv.first == "input:tablet:" + val) {
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
    auto VAL = getConfigValueSafe(v).strValue;

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
    auto VAL = getConfigValueSafeDevice(dev, v).strValue;

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

void CConfigManager::setInt(const std::string& v, int val) {
    configValues[v].intValue = val;
}

void CConfigManager::setFloat(const std::string& v, float val) {
    configValues[v].floatValue = val;
}

void CConfigManager::setString(const std::string& v, const std::string& val) {
    configValues[v].strValue = val;
}

SMonitorRule CConfigManager::getMonitorRuleFor(const std::string& name, const std::string& displayName) {
    SMonitorRule* found = nullptr;

    for (auto& r : m_dMonitorRules) {
        if (r.name == name ||
            (r.name.find("desc:") == 0 &&
             (r.name.substr(5) == displayName || r.name.substr(5) == removeBeginEndSpacesTabs(displayName.substr(0, displayName.find_first_of('(')))))) {
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

    return SMonitorRule{.name = "", .resolution = Vector2D(0, 0), .offset = Vector2D(-1, -1), .scale = -1}; // 0, 0 is preferred and -1, -1 is auto
}

std::vector<SWindowRule> CConfigManager::getMatchingRules(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return std::vector<SWindowRule>();

    std::vector<SWindowRule> returns;

    std::string              title      = g_pXWaylandManager->getTitle(pWindow);
    std::string              appidclass = g_pXWaylandManager->getAppIDClass(pWindow);

    Debug::log(LOG, "Searching for matching rules for %s (title: %s)", appidclass.c_str(), title.c_str());

    for (auto& rule : m_dWindowRules) {
        // check if we have a matching rule
        if (!rule.v2) {
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
        } else {
            try {
                if (rule.szClass != "") {
                    std::regex RULECHECK(rule.szClass);

                    if (!std::regex_search(appidclass, RULECHECK))
                        continue;
                }

                if (rule.szTitle != "") {
                    std::regex RULECHECK(rule.szTitle);

                    if (!std::regex_search(title, RULECHECK))
                        continue;
                }

                if (rule.bX11 != -1) {
                    if (pWindow->m_bIsX11 != rule.bX11)
                        continue;
                }

                if (rule.bFloating != -1) {
                    if (pWindow->m_bIsFloating != rule.bFloating)
                        continue;
                }

                if (rule.bFullscreen != -1) {
                    if (pWindow->m_bIsFullscreen != rule.bFullscreen)
                        continue;
                }

                if (rule.bPinned != -1) {
                    if (pWindow->m_bPinned != rule.bPinned)
                        continue;
                }
            } catch (...) {
                Debug::log(ERR, "Regex error at %s", rule.szValue.c_str());
                continue;
            }
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule %s -> %s matched %x [%s]", rule.szRule.c_str(), rule.szValue.c_str(), pWindow, pWindow->m_szTitle.c_str());

        returns.push_back(rule);
    }

    const uint64_t PID          = pWindow->getPID();
    bool           anyExecFound = false;

    for (auto& er : execRequestedRules) {
        if (er.iPid == PID) {
            returns.push_back({er.szRule, "execRule"});
            anyExecFound = true;
        }
    }

    if (anyExecFound) // remove exec rules to unclog searches in the future, why have the garbage here.
        execRequestedRules.erase(std::remove_if(execRequestedRules.begin(), execRequestedRules.end(), [&](const SExecRequestedRule& other) { return other.iPid == PID; }));

    return returns;
}

std::vector<SLayerRule> CConfigManager::getMatchingRules(SLayerSurface* pLS) {
    std::vector<SLayerRule> returns;

    if (!pLS->layerSurface || pLS->fadingOut)
        return returns;

    for (auto& lr : m_dLayerRules) {
        if (lr.targetNamespace.find("address:0x") == 0) {
            if (getFormat("address:0x%x", pLS) != lr.targetNamespace)
                continue;
        } else {
            std::regex NSCHECK(lr.targetNamespace);

            if (!pLS->layerSurface->_namespace || !std::regex_search(pLS->layerSurface->_namespace, NSCHECK))
                continue;
        }

        // hit
        returns.push_back(lr);
    }

    if (pLS->layerSurface->_namespace && shouldBlurLS(pLS->layerSurface->_namespace))
        returns.push_back({pLS->layerSurface->_namespace, "blur"});

    return returns;
}

void CConfigManager::dispatchExecOnce() {
    if (firstExecDispatched || isFirstLaunch)
        return;

    // update dbus env
    if (g_pCompositor->m_sWLRSession)
        handleRawExec(
            "",
            "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP && hash dbus-update-activation-environment 2>/dev/null && "
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE");

    firstExecDispatched = true;

    for (auto& c : firstExecRequests) {
        handleRawExec("", c);
    }

    firstExecRequests.clear(); // free some kb of memory :P

    // set input, fixes some certain issues
    g_pInputManager->setKeyboardLayout();
    g_pInputManager->setPointerConfigs();
    g_pInputManager->setTouchDeviceConfigs();
    g_pInputManager->setTabletConfigs();

    // set ws names again
    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        wlr_ext_workspace_handle_v1_set_name(ws->m_pWlrHandle, ws->m_szName.c_str());
    }

    // check for user's possible errors with their setup and notify them if needed
    g_pCompositor->performUserChecks();
}

void CConfigManager::performMonitorReload() {

    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (!m->output)
            continue;

        auto rule = getMonitorRuleFor(m->szName, m->output->description ? m->output->description : "");

        if (!g_pHyprRenderer->applyMonitorRule(m.get(), &rule)) {
            overAgain = true;
            break;
        }

        // ensure mirror
        m->setMirror(rule.mirrorOf);

        g_pHyprRenderer->arrangeLayersForMonitor(m->ID);
    }

    if (overAgain)
        performMonitorReload();

    if (!g_pCompositor->m_vMonitors.empty()) // reset unsafe state if we have monitors
        g_pCompositor->m_bUnsafeState = false;

    m_bWantsMonitorReload = false;
}

SConfigValue* CConfigManager::getConfigValuePtr(const std::string& val) {
    return &configValues[val];
}

SConfigValue* CConfigManager::getConfigValuePtrSafe(const std::string& val) {
    const auto IT = configValues.find(val);

    if (IT == configValues.end()) {
        // maybe plugin
        for (auto& [pl, pMap] : pluginConfigs) {
            const auto PLIT = pMap->find(val);

            if (PLIT != pMap->end())
                return &PLIT->second;
        }

        return nullptr;
    }

    return &(IT->second);
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    auto copy = dev;
    std::replace(copy.begin(), copy.end(), ' ', '-');

    const auto it = deviceConfigs.find(copy);

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

void CConfigManager::ensureMonitorStatus() {
    for (auto& rm : g_pCompositor->m_vRealMonitors) {
        if (!rm->output)
            continue;

        auto rule = getMonitorRuleFor(rm->szName, rm->output->description ? rm->output->description : "");

        if (rule.disabled == rm->m_bEnabled) {
            rm->m_pThisWrap = &rm;
            g_pHyprRenderer->applyMonitorRule(rm.get(), &rule);
        }
    }
}

void CConfigManager::ensureVRR(CMonitor* pMonitor) {
    static auto* const PVRR = &getConfigValuePtr("misc:vrr")->intValue;

    static auto        ensureVRRForDisplay = [&](CMonitor* m) -> void {
        if (!m->output)
            return;

        if (*PVRR == 0) {
            if (m->vrrActive) {
                wlr_output_enable_adaptive_sync(m->output, 0);

                if (!wlr_output_commit(m->output)) {
                    Debug::log(ERR, "Couldn't commit output %s in ensureVRR -> false", m->output->name);
                }
            }
            m->vrrActive = false;
            return;
        } else if (*PVRR == 1) {
            if (!m->vrrActive) {
                wlr_output_enable_adaptive_sync(m->output, 1);

                if (!wlr_output_test(m->output)) {
                    Debug::log(LOG, "Pending output %s does not accept VRR.", m->output->name);
                    wlr_output_enable_adaptive_sync(m->output, 0);
                }

                if (!wlr_output_commit(m->output)) {
                    Debug::log(ERR, "Couldn't commit output %s in ensureVRR -> true", m->output->name);
                }
            }
            m->vrrActive = true;
            return;
        } else if (*PVRR == 2) {
            /* fullscreen */
            m->vrrActive = true;

            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m->activeWorkspace);

            if (!PWORKSPACE)
                return; // ???

            if (PWORKSPACE->m_bHasFullscreenWindow && m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED) {
                wlr_output_enable_adaptive_sync(m->output, 1);

                if (!wlr_output_test(m->output)) {
                    Debug::log(LOG, "Pending output %s does not accept VRR.", m->output->name);
                    wlr_output_enable_adaptive_sync(m->output, 0);
                }

                if (!wlr_output_commit(m->output)) {
                    Debug::log(ERR, "Couldn't commit output %s in ensureVRR -> true", m->output->name);
                }
            } else if (!PWORKSPACE->m_bHasFullscreenWindow && m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED) {
                wlr_output_enable_adaptive_sync(m->output, 0);

                if (!wlr_output_commit(m->output)) {
                    Debug::log(ERR, "Couldn't commit output %s in ensureVRR -> false", m->output->name);
                }
            }
        }
    };

    if (pMonitor) {
        ensureVRRForDisplay(pMonitor);
        return;
    }

    for (auto& m : g_pCompositor->m_vMonitors) {
        ensureVRRForDisplay(m.get());
    }
}

SAnimationPropertyConfig* CConfigManager::getAnimationPropertyConfig(const std::string& name) {
    return &animationConfig[name];
}

void CConfigManager::addParseError(const std::string& err) {
    if (parseError == "")
        parseError = err;

    g_pHyprError->queueCreate(parseError + "\nHyprland may not work correctly.", CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
}

CMonitor* CConfigManager::getBoundMonitorForWS(const std::string& wsname) {
    for (auto& [ws, mon] : boundWorkspaces) {
        const auto WSNAME = ws.find("name:") == 0 ? ws.substr(5) : ws;

        if (WSNAME == wsname) {
            return g_pCompositor->getMonitorFromString(mon);
        }
    }

    return nullptr;
}

std::string CConfigManager::getBoundMonitorStringForWS(const std::string& wsname) {
    for (auto& [ws, mon] : boundWorkspaces) {
        const auto WSNAME = ws.find("name:") == 0 ? ws.substr(5) : ws;

        if (WSNAME == wsname) {
            return mon;
        }
    }

    return "";
}

void CConfigManager::addExecRule(const SExecRequestedRule& rule) {
    execRequestedRules.push_back(rule);
}

ICustomConfigValueData::~ICustomConfigValueData() {
    ; // empty
}

std::unordered_map<std::string, SAnimationPropertyConfig> CConfigManager::getAnimationConfig() {
    return animationConfig;
}

void CConfigManager::addPluginConfigVar(HANDLE handle, const std::string& name, const SConfigValue& value) {
    auto CONFIGMAPIT = std::find_if(pluginConfigs.begin(), pluginConfigs.end(), [&](const auto& other) { return other.first == handle; });

    if (CONFIGMAPIT == pluginConfigs.end()) {
        pluginConfigs.emplace(
            std::pair<HANDLE, std::unique_ptr<std::unordered_map<std::string, SConfigValue>>>(handle, std::make_unique<std::unordered_map<std::string, SConfigValue>>()));
        CONFIGMAPIT = std::find_if(pluginConfigs.begin(), pluginConfigs.end(), [&](const auto& other) { return other.first == handle; });
    }

    (*CONFIGMAPIT->second)[name] = value;
}

void CConfigManager::removePluginConfig(HANDLE handle) {
    std::erase_if(pluginConfigs, [&](const auto& other) { return other.first == handle; });
}

std::string CConfigManager::getDefaultWorkspaceFor(const std::string& name) {
    const auto IT = std::find_if(m_mDefaultWorkspaces.begin(), m_mDefaultWorkspaces.end(), [&](const auto& other) { return other.first == name; });
    if (IT == m_mDefaultWorkspaces.end())
        return "";
    return IT->second;
}