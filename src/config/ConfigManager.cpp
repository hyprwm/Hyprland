#include "ConfigManager.hpp"
#include "../managers/KeybindManager.hpp"

#include "../render/decorations/CHyprGroupBarDecoration.hpp"

#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

extern "C" char** environ;

CConfigManager::CConfigManager() {
    configValues["general:col.active_border"].data         = std::make_shared<CGradientValueData>(0xffffffff);
    configValues["general:col.inactive_border"].data       = std::make_shared<CGradientValueData>(0xff444444);
    configValues["general:col.nogroup_border"].data        = std::make_shared<CGradientValueData>(0xffffaaff);
    configValues["general:col.nogroup_border_active"].data = std::make_shared<CGradientValueData>(0xffff00ff);

    configValues["group:col.border_active"].data          = std::make_shared<CGradientValueData>(0x66ffff00);
    configValues["group:col.border_inactive"].data        = std::make_shared<CGradientValueData>(0x66777700);
    configValues["group:col.border_locked_active"].data   = std::make_shared<CGradientValueData>(0x66ff5500);
    configValues["group:col.border_locked_inactive"].data = std::make_shared<CGradientValueData>(0x66775500);

    configValues["group:groupbar:col.active"].data          = std::make_shared<CGradientValueData>(0x66ffff00);
    configValues["group:groupbar:col.inactive"].data        = std::make_shared<CGradientValueData>(0x66777700);
    configValues["group:groupbar:col.locked_active"].data   = std::make_shared<CGradientValueData>(0x66ff5500);
    configValues["group:groupbar:col.locked_inactive"].data = std::make_shared<CGradientValueData>(0x66775500);

    Debug::log(LOG, "NOTE: further logs to stdout / logfile are disabled by default. Use debug:disable_logs and debug:enable_stdout_logs to override this.");

    setDefaultVars();
    setDefaultAnimationVars();

    configPaths.emplace_back(getMainConfigPath());

    Debug::disableLogs = &configValues["debug:disable_logs"].intValue;
    Debug::disableTime = &configValues["debug:disable_time"].intValue;

    populateEnvironment();
}

std::string CConfigManager::getConfigDir() {
    static const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");

    if (xdgConfigHome && std::filesystem::path(xdgConfigHome).is_absolute())
        return xdgConfigHome;

    return getenv("HOME") + std::string("/.config");
}

std::string CConfigManager::getMainConfigPath() {
    if (!g_pCompositor->explicitConfigPath.empty())
        return g_pCompositor->explicitConfigPath;

    return getConfigDir() + "/hypr/" + (ISDEBUG ? "hyprlandd.conf" : "hyprland.conf");
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
    configValues["general:border_part_of_window"].intValue = 1;
    configValues["general:gaps_in"].intValue               = 5;
    configValues["general:gaps_out"].intValue              = 20;
    configValues["general:gaps_workspaces"].intValue       = 0;
    ((CGradientValueData*)configValues["general:col.active_border"].data.get())->reset(0xffffffff);
    ((CGradientValueData*)configValues["general:col.inactive_border"].data.get())->reset(0xff444444);
    ((CGradientValueData*)configValues["general:col.nogroup_border"].data.get())->reset(0xff444444);
    ((CGradientValueData*)configValues["general:col.nogroup_border_active"].data.get())->reset(0xffff00ff);
    configValues["general:cursor_inactive_timeout"].intValue = 0;
    configValues["general:no_cursor_warps"].intValue         = 0;
    configValues["general:no_focus_fallback"].intValue       = 0;
    configValues["general:resize_on_border"].intValue        = 0;
    configValues["general:extend_border_grab_area"].intValue = 15;
    configValues["general:hover_icon_on_border"].intValue    = 1;
    configValues["general:layout"].strValue                  = "dwindle";
    configValues["general:allow_tearing"].intValue           = 0;

    configValues["misc:disable_hyprland_logo"].intValue            = 0;
    configValues["misc:disable_splash_rendering"].intValue         = 0;
    configValues["misc:force_default_wallpaper"].intValue          = -1;
    configValues["misc:vfr"].intValue                              = 1;
    configValues["misc:vrr"].intValue                              = 0;
    configValues["misc:mouse_move_enables_dpms"].intValue          = 0;
    configValues["misc:key_press_enables_dpms"].intValue           = 0;
    configValues["misc:always_follow_on_dnd"].intValue             = 1;
    configValues["misc:layers_hog_keyboard_focus"].intValue        = 1;
    configValues["misc:animate_manual_resizes"].intValue           = 0;
    configValues["misc:animate_mouse_windowdragging"].intValue     = 0;
    configValues["misc:disable_autoreload"].intValue               = 0;
    configValues["misc:enable_swallow"].intValue                   = 0;
    configValues["misc:swallow_regex"].strValue                    = STRVAL_EMPTY;
    configValues["misc:swallow_exception_regex"].strValue          = STRVAL_EMPTY;
    configValues["misc:focus_on_activate"].intValue                = 0;
    configValues["misc:no_direct_scanout"].intValue                = 1;
    configValues["misc:hide_cursor_on_touch"].intValue             = 1;
    configValues["misc:mouse_move_focuses_monitor"].intValue       = 1;
    configValues["misc:render_ahead_of_time"].intValue             = 0;
    configValues["misc:render_ahead_safezone"].intValue            = 1;
    configValues["misc:cursor_zoom_factor"].floatValue             = 1.f;
    configValues["misc:cursor_zoom_rigid"].intValue                = 0;
    configValues["misc:allow_session_lock_restore"].intValue       = 0;
    configValues["misc:close_special_on_empty"].intValue           = 1;
    configValues["misc:background_color"].intValue                 = 0xff111111;
    configValues["misc:new_window_takes_over_fullscreen"].intValue = 0;

    ((CGradientValueData*)configValues["group:col.border_active"].data.get())->reset(0x66ffff00);
    ((CGradientValueData*)configValues["group:col.border_inactive"].data.get())->reset(0x66777700);
    ((CGradientValueData*)configValues["group:col.border_locked_active"].data.get())->reset(0x66ff5500);
    ((CGradientValueData*)configValues["group:col.border_locked_inactive"].data.get())->reset(0x66775500);

    configValues["group:insert_after_current"].intValue = 1;
    configValues["group:focus_removed_window"].intValue = 1;

    configValues["group:groupbar:enabled"].intValue       = 1;
    configValues["group:groupbar:font_family"].strValue   = "Sans";
    configValues["group:groupbar:font_size"].intValue     = 8;
    configValues["group:groupbar:gradients"].intValue     = 1;
    configValues["group:groupbar:height"].intValue        = 14;
    configValues["group:groupbar:priority"].intValue      = 3;
    configValues["group:groupbar:render_titles"].intValue = 1;
    configValues["group:groupbar:scrolling"].intValue     = 1;
    configValues["group:groupbar:text_color"].intValue    = 0xffffffff;

    ((CGradientValueData*)configValues["group:groupbar:col.active"].data.get())->reset(0x66ffff00);
    ((CGradientValueData*)configValues["group:groupbar:col.inactive"].data.get())->reset(0x66777700);
    ((CGradientValueData*)configValues["group:groupbar:col.locked_active"].data.get())->reset(0x66ff5500);
    ((CGradientValueData*)configValues["group:groupbar:col.locked_inactive"].data.get())->reset(0x66775500);

    configValues["debug:int"].intValue                  = 0;
    configValues["debug:log_damage"].intValue           = 0;
    configValues["debug:overlay"].intValue              = 0;
    configValues["debug:damage_blink"].intValue         = 0;
    configValues["debug:disable_logs"].intValue         = 1;
    configValues["debug:disable_time"].intValue         = 1;
    configValues["debug:enable_stdout_logs"].intValue   = 0;
    configValues["debug:damage_tracking"].intValue      = DAMAGE_TRACKING_FULL;
    configValues["debug:manual_crash"].intValue         = 0;
    configValues["debug:suppress_errors"].intValue      = 0;
    configValues["debug:watchdog_timeout"].intValue     = 5;
    configValues["debug:disable_scale_checks"].intValue = 0;

    configValues["decoration:rounding"].intValue                  = 0;
    configValues["decoration:blur:enabled"].intValue              = 1;
    configValues["decoration:blur:size"].intValue                 = 8;
    configValues["decoration:blur:passes"].intValue               = 1;
    configValues["decoration:blur:ignore_opacity"].intValue       = 0;
    configValues["decoration:blur:new_optimizations"].intValue    = 1;
    configValues["decoration:blur:xray"].intValue                 = 0;
    configValues["decoration:blur:contrast"].floatValue           = 0.8916;
    configValues["decoration:blur:brightness"].floatValue         = 1.0;
    configValues["decoration:blur:vibrancy"].floatValue           = 0.1696;
    configValues["decoration:blur:vibrancy_darkness"].floatValue  = 0.0;
    configValues["decoration:blur:noise"].floatValue              = 0.0117;
    configValues["decoration:blur:special"].intValue              = 0;
    configValues["decoration:blur:popups"].intValue               = 0;
    configValues["decoration:blur:popups_ignorealpha"].floatValue = 0.2;
    configValues["decoration:active_opacity"].floatValue          = 1;
    configValues["decoration:inactive_opacity"].floatValue        = 1;
    configValues["decoration:fullscreen_opacity"].floatValue      = 1;
    configValues["decoration:no_blur_on_oversized"].intValue      = 0;
    configValues["decoration:drop_shadow"].intValue               = 1;
    configValues["decoration:shadow_range"].intValue              = 4;
    configValues["decoration:shadow_render_power"].intValue       = 3;
    configValues["decoration:shadow_ignore_window"].intValue      = 1;
    configValues["decoration:shadow_offset"].vecValue             = Vector2D();
    configValues["decoration:shadow_scale"].floatValue            = 1.f;
    configValues["decoration:col.shadow"].intValue                = 0xee1a1a1a;
    configValues["decoration:col.shadow_inactive"].intValue       = INT_MAX;
    configValues["decoration:dim_inactive"].intValue              = 0;
    configValues["decoration:dim_strength"].floatValue            = 0.5f;
    configValues["decoration:dim_special"].floatValue             = 0.2f;
    configValues["decoration:dim_around"].floatValue              = 0.4f;
    configValues["decoration:screen_shader"].strValue             = STRVAL_EMPTY;

    configValues["dwindle:pseudotile"].intValue                   = 0;
    configValues["dwindle:force_split"].intValue                  = 0;
    configValues["dwindle:permanent_direction_override"].intValue = 0;
    configValues["dwindle:preserve_split"].intValue               = 0;
    configValues["dwindle:special_scale_factor"].floatValue       = 1.f;
    configValues["dwindle:split_width_multiplier"].floatValue     = 1.0f;
    configValues["dwindle:no_gaps_when_only"].intValue            = 0;
    configValues["dwindle:use_active_for_splits"].intValue        = 1;
    configValues["dwindle:default_split_ratio"].floatValue        = 1.f;
    configValues["dwindle:smart_split"].intValue                  = 0;
    configValues["dwindle:smart_resizing"].intValue               = 1;

    configValues["master:special_scale_factor"].floatValue = 1.f;
    configValues["master:mfact"].floatValue                = 0.55f;
    configValues["master:new_is_master"].intValue          = 1;
    configValues["master:always_center_master"].intValue   = 0;
    configValues["master:new_on_top"].intValue             = 0;
    configValues["master:no_gaps_when_only"].intValue      = 0;
    configValues["master:orientation"].strValue            = "left";
    configValues["master:inherit_fullscreen"].intValue     = 1;
    configValues["master:allow_small_split"].intValue      = 0;
    configValues["master:smart_resizing"].intValue         = 1;
    configValues["master:drop_at_cursor"].intValue         = 1;

    configValues["animations:enabled"].intValue                = 1;
    configValues["animations:first_launch_animation"].intValue = 1;

    configValues["input:follow_mouse"].intValue                     = 1;
    configValues["input:mouse_refocus"].intValue                    = 1;
    configValues["input:special_fallthrough"].intValue              = 0;
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
    configValues["input:scroll_button_lock"].intValue               = 0;
    configValues["input:scroll_points"].strValue                    = STRVAL_EMPTY;
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
    configValues["input:touchdevice:enabled"].intValue              = 1;
    configValues["input:tablet:transform"].intValue                 = 0;
    configValues["input:tablet:output"].strValue                    = STRVAL_EMPTY;
    configValues["input:tablet:region_position"].vecValue           = Vector2D();
    configValues["input:tablet:region_size"].vecValue               = Vector2D();
    configValues["input:tablet:relative_input"].intValue            = 0;

    configValues["binds:pass_mouse_when_bound"].intValue       = 0;
    configValues["binds:scroll_event_delay"].intValue          = 300;
    configValues["binds:workspace_back_and_forth"].intValue    = 0;
    configValues["binds:allow_workspace_cycles"].intValue      = 0;
    configValues["binds:workspace_center_on"].intValue         = 1;
    configValues["binds:focus_preferred_method"].intValue      = 0;
    configValues["binds:ignore_group_lock"].intValue           = 0;
    configValues["binds:movefocus_cycles_fullscreen"].intValue = 1;

    configValues["gestures:workspace_swipe"].intValue                          = 0;
    configValues["gestures:workspace_swipe_fingers"].intValue                  = 3;
    configValues["gestures:workspace_swipe_distance"].intValue                 = 300;
    configValues["gestures:workspace_swipe_invert"].intValue                   = 1;
    configValues["gestures:workspace_swipe_min_speed_to_force"].intValue       = 30;
    configValues["gestures:workspace_swipe_cancel_ratio"].floatValue           = 0.5f;
    configValues["gestures:workspace_swipe_create_new"].intValue               = 1;
    configValues["gestures:workspace_swipe_direction_lock"].intValue           = 1;
    configValues["gestures:workspace_swipe_direction_lock_threshold"].intValue = 10;
    configValues["gestures:workspace_swipe_forever"].intValue                  = 0;
    configValues["gestures:workspace_swipe_numbered"].intValue                 = 0;
    configValues["gestures:workspace_swipe_use_r"].intValue                    = 0;

    configValues["xwayland:use_nearest_neighbor"].intValue = 1;
    configValues["xwayland:force_zero_scaling"].intValue   = 0;

    configValues["opengl:nvidia_anti_flicker"].intValue = 1;

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
    cfgValues["scroll_button_lock"].intValue      = 0;
    cfgValues["scroll_points"].strValue           = STRVAL_EMPTY;
    cfgValues["transform"].intValue               = 0;
    cfgValues["output"].strValue                  = STRVAL_EMPTY;
    cfgValues["enabled"].intValue                 = 1;          // only for mice, touchpads, and touchdevices
    cfgValues["region_position"].vecValue         = Vector2D(); // only for tablets
    cfgValues["region_size"].vecValue             = Vector2D(); // only for tablets
    cfgValues["relative_input"].intValue          = 0;          // only for tablets
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

    const std::string CONFIGPATH = getMainConfigPath();

    struct stat       fileStat;
    int               err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at statting config, error {}", errno);
    }

    configModifyTimes[CONFIGPATH] = fileStat.st_mtime;

    isFirstLaunch = false;
}

void CConfigManager::configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (!configValues.contains(COMMAND)) {
        if (!COMMAND.starts_with("device:") /* devices parsed later */ && !COMMAND.starts_with("plugin:") /* plugins parsed later */) {
            if (COMMAND[0] == '$') {
                // register a dynamic var
                bool found = false;
                for (auto& [var, val] : configDynamicVars) {
                    if (var == COMMAND.substr(1)) {
                        Debug::log(LOG, "Registered new value for dynamic var \"{}\" -> {}", COMMAND, VALUE);
                        val   = VALUE;
                        found = true;
                    }
                }

                if (!found) {
                    Debug::log(LOG, "Registered dynamic var \"{}\" -> {}", COMMAND, VALUE);
                    configDynamicVars.emplace_back(std::make_pair<>(COMMAND.substr(1), VALUE));
                    std::sort(configDynamicVars.begin(), configDynamicVars.end(), [&](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
                }
            } else {
                parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
            }

            return;
        }
    }

    SConfigValue* CONFIGENTRY = nullptr;

    if (COMMAND.starts_with("device:")) {
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
    } else if (COMMAND.starts_with("plugin:")) {
        for (auto& [handle, pMap] : pluginConfigs) {
            auto it = std::find_if(pMap->begin(), pMap->end(), [&](const auto& other) { return other.first == COMMAND; });
            if (it == pMap->end()) {
                continue; // May be in another plugin
            }

            CONFIGENTRY = &it->second;
        }

        if (!CONFIGENTRY) {
            m_vFailedPluginConfigValues.emplace_back(std::make_pair<>(COMMAND, VALUE));
            return; // silent ignore
        }
    } else {
        CONFIGENTRY = &configValues.at(COMMAND);
    }

    CONFIGENTRY->set = true;

    if (CONFIGENTRY->intValue != -INT64_MAX) {
        try {
            CONFIGENTRY->intValue = configStringToInt(VALUE);
        } catch (std::exception& e) {
            Debug::log(WARN, "Error reading value of {}", COMMAND);
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. " + e.what();
        }
    } else if (CONFIGENTRY->floatValue != -__FLT_MAX__) {
        try {
            CONFIGENTRY->floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of {}", COMMAND);
            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY->strValue != "") {
        try {
            CONFIGENTRY->strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of {}", COMMAND);
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
                Debug::log(WARN, "Error reading value of {}", COMMAND);
                parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
            }
        } catch (...) {
            Debug::log(WARN, "Error reading value of {}", COMMAND);
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
                            Debug::log(WARN, "Error reading value of {}", COMMAND);
                            parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
                        }

                        break;
                    }

                    if (data->m_vColors.size() >= 10) {
                        Debug::log(WARN, "Error reading value of {}", COMMAND);
                        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. Max colors in a gradient is 10.";
                        break;
                    }

                    try {
                        data->m_vColors.push_back(CColor(configStringToInt(var)));
                    } catch (std::exception& e) {
                        Debug::log(WARN, "Error reading value of {}", COMMAND);
                        parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">. " + e.what();
                    }
                }

                if (data->m_vColors.size() == 0) {
                    Debug::log(WARN, "Error reading value of {}", COMMAND);
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

    if (COMMAND == "decoration:screen_shader" && VALUE != STRVAL_EMPTY) {
        const auto PATH = absolutePath(VALUE, configCurrentPath);

        configPaths.push_back(PATH);

        struct stat fileStat;
        int         err = stat(PATH.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at {}, error {}: {}", PATH, err, strerror(err));
            return;
        }

        configModifyTimes[PATH] = fileStat.st_mtime;
    }
}

void CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.
    g_pKeybindManager->spawn(args);
}

static bool parseModeLine(const std::string& modeline, drmModeModeInfo& mode) {
    auto args = CVarList(modeline, 0, 's');

    auto keyword = args[0];
    std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);

    if (keyword != "modeline")
        return false;

    if (args.size() < 10) {
        Debug::log(ERR, "modeline parse error: expected at least 9 arguments, got {}", args.size() - 1);
        return false;
    }

    int argno = 1;

    mode.type        = DRM_MODE_TYPE_USERDEF;
    mode.clock       = std::stof(args[argno++]) * 1000;
    mode.hdisplay    = std::stoi(args[argno++]);
    mode.hsync_start = std::stoi(args[argno++]);
    mode.hsync_end   = std::stoi(args[argno++]);
    mode.htotal      = std::stoi(args[argno++]);
    mode.vdisplay    = std::stoi(args[argno++]);
    mode.vsync_start = std::stoi(args[argno++]);
    mode.vsync_end   = std::stoi(args[argno++]);
    mode.vtotal      = std::stoi(args[argno++]);
    mode.vrefresh    = mode.clock * 1000.0 * 1000.0 / mode.htotal / mode.vtotal;

    // clang-format off
    static std::unordered_map<std::string, uint32_t> flagsmap = {
        {"+hsync", DRM_MODE_FLAG_PHSYNC},
        {"-hsync", DRM_MODE_FLAG_NHSYNC},
        {"+vsync", DRM_MODE_FLAG_PVSYNC},
        {"-vsync", DRM_MODE_FLAG_NVSYNC},
        {"Interlace", DRM_MODE_FLAG_INTERLACE},
    };
    // clang-format on

    for (; argno < static_cast<int>(args.size()); argno++) {
        auto key = args[argno];
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        auto it = flagsmap.find(key);

        if (it != flagsmap.end())
            mode.flags |= it->second;
        else
            Debug::log(ERR, "invalid flag {} in modeline", it->first);
    }

    snprintf(mode.name, sizeof(mode.name), "%dx%d@%d", mode.hdisplay, mode.vdisplay, mode.vrefresh / 1000);

    return true;
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
                Debug::log(ERR, "invalid transform {} in monitor", TSF);
                parseError = "invalid transform";
                return;
            }

            const auto TRANSFORM = (wl_output_transform)TSF;

            // overwrite if exists
            for (auto& r : m_dMonitorRules) {
                if (r.name == newrule.name) {
                    r.transform = TRANSFORM;
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

    if (ARGS[1].starts_with("pref")) {
        newrule.resolution = Vector2D();
    } else if (ARGS[1].starts_with("highrr")) {
        newrule.resolution = Vector2D(-1, -1);
    } else if (ARGS[1].starts_with("highres")) {
        newrule.resolution = Vector2D(-1, -2);
    } else if (parseModeLine(ARGS[1], newrule.drmMode)) {
        newrule.resolution  = Vector2D(newrule.drmMode.hdisplay, newrule.drmMode.vdisplay);
        newrule.refreshRate = newrule.drmMode.vrefresh / 1000;
    } else {
        newrule.resolution.x = stoi(ARGS[1].substr(0, ARGS[1].find_first_of('x')));
        newrule.resolution.y = stoi(ARGS[1].substr(ARGS[1].find_first_of('x') + 1, ARGS[1].find_first_of('@')));

        if (ARGS[1].contains("@"))
            newrule.refreshRate = stof(ARGS[1].substr(ARGS[1].find_first_of('@') + 1));
    }

    if (ARGS[2].starts_with("auto")) {
        newrule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
    } else {
        newrule.offset.x = stoi(ARGS[2].substr(0, ARGS[2].find_first_of('x')));
        newrule.offset.y = stoi(ARGS[2].substr(ARGS[2].find_first_of('x') + 1));
    }

    if (ARGS[3].starts_with("auto")) {
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
        } else if (ARGS[argno] == "vrr") {
            newrule.vrr = std::stoi(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "workspace") {
            std::string    name = "";
            int            wsId = getWorkspaceIDFromString(ARGS[argno + 1], name);

            SWorkspaceRule wsRule;
            wsRule.monitor         = newrule.name;
            wsRule.workspaceString = ARGS[argno + 1];
            wsRule.workspaceName   = name;
            wsRule.workspaceId     = wsId;

            m_dWorkspaceRules.emplace_back(wsRule);
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

    if (PANIM->second.internalEnabled) {
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
    }

    // now, check for children, recursively
    setAnimForChildren(&PANIM->second);
}

void CConfigManager::handleBind(const std::string& command, const std::string& value) {
    // example:
    // bind[fl]=SUPER,G,exec,dmenu_run <args>

    // flags
    bool       locked       = false;
    bool       release      = false;
    bool       repeat       = false;
    bool       mouse        = false;
    bool       nonConsuming = false;
    bool       transparent  = false;
    bool       ignoreMods   = false;
    const auto BINDARGS     = command.substr(4);

    for (auto& arg : BINDARGS) {
        if (arg == 'l') {
            locked = true;
        } else if (arg == 'r') {
            release = true;
        } else if (arg == 'e') {
            repeat = true;
        } else if (arg == 'm') {
            mouse = true;
        } else if (arg == 'n') {
            nonConsuming = true;
        } else if (arg == 't') {
            transparent = true;
        } else if (arg == 'i') {
            ignoreMods = true;
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
            g_pKeybindManager->addKeybind(
                SKeybind{"", std::stoi(KEY), MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse, nonConsuming, transparent, ignoreMods});
        else if (KEY.starts_with("code:") && isNumber(KEY.substr(5)))
            g_pKeybindManager->addKeybind(
                SKeybind{"", std::stoi(KEY.substr(5)), MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse, nonConsuming, transparent, ignoreMods});
        else
            g_pKeybindManager->addKeybind(SKeybind{KEY, 0, MOD, HANDLER, COMMAND, locked, m_szCurrentSubmap, release, repeat, mouse, nonConsuming, transparent, ignoreMods});
    }
}

void CConfigManager::handleUnbind(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    const auto MOD = g_pKeybindManager->stringToModMask(ARGS[0]);

    const auto KEY = ARGS[1];

    g_pKeybindManager->removeKeybind(MOD, KEY);
}

bool windowRuleValid(const std::string& RULE) {
    return RULE == "float" || RULE == "tile" || RULE.starts_with("opacity") || RULE.starts_with("move") || RULE.starts_with("size") || RULE.starts_with("minsize") ||
        RULE.starts_with("maxsize") || RULE.starts_with("pseudo") || RULE.starts_with("monitor") || RULE.starts_with("idleinhibit") || RULE == "nofocus" || RULE == "noblur" ||
        RULE == "noshadow" || RULE == "nodim" || RULE == "noborder" || RULE == "opaque" || RULE == "forceinput" || RULE == "fullscreen" || RULE == "fakefullscreen" ||
        RULE == "nomaxsize" || RULE == "pin" || RULE == "noanim" || RULE == "dimaround" || RULE == "windowdance" || RULE == "maximize" || RULE == "keepaspectratio" ||
        RULE.starts_with("animation") || RULE.starts_with("rounding") || RULE.starts_with("workspace") || RULE.starts_with("bordercolor") || RULE == "forcergbx" ||
        RULE == "noinitialfocus" || RULE == "stayfocused" || RULE.starts_with("bordersize") || RULE.starts_with("xray") || RULE.starts_with("center") ||
        RULE.starts_with("group") || RULE == "immediate" || RULE == "nearestneighbor" || RULE.starts_with("suppressevent");
}

bool layerRuleValid(const std::string& RULE) {
    return RULE == "noanim" || RULE == "blur" || RULE.starts_with("ignorealpha") || RULE.starts_with("ignorezero") || RULE.starts_with("xray");
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
        Debug::log(ERR, "Invalid rule found: {}", RULE);
        parseError = "Invalid rule found: " + RULE;
        return;
    }

    if (RULE.starts_with("size") || RULE.starts_with("maxsize") || RULE.starts_with("minsize"))
        m_dWindowRules.push_front({RULE, VALUE});
    else
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
        Debug::log(ERR, "Invalid rule found: {}", RULE);
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
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = value.substr(value.find_first_of(',') + 1);

    if (!windowRuleValid(RULE) && RULE != "unset") {
        Debug::log(ERR, "Invalid rulev2 found: {}", RULE);
        parseError = "Invalid rulev2 found: " + RULE;
        return;
    }

    // now we estract shit from the value
    SWindowRule rule;
    rule.v2      = true;
    rule.szRule  = RULE;
    rule.szValue = VALUE;

    const auto TITLEPOS        = VALUE.find("title:");
    const auto CLASSPOS        = VALUE.find("class:");
    const auto INITIALTITLEPOS = VALUE.find("initialTitle:");
    const auto INITIALCLASSPOS = VALUE.find("initialClass:");
    const auto X11POS          = VALUE.find("xwayland:");
    const auto FLOATPOS        = VALUE.find("floating:");
    const auto FULLSCREENPOS   = VALUE.find("fullscreen:");
    const auto PINNEDPOS       = VALUE.find("pinned:");
    const auto FOCUSPOS        = VALUE.find("focus:");
    const auto ONWORKSPACEPOS  = VALUE.find("onworkspace:");

    // find workspacepos that isn't onworkspacepos
    size_t WORKSPACEPOS = std::string::npos;
    size_t currentPos   = VALUE.find("workspace:");
    while (currentPos != std::string::npos) {
        if (currentPos == 0 || VALUE[currentPos - 1] != 'n') {
            WORKSPACEPOS = currentPos;
            break;
        }
        currentPos = VALUE.find("workspace:", currentPos + 1);
    }

    if (TITLEPOS == std::string::npos && CLASSPOS == std::string::npos && INITIALTITLEPOS == std::string::npos && INITIALCLASSPOS == std::string::npos &&
        X11POS == std::string::npos && FLOATPOS == std::string::npos && FULLSCREENPOS == std::string::npos && PINNEDPOS == std::string::npos && WORKSPACEPOS == std::string::npos &&
        FOCUSPOS == std::string::npos && ONWORKSPACEPOS == std::string::npos) {
        Debug::log(ERR, "Invalid rulev2 syntax: {}", VALUE);
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
        if (INITIALTITLEPOS > pos && INITIALTITLEPOS < min)
            min = INITIALTITLEPOS;
        if (INITIALCLASSPOS > pos && INITIALCLASSPOS < min)
            min = INITIALCLASSPOS;
        if (X11POS > pos && X11POS < min)
            min = X11POS;
        if (FLOATPOS > pos && FLOATPOS < min)
            min = FLOATPOS;
        if (FULLSCREENPOS > pos && FULLSCREENPOS < min)
            min = FULLSCREENPOS;
        if (PINNEDPOS > pos && PINNEDPOS < min)
            min = PINNEDPOS;
        if (ONWORKSPACEPOS > pos && ONWORKSPACEPOS < min)
            min = ONWORKSPACEPOS;
        if (WORKSPACEPOS > pos && WORKSPACEPOS < min)
            min = WORKSPACEPOS;
        if (FOCUSPOS > pos && FOCUSPOS < min)
            min = FOCUSPOS;

        result = result.substr(0, min - pos);

        result = removeBeginEndSpacesTabs(result);

        if (result.back() == ',')
            result.pop_back();

        return result;
    };

    if (CLASSPOS != std::string::npos)
        rule.szClass = extract(CLASSPOS + 6);

    if (TITLEPOS != std::string::npos)
        rule.szTitle = extract(TITLEPOS + 6);

    if (INITIALCLASSPOS != std::string::npos)
        rule.szInitialClass = extract(INITIALCLASSPOS + 13);

    if (INITIALTITLEPOS != std::string::npos)
        rule.szInitialTitle = extract(INITIALTITLEPOS + 13);

    if (X11POS != std::string::npos)
        rule.bX11 = extract(X11POS + 9) == "1" ? 1 : 0;

    if (FLOATPOS != std::string::npos)
        rule.bFloating = extract(FLOATPOS + 9) == "1" ? 1 : 0;

    if (FULLSCREENPOS != std::string::npos)
        rule.bFullscreen = extract(FULLSCREENPOS + 11) == "1" ? 1 : 0;

    if (PINNEDPOS != std::string::npos)
        rule.bPinned = extract(PINNEDPOS + 7) == "1" ? 1 : 0;

    if (WORKSPACEPOS != std::string::npos)
        rule.szWorkspace = extract(WORKSPACEPOS + 10);

    if (FOCUSPOS != std::string::npos)
        rule.bFocus = extract(FOCUSPOS + 6) == "1" ? 1 : 0;

    if (ONWORKSPACEPOS != std::string::npos)
        rule.iOnWorkspace = configStringToInt(extract(ONWORKSPACEPOS + 12));

    if (RULE == "unset") {
        std::erase_if(m_dWindowRules, [&](const SWindowRule& other) {
            if (!other.v2) {
                return other.szClass == rule.szClass && !rule.szClass.empty();
            } else {
                if (!rule.szClass.empty() && rule.szClass != other.szClass)
                    return false;

                if (!rule.szTitle.empty() && rule.szTitle != other.szTitle)
                    return false;

                if (!rule.szInitialClass.empty() && rule.szInitialClass != other.szInitialClass)
                    return false;

                if (!rule.szInitialTitle.empty() && rule.szInitialTitle != other.szInitialTitle)
                    return false;

                if (rule.bX11 != -1 && rule.bX11 != other.bX11)
                    return false;

                if (rule.bFloating != -1 && rule.bFloating != other.bFloating)
                    return false;

                if (rule.bFullscreen != -1 && rule.bFullscreen != other.bFullscreen)
                    return false;

                if (rule.bPinned != -1 && rule.bPinned != other.bPinned)
                    return false;

                if (!rule.szWorkspace.empty() && rule.szWorkspace != other.szWorkspace)
                    return false;

                if (rule.bFocus != -1 && rule.bFocus != other.bFocus)
                    return false;

                if (rule.iOnWorkspace != -1 && rule.iOnWorkspace != other.iOnWorkspace)
                    return false;

                return true;
            }
        });
        return;
    }

    if (RULE.starts_with("size") || RULE.starts_with("maxsize") || RULE.starts_with("minsize"))
        m_dWindowRules.push_front(rule);
    else
        m_dWindowRules.push_back(rule);
}

void CConfigManager::updateBlurredLS(const std::string& name, const bool forceBlur) {
    const bool  BYADDRESS = name.starts_with("address:");
    std::string matchName = name;

    if (BYADDRESS) {
        matchName = matchName.substr(8);
    }

    for (auto& m : g_pCompositor->m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLayers) {
            for (auto& ls : lsl) {
                if (BYADDRESS) {
                    if (std::format("0x{:x}", (uintptr_t)ls.get()) == matchName)
                        ls->forceBlur = forceBlur;
                } else if (ls->szNamespace == matchName)
                    ls->forceBlur = forceBlur;
            }
        }
    }
}

void CConfigManager::handleBlurLS(const std::string& command, const std::string& value) {
    if (value.starts_with("remove,")) {
        const auto TOREMOVE = removeBeginEndSpacesTabs(value.substr(7));
        if (std::erase_if(m_dBlurLSNamespaces, [&](const auto& other) { return other == TOREMOVE; }))
            updateBlurredLS(TOREMOVE, false);
        return;
    }

    m_dBlurLSNamespaces.emplace_back(value);
    updateBlurredLS(value, true);
}

void CConfigManager::handleWorkspaceRules(const std::string& command, const std::string& value) {
    // This can either be the monitor or the workspace identifier
    const auto     FIRST_DELIM = value.find_first_of(',');

    std::string    name        = "";
    auto           first_ident = removeBeginEndSpacesTabs(value.substr(0, FIRST_DELIM));
    int            id          = getWorkspaceIDFromString(first_ident, name);

    auto           rules = value.substr(FIRST_DELIM + 1);
    SWorkspaceRule wsRule;
    wsRule.workspaceString = first_ident;
    if (id == WORKSPACE_INVALID) {
        // it could be the monitor. If so, second value MUST be
        // the workspace.
        const auto WORKSPACE_DELIM = value.find_first_of(',', FIRST_DELIM + 1);
        auto       wsIdent         = removeBeginEndSpacesTabs(value.substr(FIRST_DELIM + 1, (WORKSPACE_DELIM - FIRST_DELIM - 1)));
        id                         = getWorkspaceIDFromString(wsIdent, name);
        if (id == WORKSPACE_INVALID) {
            Debug::log(ERR, "Invalid workspace identifier found: {}", wsIdent);
            parseError = "Invalid workspace identifier found: " + wsIdent;
            return;
        }
        wsRule.monitor         = first_ident;
        wsRule.workspaceString = wsIdent;
        wsRule.isDefault       = true; // backwards compat
        rules                  = value.substr(WORKSPACE_DELIM + 1);
    }

    const static std::string ruleOnCreatedEmtpy    = "on-created-empty:";
    const static int         ruleOnCreatedEmtpyLen = ruleOnCreatedEmtpy.length();

    auto                     assignRule = [&](std::string rule) {
        size_t delim = std::string::npos;
        if ((delim = rule.find("gapsin:")) != std::string::npos)
            wsRule.gapsIn = std::stoi(rule.substr(delim + 7));
        else if ((delim = rule.find("gapsout:")) != std::string::npos)
            wsRule.gapsOut = std::stoi(rule.substr(delim + 8));
        else if ((delim = rule.find("bordersize:")) != std::string::npos)
            wsRule.borderSize = std::stoi(rule.substr(delim + 11));
        else if ((delim = rule.find("border:")) != std::string::npos)
            wsRule.border = configStringToInt(rule.substr(delim + 7));
        else if ((delim = rule.find("shadow:")) != std::string::npos)
            wsRule.shadow = configStringToInt(rule.substr(delim + 7));
        else if ((delim = rule.find("rounding:")) != std::string::npos)
            wsRule.rounding = configStringToInt(rule.substr(delim + 9));
        else if ((delim = rule.find("decorate:")) != std::string::npos)
            wsRule.decorate = configStringToInt(rule.substr(delim + 9));
        else if ((delim = rule.find("monitor:")) != std::string::npos)
            wsRule.monitor = rule.substr(delim + 8);
        else if ((delim = rule.find("default:")) != std::string::npos)
            wsRule.isDefault = configStringToInt(rule.substr(delim + 8));
        else if ((delim = rule.find("persistent:")) != std::string::npos)
            wsRule.isPersistent = configStringToInt(rule.substr(delim + 11));
        else if ((delim = rule.find(ruleOnCreatedEmtpy)) != std::string::npos)
            wsRule.onCreatedEmptyRunCmd = cleanCmdForWorkspace(name, rule.substr(delim + ruleOnCreatedEmtpyLen));
        else if ((delim = rule.find("layoutopt:")) != std::string::npos) {
            std::string opt = rule.substr(delim + 10);
            if (!opt.contains(":")) {
                // invalid
                Debug::log(ERR, "Invalid workspace rule found: {}", rule);
                parseError = "Invalid workspace rule found: " + rule;
                return;
            }

            std::string val = opt.substr(opt.find(":") + 1);
            opt             = opt.substr(0, opt.find(":"));

            wsRule.layoutopts[opt] = val;
        }
    };

    size_t      pos = 0;
    std::string rule;
    while ((pos = rules.find(',')) != std::string::npos) {
        rule = rules.substr(0, pos);
        assignRule(rule);
        rules.erase(0, pos + 1);
    }
    assignRule(rules); // match remaining rule

    wsRule.workspaceId   = id;
    wsRule.workspaceName = name;

    const auto IT = std::find_if(m_dWorkspaceRules.begin(), m_dWorkspaceRules.end(), [&](const auto& other) { return other.workspaceString == wsRule.workspaceString; });

    if (IT == m_dWorkspaceRules.end())
        m_dWorkspaceRules.emplace_back(wsRule);
    else
        *IT = wsRule;
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
    std::unique_ptr<glob_t, void (*)(glob_t*)> glob_buf{new glob_t, [](glob_t* g) { globfree(g); }};
    memset(glob_buf.get(), 0, sizeof(glob_t));

    if (auto r = glob(absolutePath(rawpath, configCurrentPath).c_str(), GLOB_TILDE, nullptr, glob_buf.get()); r != 0) {
        parseError = std::format("source= globbing error: {}", r == GLOB_NOMATCH ? "found no match" : GLOB_ABORTED ? "read error" : "out of memory");
        Debug::log(ERR, "{}", parseError);
        return;
    }

    for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
        auto value = absolutePath(glob_buf->gl_pathv[i], configCurrentPath);

        if (!std::filesystem::is_regular_file(value)) {
            if (std::filesystem::exists(value)) {
                Debug::log(WARN, "source= skipping non-file {}", value);
                continue;
            }

            Debug::log(ERR, "source= file doesnt exist");
            parseError = "source file " + value + " doesn't exist!";
            return;
        }
        configPaths.push_back(value);

        struct stat fileStat;
        int         err = stat(value.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at {}, error {}: {}", value, err, strerror(err));
            return;
        }

        configModifyTimes[value] = fileStat.st_mtime;

        std::ifstream ifs;
        ifs.open(value);

        std::string line    = "";
        int         linenum = 1;
        if (ifs.is_open()) {
            auto configCurrentPathBackup = configCurrentPath;

            while (std::getline(ifs, line)) {
                // Read line by line.
                try {
                    configCurrentPath = value;
                    parseLine(line);
                } catch (...) {
                    Debug::log(ERR, "Error reading line from config. Line:");
                    Debug::log(NONE, "{}", line.c_str());

                    parseError += "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): Line parsing error.";
                }

                if (parseError != "" && !parseError.starts_with("Config error at line")) {
                    parseError = "Config error at line " + std::to_string(linenum) + " (" + configCurrentPath + "): " + parseError;
                }

                ++linenum;
            }

            ifs.close();

            configCurrentPath = configCurrentPathBackup;
        }
    }
}

void CConfigManager::handleBindWS(const std::string& command, const std::string& value) {
    parseError = "bindws has been deprecated in favor of workspace rules, see the wiki -> workspace rules";
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
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user import-environment " + ARGS[0] +
            " && hash dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd " +
            ARGS[0];
        handleRawExec("", CMD);
    }
}

void CConfigManager::handlePlugin(const std::string& command, const std::string& path) {
    if (std::find(m_vDeclaredPlugins.begin(), m_vDeclaredPlugins.end(), path) != m_vDeclaredPlugins.end()) {
        parseError = "plugin '" + path + "' declared twice";
        return;
    }

    m_vDeclaredPlugins.push_back(path);
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
    else if (COMMAND.starts_with("bind"))
        handleBind(COMMAND, VALUE);
    else if (COMMAND == "unbind")
        handleUnbind(COMMAND, VALUE);
    else if (COMMAND == "workspace")
        handleWorkspaceRules(COMMAND, VALUE);
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
    else if (COMMAND == "plugin")
        handlePlugin(COMMAND, VALUE);
    else if (COMMAND.starts_with("env"))
        handleEnv(COMMAND, VALUE);
    else {
        // try config
        const auto IT = std::find_if(pluginKeywords.begin(), pluginKeywords.end(), [&](const auto& other) { return other.name == COMMAND; });

        if (IT != pluginKeywords.end()) {
            IT->fn(COMMAND, VALUE);
        } else {
            configSetValueSafe(currentCategory + (currentCategory == "" ? "" : ":") + COMMAND, VALUE);
            needsLayoutRecalc = 2;
        }
    }

    if (dynamic) {
        std::string retval = parseError;
        parseError         = "";

        // invalidate layouts if they changed
        if (needsLayoutRecalc) {
            if (needsLayoutRecalc == 1 || COMMAND.contains("gaps_") || COMMAND.starts_with("dwindle:") || COMMAND.starts_with("master:")) {
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

    int  times = 0;

    while (dollarPlace != std::string::npos) {
        times++;

        const auto STRAFTERDOLLAR = line.substr(dollarPlace + 1);
        bool       found          = false;
        for (auto& [var, value] : configDynamicVars) {
            if (STRAFTERDOLLAR.starts_with(var)) {
                line.replace(dollarPlace, var.length() + 1, value);
                found = true;
                break;
            }
        }

        if (!found) {
            // maybe env?
            for (auto& [var, value] : environmentVariables) {
                if (STRAFTERDOLLAR.starts_with(var)) {
                    line.replace(dollarPlace, var.length() + 1, value);
                    break;
                }
            }
        }

        dollarPlace = line.find_first_of('$', dollarPlace + 1);

        if (times > 256 /* arbitrary limit */) {
            line       = "";
            parseError = "Maximum variable recursion limit hit. Evaluating the line led to too many variable substitutions.";
            Debug::log(ERR, "Variable recursion limit hit in configmanager");
            break;
        }
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

        if (LASTSEP == std::string::npos || currentCategory.starts_with("device"))
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
    EMIT_HOOK_EVENT("preConfigReload", nullptr);

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
    m_dWorkspaceRules.clear();
    setDefaultAnimationVars(); // reset anims
    m_vDeclaredPlugins.clear();
    m_dLayerRules.clear();
    m_vFailedPluginConfigValues.clear();

    // paths
    configPaths.clear();
    std::string mainConfigPath = getMainConfigPath();
    Debug::log(LOG, "Using config: {}", mainConfigPath);
    configPaths.push_back(mainConfigPath);

    if (g_pCompositor->explicitConfigPath.empty() && !std::filesystem::exists(mainConfigPath)) {
        std::string configPath = std::filesystem::path(mainConfigPath).parent_path();

        if (!std::filesystem::is_directory(configPath)) {
            Debug::log(WARN, "Creating config home directory");
            try {
                std::filesystem::create_directories(configPath);
            } catch (...) {
                parseError = "Broken config file! (Could not create config directory)";
                return;
            }
        }

        Debug::log(WARN, "No config file found; attempting to generate.");
        std::ofstream ofs;
        ofs.open(mainConfigPath, std::ios::trunc);
        ofs << AUTOCONFIG;
        ofs.close();
    }

    std::ifstream ifs;
    ifs.open(mainConfigPath);

    if (!ifs.good()) {
        ifs.close();

        if (!g_pCompositor->explicitConfigPath.empty()) {
            Debug::log(WARN, "Config reading error!");
            parseError = "Broken config file! (Could not read)";
            return;
        }

        Debug::log(WARN, "Config reading error. Attempting to generate, backing up old one if exists");

        if (std::filesystem::exists(mainConfigPath))
            std::filesystem::rename(mainConfigPath, mainConfigPath + ".backup");

        // Create default config
        std::ofstream ofs;
        ofs.open(mainConfigPath, std::ios::trunc);
        ofs << AUTOCONFIG;
        ofs.close();

        // Try to re-open
        ifs.open(mainConfigPath);
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
                configCurrentPath = mainConfigPath;
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "{}", line);

                parseError += "Config error at line " + std::to_string(linenum) + " (" + mainConfigPath + "): Line parsing error.";
            }

            if (parseError != "" && !parseError.starts_with("Config error at line")) {
                parseError = "Config error at line " + std::to_string(linenum) + " (" + mainConfigPath + "): " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        w->uncacheWindowDecos();
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
    if (parseError != "" && !configValues["debug:suppress_errors"].intValue)
        g_pHyprError->queueCreate(parseError + "\nHyprland may not work correctly.", CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    else if (configValues["autogenerated"].intValue == 1)
        g_pHyprError->queueCreate("Warning: You're using an autogenerated config! (config file: " + mainConfigPath + " )\nSUPER+Q -> kitty\nSUPER+M -> exit Hyprland",
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

    if (!isFirstLaunch && !g_pCompositor->m_bUnsafeState)
        refreshGroupBarGradients();

    // Updates dynamic window and workspace rules
    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        w->updateDynamicRules();
        w->updateSpecialRenderData();
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
    if (Debug::disableStdout && isFirstLaunch)
        Debug::log(LOG, "Disabling stdout logs! Check the log for further logs.");

    for (auto& m : g_pCompositor->m_vMonitors) {
        // mark blur dirty
        g_pHyprOpenGL->markBlurDirtyForMonitor(m.get());

        g_pCompositor->scheduleFrameForMonitor(m.get());

        // Force the compositor to fully re-render all monitors
        m->forceFullFrames = 2;
    }

    // Reset no monitor reload
    m_bNoMonitorReload = false;

    // update plugins
    handlePluginLoads();

    EMIT_HOOK_EVENT("configReloaded", nullptr);
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::tick() {
    std::string CONFIGPATH = getMainConfigPath();
    if (!std::filesystem::exists(CONFIGPATH)) {
        Debug::log(ERR, "Config doesn't exist??");
        return;
    }

    bool parse = false;

    for (auto& cf : configPaths) {
        struct stat fileStat;
        int         err = stat(cf.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at {}, error {}: {}", cf, err, strerror(err));
            continue;
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

SConfigValue CConfigManager::getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback) {
    std::lock_guard<std::mutex> lg(configmtx);

    const auto                  it = deviceConfigs.find(dev);

    if (it == deviceConfigs.end()) {
        if (fallback.empty()) {
            Debug::log(ERR, "getConfigValueSafeDevice: No device config for {} found???", dev);
            return SConfigValue();
        }
        return configValues[fallback];
    }

    const SConfigValue DEVICECONFIG = it->second[val];

    if (!DEVICECONFIG.set && !fallback.empty()) {
        return configValues[fallback];
    }

    return DEVICECONFIG;
}

int CConfigManager::getInt(const std::string& v) {
    return getConfigValueSafe(v).intValue;
}

float CConfigManager::getFloat(const std::string& v) {
    return getConfigValueSafe(v).floatValue;
}

Vector2D CConfigManager::getVec(const std::string& v) {
    return getConfigValueSafe(v).vecValue;
}

std::string CConfigManager::getString(const std::string& v) {
    auto VAL = getConfigValueSafe(v).strValue;

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& v, const std::string& fallback) {
    return getConfigValueSafeDevice(dev, v, fallback).intValue;
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& v, const std::string& fallback) {
    return getConfigValueSafeDevice(dev, v, fallback).floatValue;
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& v, const std::string& fallback) {
    return getConfigValueSafeDevice(dev, v, fallback).vecValue;
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& v, const std::string& fallback) {
    auto VAL = getConfigValueSafeDevice(dev, v, fallback).strValue;

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

void CConfigManager::setVec(const std::string& v, Vector2D val) {
    configValues[v].vecValue = val;
}

void CConfigManager::setString(const std::string& v, const std::string& val) {
    configValues[v].strValue = val;
}

SMonitorRule CConfigManager::getMonitorRuleFor(const std::string& name, const std::string& displayName) {
    SMonitorRule* found = nullptr;

    for (auto& r : m_dMonitorRules) {
        if (r.name == name ||
            (r.name.starts_with("desc:") &&
             (r.name.substr(5) == displayName || r.name.substr(5) == removeBeginEndSpacesTabs(displayName.substr(0, displayName.find_first_of('(')))))) {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    Debug::log(WARN, "No rule found for {}, trying to use the first.", name);

    for (auto& r : m_dMonitorRules) {
        if (r.name == "") {
            found = &r;
            break;
        }
    }

    if (found)
        return *found;

    Debug::log(WARN, "No rules configured. Using the default hardcoded one.");

    return SMonitorRule{.name = "", .resolution = Vector2D(0, 0), .offset = Vector2D(-INT32_MAX, -INT32_MAX), .scale = -1}; // 0, 0 is preferred and -1, -1 is auto
}

SWorkspaceRule CConfigManager::getWorkspaceRuleFor(CWorkspace* pWorkspace) {
    const auto WORKSPACEIDSTR = std::to_string(pWorkspace->m_iID);
    const auto IT             = std::find_if(m_dWorkspaceRules.begin(), m_dWorkspaceRules.end(), [&](const auto& other) {
        return other.workspaceName == pWorkspace->m_szName /* name matches */
            || (pWorkspace->m_bIsSpecialWorkspace && other.workspaceName.starts_with("special:") &&
                other.workspaceName.substr(8) == pWorkspace->m_szName)           /* special and special:name */
            || (pWorkspace->m_iID > 0 && WORKSPACEIDSTR == other.workspaceName); /* id matches and workspace is numerical */
    });
    if (IT == m_dWorkspaceRules.end())
        return SWorkspaceRule{};
    return *IT;
}

std::vector<SWindowRule> CConfigManager::getMatchingRules(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return std::vector<SWindowRule>();

    std::vector<SWindowRule> returns;

    std::string              title      = g_pXWaylandManager->getTitle(pWindow);
    std::string              appidclass = g_pXWaylandManager->getAppIDClass(pWindow);

    Debug::log(LOG, "Searching for matching rules for {} (title: {})", appidclass, title);

    // since some rules will be applied later, we need to store some flags
    bool hasFloating   = pWindow->m_bIsFloating;
    bool hasFullscreen = pWindow->m_bIsFullscreen;

    for (auto& rule : m_dWindowRules) {
        // check if we have a matching rule
        if (!rule.v2) {
            try {
                if (rule.szValue.starts_with("title:")) {
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
                Debug::log(ERR, "Regex error at {}", rule.szValue);
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

                if (rule.szInitialTitle != "") {
                    std::regex RULECHECK(rule.szInitialTitle);

                    if (!std::regex_search(pWindow->m_szInitialTitle, RULECHECK))
                        continue;
                }

                if (rule.szInitialClass != "") {
                    std::regex RULECHECK(rule.szInitialClass);

                    if (!std::regex_search(pWindow->m_szInitialClass, RULECHECK))
                        continue;
                }

                if (rule.bX11 != -1) {
                    if (pWindow->m_bIsX11 != rule.bX11)
                        continue;
                }

                if (rule.bFloating != -1) {
                    if (hasFloating != rule.bFloating)
                        continue;
                }

                if (rule.bFullscreen != -1) {
                    if (hasFullscreen != rule.bFullscreen)
                        continue;
                }

                if (rule.bPinned != -1) {
                    if (pWindow->m_bPinned != rule.bPinned)
                        continue;
                }

                if (rule.bFocus != -1) {
                    if (rule.bFocus != (g_pCompositor->m_pLastWindow == pWindow))
                        continue;
                }

                if (rule.iOnWorkspace != -1) {
                    if (rule.iOnWorkspace != g_pCompositor->getWindowsOnWorkspace(pWindow->m_iWorkspaceID))
                        continue;
                }

                if (!rule.szWorkspace.empty()) {
                    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

                    if (!PWORKSPACE)
                        continue;

                    if (rule.szWorkspace.starts_with("name:")) {
                        if (PWORKSPACE->m_szName != rule.szWorkspace.substr(5))
                            continue;
                    } else {
                        // number
                        if (!isNumber(rule.szWorkspace))
                            throw std::runtime_error("szWorkspace not name: or number");

                        const int64_t ID = std::stoll(rule.szWorkspace);

                        if (PWORKSPACE->m_iID != ID)
                            continue;
                    }
                }
            } catch (std::exception& e) {
                Debug::log(ERR, "Regex error at {} ({})", rule.szValue, e.what());
                continue;
            }
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule {} -> {} matched {}", rule.szRule, rule.szValue, pWindow);

        returns.push_back(rule);

        if (rule.szRule == "float")
            hasFloating = true;
        else if (rule.szRule == "fullscreen")
            hasFullscreen = true;
    }

    std::vector<uint64_t> PIDs = {(uint64_t)pWindow->getPID()};
    while (getPPIDof(PIDs.back()) > 10)
        PIDs.push_back(getPPIDof(PIDs.back()));

    bool anyExecFound = false;

    for (auto& er : execRequestedRules) {
        if (std::ranges::any_of(PIDs, [&](const auto& pid) { return pid == er.iPid; })) {
            returns.push_back({er.szRule, "execRule"});
            anyExecFound = true;
        }
    }

    if (anyExecFound) // remove exec rules to unclog searches in the future, why have the garbage here.
        execRequestedRules.erase(std::remove_if(execRequestedRules.begin(), execRequestedRules.end(),
                                                [&](const SExecRequestedRule& other) { return std::ranges::any_of(PIDs, [&](const auto& pid) { return pid == other.iPid; }); }));

    return returns;
}

std::vector<SLayerRule> CConfigManager::getMatchingRules(SLayerSurface* pLS) {
    std::vector<SLayerRule> returns;

    if (!pLS->layerSurface || pLS->fadingOut)
        return returns;

    for (auto& lr : m_dLayerRules) {
        if (lr.targetNamespace.starts_with("address:0x")) {
            if (std::format("address:0x{:x}", (uintptr_t)pLS) != lr.targetNamespace)
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
        handleRawExec("",
#ifdef USES_SYSTEMD
                      "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME && hash "
                      "dbus-update-activation-environment 2>/dev/null && "
#endif
                      "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME");

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

    // check for user's possible errors with their setup and notify them if needed
    g_pCompositor->performUserChecks();
}

void CConfigManager::performMonitorReload() {

    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (!m->output || m->isUnsafeFallback)
            continue;

        auto rule = getMonitorRuleFor(m->szName, m->szDescription);

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

    m_bWantsMonitorReload = false;

    EMIT_HOOK_EVENT("monitorLayoutChanged", nullptr);
}

SConfigValue* CConfigManager::getConfigValuePtr(const std::string& val) {
    return &configValues[val];
}

SConfigValue* CConfigManager::getConfigValuePtrSafe(const std::string& val) {
    if (val.starts_with("device:")) {
        const auto DEVICE    = val.substr(7, val.find_last_of(':') - 7);
        const auto CONFIGVAR = val.substr(val.find_last_of(':') + 1);

        const auto DEVICECONF = deviceConfigs.find(DEVICE);
        if (DEVICECONF == deviceConfigs.end())
            return nullptr;

        const auto IT = DEVICECONF->second.find(CONFIGVAR);

        if (IT == DEVICECONF->second.end())
            return nullptr;

        return &IT->second;
    } else if (val.starts_with("plugin:")) {
        for (auto& [pl, pMap] : pluginConfigs) {
            const auto IT = pMap->find(val);

            if (IT != pMap->end())
                return &IT->second;
        }

        return nullptr;
    }

    const auto IT = configValues.find(val);

    if (IT == configValues.end())
        return nullptr;

    return &(IT->second);
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    auto copy = dev;
    std::replace(copy.begin(), copy.end(), ' ', '-');

    return deviceConfigs.contains(copy);
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
        if (!rm->output || rm->isUnsafeFallback)
            continue;

        auto rule = getMonitorRuleFor(rm->szName, rm->szDescription);

        if (rule.disabled == rm->m_bEnabled)
            g_pHyprRenderer->applyMonitorRule(rm.get(), &rule);
    }
}

void CConfigManager::ensureVRR(CMonitor* pMonitor) {
    static auto* const PVRR = &getConfigValuePtr("misc:vrr")->intValue;

    static auto        ensureVRRForDisplay = [&](CMonitor* m) -> void {
        if (!m->output || m->createdByUser)
            return;

        const auto USEVRR = m->activeMonitorRule.vrr.has_value() ? m->activeMonitorRule.vrr.value() : *PVRR;

        if (USEVRR == 0) {
            if (m->vrrActive) {
                wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 0);

                if (!m->state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> false", m->output->name);
            }
            m->vrrActive = false;
            return;
        } else if (USEVRR == 1) {
            if (!m->vrrActive) {
                wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 1);

                if (!m->state.test()) {
                    Debug::log(LOG, "Pending output {} does not accept VRR.", m->output->name);
                    wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 0);
                }

                if (!m->state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> true", m->output->name);
            }
            m->vrrActive = true;
            return;
        } else if (USEVRR == 2) {
            /* fullscreen */
            m->vrrActive = true;

            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m->activeWorkspace);

            if (!PWORKSPACE)
                return; // ???

            const auto WORKSPACEFULL = PWORKSPACE->m_bHasFullscreenWindow && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL;

            if (WORKSPACEFULL && m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED) {
                wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 1);

                if (!m->state.test()) {
                    Debug::log(LOG, "Pending output {} does not accept VRR.", m->output->name);
                    wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 0);
                }

                if (!m->state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> true", m->output->name);

            } else if (!WORKSPACEFULL && m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED) {
                wlr_output_state_set_adaptive_sync_enabled(m->state.wlr(), 0);

                if (!m->state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> false", m->output->name);
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
    auto monitor = getBoundMonitorStringForWS(wsname);
    if (monitor.substr(0, 5) == "desc:")
        return g_pCompositor->getMonitorFromDesc(monitor.substr(5));
    else
        return g_pCompositor->getMonitorFromName(monitor);
}

std::string CConfigManager::getBoundMonitorStringForWS(const std::string& wsname) {
    for (auto& wr : m_dWorkspaceRules) {
        const auto WSNAME = wr.workspaceName.starts_with("name:") ? wr.workspaceName.substr(5) : wr.workspaceName;

        if (WSNAME == wsname) {
            return wr.monitor;
        }
    }

    return "";
}

const std::deque<SWorkspaceRule>& CConfigManager::getAllWorkspaceRules() {
    return m_dWorkspaceRules;
}

void CConfigManager::addExecRule(const SExecRequestedRule& rule) {
    execRequestedRules.push_back(rule);
}

void CConfigManager::handlePluginLoads() {
    if (g_pPluginSystem == nullptr)
        return;

    bool pluginsChanged = false;
    auto failedPlugins  = g_pPluginSystem->updateConfigPlugins(m_vDeclaredPlugins, pluginsChanged);

    if (!failedPlugins.empty()) {
        std::stringstream error;
        error << "Failed to load the following plugins:";

        for (auto path : failedPlugins) {
            error << "\n" << path;
        }

        g_pHyprError->queueCreate(error.str(), CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    }

    if (pluginsChanged) {
        g_pHyprError->destroy();
        m_bForceReload = true;
        tick();
    }
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

    if (const auto IT = std::find_if(m_vFailedPluginConfigValues.begin(), m_vFailedPluginConfigValues.end(), [&](const auto& other) { return other.first == name; });
        IT != m_vFailedPluginConfigValues.end()) {
        configSetValueSafe(IT->first, IT->second);
    }
}

void CConfigManager::addPluginKeyword(HANDLE handle, const std::string& name, std::function<void(const std::string&, const std::string&)> fn) {
    pluginKeywords.emplace_back(SPluginKeyword{handle, name, fn});
}

void CConfigManager::removePluginConfig(HANDLE handle) {
    std::erase_if(pluginConfigs, [&](const auto& other) { return other.first == handle; });
    std::erase_if(pluginKeywords, [&](const auto& other) { return other.handle == handle; });
}

std::string CConfigManager::getDefaultWorkspaceFor(const std::string& name) {
    for (auto other = m_dWorkspaceRules.begin(); other != m_dWorkspaceRules.end(); ++other) {
        if (other->isDefault) {
            if (other->monitor == name)
                return other->workspaceString;
            if (other->monitor.substr(0, 5) == "desc:") {
                auto monitor = g_pCompositor->getMonitorFromDesc(other->monitor.substr(5));
                if (monitor && monitor->szName == name)
                    return other->workspaceString;
            }
        }
    }
    return "";
}
