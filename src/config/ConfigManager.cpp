#include "ConfigManager.hpp"
#include "../managers/KeybindManager.hpp"

#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "config/ConfigDataValues.hpp"
#include "helpers/VarList.hpp"
#include "../protocols/LayerShell.hpp"

#include <cstdint>
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
#include <ranges>
#include <unordered_set>
#include <xkbcommon/xkbcommon.h>

extern "C" char**             environ;

static Hyprlang::CParseResult configHandleGradientSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new CGradientValueData();

    const auto DATA = reinterpret_cast<CGradientValueData*>(*data);

    CVarList   varlist(V, 0, ' ');
    DATA->m_vColors.clear();

    std::string parseError = "";

    for (auto& var : varlist) {
        if (var.find("deg") != std::string::npos) {
            // last arg
            try {
                DATA->m_fAngle = std::stoi(var.substr(0, var.find("deg"))) * (PI / 180.0); // radians
            } catch (...) {
                Debug::log(WARN, "Error parsing gradient {}", V);
                parseError = "Error parsing gradient " + V;
            }

            break;
        }

        if (DATA->m_vColors.size() >= 10) {
            Debug::log(WARN, "Error parsing gradient {}: max colors is 10.", V);
            parseError = "Error parsing gradient " + V + ": max colors is 10.";
            break;
        }

        try {
            DATA->m_vColors.push_back(CColor(configStringToInt(var)));
        } catch (std::exception& e) {
            Debug::log(WARN, "Error parsing gradient {}", V);
            parseError = "Error parsing gradient " + V + ": " + e.what();
        }
    }

    if (DATA->m_vColors.size() == 0) {
        Debug::log(WARN, "Error parsing gradient {}", V);
        parseError = "Error parsing gradient " + V + ": No colors?";

        DATA->m_vColors.push_back(0); // transparent
    }

    Hyprlang::CParseResult result;
    if (!parseError.empty())
        result.setError(parseError.c_str());

    return result;
}

static void configHandleGradientDestroy(void** data) {
    if (*data)
        delete reinterpret_cast<CGradientValueData*>(*data);
}

static Hyprlang::CParseResult configHandleGapSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new CCssGapData();

    const auto             DATA = reinterpret_cast<CCssGapData*>(*data);
    CVarList               varlist(V);
    Hyprlang::CParseResult result;

    try {
        DATA->parseGapData(varlist);
    } catch (...) {
        std::string parseError = "Error parsing gaps " + V;
        result.setError(parseError.c_str());
    }

    return result;
}

static void configHandleGapDestroy(void** data) {
    if (*data)
        delete reinterpret_cast<CCssGapData*>(*data);
}

static Hyprlang::CParseResult handleRawExec(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleRawExec(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleExecOnce(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleExecOnce(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleMonitor(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleMonitor(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleBezier(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleBezier(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleAnimation(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleAnimation(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleBind(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleBind(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleUnbind(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleUnbind(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWindowRule(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleWindowRule(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleLayerRule(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleLayerRule(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWindowRuleV2(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleWindowRuleV2(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleBlurLS(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleBlurLS(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWorkspaceRules(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleWorkspaceRules(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleSubmap(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleSubmap(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleSource(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleSource(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleEnv(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleEnv(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handlePlugin(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handlePlugin(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

CConfigManager::CConfigManager() {
    const auto ERR = verifyConfigExists();

    configPaths.emplace_back(getMainConfigPath());
    m_pConfig = std::make_unique<Hyprlang::CConfig>(configPaths.begin()->c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true});

    m_pConfig->addConfigValue("general:sensitivity", {1.0f});
    m_pConfig->addConfigValue("general:apply_sens_to_raw", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:border_size", Hyprlang::INT{1});
    m_pConfig->addConfigValue("general:no_border_on_floating", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:border_part_of_window", Hyprlang::INT{1});
    m_pConfig->addConfigValue("general:gaps_in", Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, "5"});
    m_pConfig->addConfigValue("general:gaps_out", Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, "20"});
    m_pConfig->addConfigValue("general:gaps_workspaces", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:no_focus_fallback", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:resize_on_border", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:extend_border_grab_area", Hyprlang::INT{15});
    m_pConfig->addConfigValue("general:hover_icon_on_border", Hyprlang::INT{1});
    m_pConfig->addConfigValue("general:layout", {"dwindle"});
    m_pConfig->addConfigValue("general:allow_tearing", Hyprlang::INT{0});
    m_pConfig->addConfigValue("general:resize_corner", Hyprlang::INT{0});

    m_pConfig->addConfigValue("misc:disable_hyprland_logo", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:disable_splash_rendering", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:col.splash", Hyprlang::INT{0x55ffffff});
    m_pConfig->addConfigValue("misc:splash_font_family", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("misc:font_family", {"Sans"});
    m_pConfig->addConfigValue("misc:force_default_wallpaper", Hyprlang::INT{-1});
    m_pConfig->addConfigValue("misc:vfr", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:vrr", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:mouse_move_enables_dpms", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:key_press_enables_dpms", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:always_follow_on_dnd", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:layers_hog_keyboard_focus", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:animate_manual_resizes", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:animate_mouse_windowdragging", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:disable_autoreload", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:enable_swallow", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:swallow_regex", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("misc:swallow_exception_regex", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("misc:focus_on_activate", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:no_direct_scanout", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:mouse_move_focuses_monitor", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:render_ahead_of_time", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:render_ahead_safezone", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:allow_session_lock_restore", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:close_special_on_empty", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:background_color", Hyprlang::INT{0xff111111});
    m_pConfig->addConfigValue("misc:new_window_takes_over_fullscreen", Hyprlang::INT{0});
    m_pConfig->addConfigValue("misc:initial_workspace_tracking", Hyprlang::INT{1});
    m_pConfig->addConfigValue("misc:middle_click_paste", Hyprlang::INT{1});

    m_pConfig->addConfigValue("group:insert_after_current", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:focus_removed_window", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:groupbar:enabled", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:groupbar:font_family", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("group:groupbar:font_size", Hyprlang::INT{8});
    m_pConfig->addConfigValue("group:groupbar:gradients", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:groupbar:height", Hyprlang::INT{14});
    m_pConfig->addConfigValue("group:groupbar:priority", Hyprlang::INT{3});
    m_pConfig->addConfigValue("group:groupbar:render_titles", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:groupbar:scrolling", Hyprlang::INT{1});
    m_pConfig->addConfigValue("group:groupbar:text_color", Hyprlang::INT{0xffffffff});
    m_pConfig->addConfigValue("group:groupbar:stacked", Hyprlang::INT{0});

    m_pConfig->addConfigValue("debug:int", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:log_damage", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:overlay", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:damage_blink", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:disable_logs", Hyprlang::INT{1});
    m_pConfig->addConfigValue("debug:disable_time", Hyprlang::INT{1});
    m_pConfig->addConfigValue("debug:enable_stdout_logs", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:damage_tracking", {(Hyprlang::INT)DAMAGE_TRACKING_FULL});
    m_pConfig->addConfigValue("debug:manual_crash", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:suppress_errors", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:error_limit", Hyprlang::INT{5});
    m_pConfig->addConfigValue("debug:error_position", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:watchdog_timeout", Hyprlang::INT{5});
    m_pConfig->addConfigValue("debug:disable_scale_checks", Hyprlang::INT{0});
    m_pConfig->addConfigValue("debug:colored_stdout_logs", Hyprlang::INT{1});

    m_pConfig->addConfigValue("decoration:rounding", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:blur:enabled", Hyprlang::INT{1});
    m_pConfig->addConfigValue("decoration:blur:size", Hyprlang::INT{8});
    m_pConfig->addConfigValue("decoration:blur:passes", Hyprlang::INT{1});
    m_pConfig->addConfigValue("decoration:blur:ignore_opacity", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:blur:new_optimizations", Hyprlang::INT{1});
    m_pConfig->addConfigValue("decoration:blur:xray", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:blur:contrast", {0.8916F});
    m_pConfig->addConfigValue("decoration:blur:brightness", {1.0F});
    m_pConfig->addConfigValue("decoration:blur:vibrancy", {0.1696F});
    m_pConfig->addConfigValue("decoration:blur:vibrancy_darkness", {0.0F});
    m_pConfig->addConfigValue("decoration:blur:noise", {0.0117F});
    m_pConfig->addConfigValue("decoration:blur:special", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:blur:popups", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:blur:popups_ignorealpha", {0.2F});
    m_pConfig->addConfigValue("decoration:active_opacity", {1.F});
    m_pConfig->addConfigValue("decoration:inactive_opacity", {1.F});
    m_pConfig->addConfigValue("decoration:fullscreen_opacity", {1.F});
    m_pConfig->addConfigValue("decoration:no_blur_on_oversized", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:drop_shadow", Hyprlang::INT{1});
    m_pConfig->addConfigValue("decoration:shadow_range", Hyprlang::INT{4});
    m_pConfig->addConfigValue("decoration:shadow_render_power", Hyprlang::INT{3});
    m_pConfig->addConfigValue("decoration:shadow_ignore_window", Hyprlang::INT{1});
    m_pConfig->addConfigValue("decoration:shadow_offset", Hyprlang::VEC2{0, 0});
    m_pConfig->addConfigValue("decoration:shadow_scale", {1.f});
    m_pConfig->addConfigValue("decoration:col.shadow", Hyprlang::INT{0xee1a1a1a});
    m_pConfig->addConfigValue("decoration:col.shadow_inactive", {(Hyprlang::INT)INT_MAX});
    m_pConfig->addConfigValue("decoration:dim_inactive", Hyprlang::INT{0});
    m_pConfig->addConfigValue("decoration:dim_strength", {0.5f});
    m_pConfig->addConfigValue("decoration:dim_special", {0.2f});
    m_pConfig->addConfigValue("decoration:dim_around", {0.4f});
    m_pConfig->addConfigValue("decoration:screen_shader", {STRVAL_EMPTY});

    m_pConfig->addConfigValue("dwindle:pseudotile", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:force_split", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:permanent_direction_override", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:preserve_split", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:special_scale_factor", {1.f});
    m_pConfig->addConfigValue("dwindle:split_width_multiplier", {1.0f});
    m_pConfig->addConfigValue("dwindle:no_gaps_when_only", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:use_active_for_splits", Hyprlang::INT{1});
    m_pConfig->addConfigValue("dwindle:default_split_ratio", {1.f});
    m_pConfig->addConfigValue("dwindle:smart_split", Hyprlang::INT{0});
    m_pConfig->addConfigValue("dwindle:smart_resizing", Hyprlang::INT{1});

    m_pConfig->addConfigValue("master:special_scale_factor", {1.f});
    m_pConfig->addConfigValue("master:mfact", {0.55f});
    m_pConfig->addConfigValue("master:new_is_master", Hyprlang::INT{1});
    m_pConfig->addConfigValue("master:always_center_master", Hyprlang::INT{0});
    m_pConfig->addConfigValue("master:new_on_top", Hyprlang::INT{0});
    m_pConfig->addConfigValue("master:no_gaps_when_only", Hyprlang::INT{0});
    m_pConfig->addConfigValue("master:orientation", {"left"});
    m_pConfig->addConfigValue("master:inherit_fullscreen", Hyprlang::INT{1});
    m_pConfig->addConfigValue("master:allow_small_split", Hyprlang::INT{0});
    m_pConfig->addConfigValue("master:smart_resizing", Hyprlang::INT{1});
    m_pConfig->addConfigValue("master:drop_at_cursor", Hyprlang::INT{1});

    m_pConfig->addConfigValue("animations:enabled", Hyprlang::INT{1});
    m_pConfig->addConfigValue("animations:first_launch_animation", Hyprlang::INT{1});

    m_pConfig->addConfigValue("input:follow_mouse", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:mouse_refocus", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:special_fallthrough", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:off_window_axis_events", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:sensitivity", {0.f});
    m_pConfig->addConfigValue("input:accel_profile", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:kb_file", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:kb_layout", {"us"});
    m_pConfig->addConfigValue("input:kb_variant", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:kb_options", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:kb_rules", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:kb_model", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:repeat_rate", Hyprlang::INT{25});
    m_pConfig->addConfigValue("input:repeat_delay", Hyprlang::INT{600});
    m_pConfig->addConfigValue("input:natural_scroll", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:numlock_by_default", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:resolve_binds_by_sym", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:force_no_accel", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:float_switch_override_focus", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:left_handed", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:scroll_method", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:scroll_button", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:scroll_button_lock", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:scroll_factor", {1.f});
    m_pConfig->addConfigValue("input:scroll_points", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:touchpad:natural_scroll", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:touchpad:disable_while_typing", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:touchpad:clickfinger_behavior", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:touchpad:tap_button_map", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:touchpad:middle_button_emulation", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:touchpad:tap-to-click", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:touchpad:tap-and-drag", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:touchpad:drag_lock", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:touchpad:scroll_factor", {1.f});
    m_pConfig->addConfigValue("input:touchdevice:transform", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:touchdevice:output", {"[[Auto]]"});
    m_pConfig->addConfigValue("input:touchdevice:enabled", Hyprlang::INT{1});
    m_pConfig->addConfigValue("input:tablet:transform", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:tablet:output", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("input:tablet:region_position", Hyprlang::VEC2{0, 0});
    m_pConfig->addConfigValue("input:tablet:region_size", Hyprlang::VEC2{0, 0});
    m_pConfig->addConfigValue("input:tablet:relative_input", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:tablet:left_handed", Hyprlang::INT{0});
    m_pConfig->addConfigValue("input:tablet:active_area_position", Hyprlang::VEC2{0, 0});
    m_pConfig->addConfigValue("input:tablet:active_area_size", Hyprlang::VEC2{0, 0});

    m_pConfig->addConfigValue("binds:pass_mouse_when_bound", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:scroll_event_delay", Hyprlang::INT{300});
    m_pConfig->addConfigValue("binds:workspace_back_and_forth", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:allow_workspace_cycles", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:workspace_center_on", Hyprlang::INT{1});
    m_pConfig->addConfigValue("binds:focus_preferred_method", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:ignore_group_lock", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:movefocus_cycles_fullscreen", Hyprlang::INT{1});
    m_pConfig->addConfigValue("binds:disable_keybind_grabbing", Hyprlang::INT{0});
    m_pConfig->addConfigValue("binds:window_direction_monitor_fallback", Hyprlang::INT{1});

    m_pConfig->addConfigValue("gestures:workspace_swipe", Hyprlang::INT{0});
    m_pConfig->addConfigValue("gestures:workspace_swipe_fingers", Hyprlang::INT{3});
    m_pConfig->addConfigValue("gestures:workspace_swipe_distance", Hyprlang::INT{300});
    m_pConfig->addConfigValue("gestures:workspace_swipe_invert", Hyprlang::INT{1});
    m_pConfig->addConfigValue("gestures:workspace_swipe_min_speed_to_force", Hyprlang::INT{30});
    m_pConfig->addConfigValue("gestures:workspace_swipe_cancel_ratio", {0.5f});
    m_pConfig->addConfigValue("gestures:workspace_swipe_create_new", Hyprlang::INT{1});
    m_pConfig->addConfigValue("gestures:workspace_swipe_direction_lock", Hyprlang::INT{1});
    m_pConfig->addConfigValue("gestures:workspace_swipe_direction_lock_threshold", Hyprlang::INT{10});
    m_pConfig->addConfigValue("gestures:workspace_swipe_forever", Hyprlang::INT{0});
    m_pConfig->addConfigValue("gestures:workspace_swipe_use_r", Hyprlang::INT{0});
    m_pConfig->addConfigValue("gestures:workspace_swipe_touch", Hyprlang::INT{0});

    m_pConfig->addConfigValue("xwayland:use_nearest_neighbor", Hyprlang::INT{1});
    m_pConfig->addConfigValue("xwayland:force_zero_scaling", Hyprlang::INT{0});

    m_pConfig->addConfigValue("opengl:nvidia_anti_flicker", Hyprlang::INT{1});
    m_pConfig->addConfigValue("opengl:force_introspection", Hyprlang::INT{2});

    m_pConfig->addConfigValue("cursor:no_hardware_cursors", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:hotspot_padding", Hyprlang::INT{1});
    m_pConfig->addConfigValue("cursor:inactive_timeout", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:no_warps", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:persistent_warps", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:default_monitor", {STRVAL_EMPTY});
    m_pConfig->addConfigValue("cursor:zoom_factor", {1.f});
    m_pConfig->addConfigValue("cursor:zoom_rigid", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:enable_hyprcursor", Hyprlang::INT{1});
    m_pConfig->addConfigValue("cursor:hide_on_key_press", Hyprlang::INT{0});
    m_pConfig->addConfigValue("cursor:hide_on_touch", Hyprlang::INT{1});

    m_pConfig->addConfigValue("autogenerated", Hyprlang::INT{0});

    m_pConfig->addConfigValue("general:col.active_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffffffff"});
    m_pConfig->addConfigValue("general:col.inactive_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xff444444"});
    m_pConfig->addConfigValue("general:col.nogroup_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffffaaff"});
    m_pConfig->addConfigValue("general:col.nogroup_border_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffff00ff"});

    m_pConfig->addConfigValue("group:col.border_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ffff00"});
    m_pConfig->addConfigValue("group:col.border_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66777700"});
    m_pConfig->addConfigValue("group:col.border_locked_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ff5500"});
    m_pConfig->addConfigValue("group:col.border_locked_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66775500"});

    m_pConfig->addConfigValue("group:groupbar:col.active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ffff00"});
    m_pConfig->addConfigValue("group:groupbar:col.inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66777700"});
    m_pConfig->addConfigValue("group:groupbar:col.locked_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ff5500"});
    m_pConfig->addConfigValue("group:groupbar:col.locked_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66775500"});

    // devices
    m_pConfig->addSpecialCategory("device", {"name"});
    m_pConfig->addSpecialConfigValue("device", "sensitivity", {0.F});
    m_pConfig->addSpecialConfigValue("device", "accel_profile", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "kb_file", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "kb_layout", {"us"});
    m_pConfig->addSpecialConfigValue("device", "kb_variant", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "kb_options", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "kb_rules", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "kb_model", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "repeat_rate", Hyprlang::INT{25});
    m_pConfig->addSpecialConfigValue("device", "repeat_delay", Hyprlang::INT{600});
    m_pConfig->addSpecialConfigValue("device", "natural_scroll", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "tap_button_map", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "numlock_by_default", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "resolve_binds_by_sym", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "disable_while_typing", Hyprlang::INT{1});
    m_pConfig->addSpecialConfigValue("device", "clickfinger_behavior", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "middle_button_emulation", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "tap-to-click", Hyprlang::INT{1});
    m_pConfig->addSpecialConfigValue("device", "tap-and-drag", Hyprlang::INT{1});
    m_pConfig->addSpecialConfigValue("device", "drag_lock", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "left_handed", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "scroll_method", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "scroll_button", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "scroll_button_lock", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "scroll_points", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "transform", Hyprlang::INT{0});
    m_pConfig->addSpecialConfigValue("device", "output", {STRVAL_EMPTY});
    m_pConfig->addSpecialConfigValue("device", "enabled", Hyprlang::INT{1});                  // only for mice, touchpads, and touchdevices
    m_pConfig->addSpecialConfigValue("device", "region_position", Hyprlang::VEC2{0, 0});      // only for tablets
    m_pConfig->addSpecialConfigValue("device", "region_size", Hyprlang::VEC2{0, 0});          // only for tablets
    m_pConfig->addSpecialConfigValue("device", "relative_input", Hyprlang::INT{0});           // only for tablets
    m_pConfig->addSpecialConfigValue("device", "active_area_position", Hyprlang::VEC2{0, 0}); // only for tablets
    m_pConfig->addSpecialConfigValue("device", "active_area_size", Hyprlang::VEC2{0, 0});     // only for tablets

    // keywords
    m_pConfig->registerHandler(&::handleRawExec, "exec", {false});
    m_pConfig->registerHandler(&::handleExecOnce, "exec-once", {false});
    m_pConfig->registerHandler(&::handleMonitor, "monitor", {false});
    m_pConfig->registerHandler(&::handleBind, "bind", {true});
    m_pConfig->registerHandler(&::handleUnbind, "unbind", {false});
    m_pConfig->registerHandler(&::handleWorkspaceRules, "workspace", {false});
    m_pConfig->registerHandler(&::handleWindowRule, "windowrule", {false});
    m_pConfig->registerHandler(&::handleLayerRule, "layerrule", {false});
    m_pConfig->registerHandler(&::handleWindowRuleV2, "windowrulev2", {false});
    m_pConfig->registerHandler(&::handleBezier, "bezier", {false});
    m_pConfig->registerHandler(&::handleAnimation, "animation", {false});
    m_pConfig->registerHandler(&::handleSource, "source", {false});
    m_pConfig->registerHandler(&::handleSubmap, "submap", {false});
    m_pConfig->registerHandler(&::handleBlurLS, "blurls", {false});
    m_pConfig->registerHandler(&::handlePlugin, "plugin", {false});
    m_pConfig->registerHandler(&::handleEnv, "env", {true});

    // pluginza
    m_pConfig->addSpecialCategory("plugin", {nullptr, true});

    m_pConfig->commence();

    setDefaultAnimationVars();
    resetHLConfig();

    Debug::log(INFO,
               "!!!!HEY YOU, YES YOU!!!!: further logs to stdout / logfile are disabled by default. BEFORE SENDING THIS LOG, ENABLE THEM. Use debug:disable_logs = false to do so: "
               "https://wiki.hyprland.org/Configuring/Variables/#debug");

    Debug::disableLogs = reinterpret_cast<int64_t* const*>(m_pConfig->getConfigValuePtr("debug:disable_logs")->getDataStaticPtr());
    Debug::disableTime = reinterpret_cast<int64_t* const*>(m_pConfig->getConfigValuePtr("debug:disable_time")->getDataStaticPtr());

    if (ERR.has_value())
        g_pHyprError->queueCreate(ERR.value(), CColor{1.0, 0.1, 0.1, 1.0});
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

const std::string CConfigManager::getConfigString() {
    std::string configString;
    std::string currFileContent;

    for (auto path : configPaths) {
        std::ifstream configFile(path);
        configString += ("\n\nConfig File: " + path + ": ");
        if (!configFile.is_open()) {
            Debug::log(LOG, "Config file not readable/found!");
            configString += "Read Failed\n";
            continue;
        }
        configString += "Read Succeeded\n";
        currFileContent.assign(std::istreambuf_iterator<char>(configFile), std::istreambuf_iterator<char>());
        configString.append(currFileContent);
    }
    return configString;
}

std::string CConfigManager::getErrors() {
    return m_szConfigErrors;
}

void CConfigManager::reload() {
    EMIT_HOOK_EVENT("preConfigReload", nullptr);
    setDefaultAnimationVars();
    resetHLConfig();
    configCurrentPath = getMainConfigPath();
    const auto ERR    = m_pConfig->parse();
    postConfigReload(ERR);
}

void CConfigManager::setDefaultAnimationVars() {
    if (isFirstLaunch) {
        INITANIMCFG("global");
        INITANIMCFG("windows");
        INITANIMCFG("layers");
        INITANIMCFG("fade");
        INITANIMCFG("border");
        INITANIMCFG("borderangle");
        INITANIMCFG("workspaces");

        // windows
        INITANIMCFG("windowsIn");
        INITANIMCFG("windowsOut");
        INITANIMCFG("windowsMove");

        // layers
        INITANIMCFG("layersIn");
        INITANIMCFG("layersOut");

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
    CREATEANIMCFG("layers", "global");
    CREATEANIMCFG("fade", "global");
    CREATEANIMCFG("border", "global");
    CREATEANIMCFG("borderangle", "global");
    CREATEANIMCFG("workspaces", "global");

    CREATEANIMCFG("layersIn", "layers");
    CREATEANIMCFG("layersOut", "layers");

    CREATEANIMCFG("windowsIn", "windows");
    CREATEANIMCFG("windowsOut", "windows");
    CREATEANIMCFG("windowsMove", "windows");

    CREATEANIMCFG("fadeIn", "fade");
    CREATEANIMCFG("fadeOut", "fade");
    CREATEANIMCFG("fadeSwitch", "fade");
    CREATEANIMCFG("fadeShadow", "fade");
    CREATEANIMCFG("fadeDim", "fade");
    CREATEANIMCFG("fadeLayers", "fade");
    CREATEANIMCFG("fadeLayersIn", "fadeLayers");
    CREATEANIMCFG("fadeLayersOut", "fadeLayers");

    CREATEANIMCFG("specialWorkspace", "workspaces");
}

std::optional<std::string> CConfigManager::verifyConfigExists() {
    std::string mainConfigPath = getMainConfigPath();

    if (g_pCompositor->explicitConfigPath.empty() && !std::filesystem::exists(mainConfigPath)) {
        std::string configPath = std::filesystem::path(mainConfigPath).parent_path();

        if (!std::filesystem::is_directory(configPath)) {
            Debug::log(WARN, "Creating config home directory");
            try {
                std::filesystem::create_directories(configPath);
            } catch (...) { return "Broken config file! (Could not create config directory)"; }
        }

        Debug::log(WARN, "No config file found; attempting to generate.");
        std::ofstream ofs;
        ofs.open(mainConfigPath, std::ios::trunc);
        ofs << AUTOCONFIG;
        ofs.close();
    }

    if (!std::filesystem::exists(mainConfigPath))
        return "broken config dir?";

    return {};
}

std::optional<std::string> CConfigManager::resetHLConfig() {
    m_dMonitorRules.clear();
    m_dWindowRules.clear();
    g_pKeybindManager->clearKeybinds();
    g_pAnimationManager->removeAllBeziers();
    m_mAdditionalReservedAreas.clear();
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

    const auto RET = verifyConfigExists();

    return RET;
}

void CConfigManager::postConfigReload(const Hyprlang::CParseResult& result) {
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

    if (result.error)
        m_szConfigErrors = result.getError();
    else
        m_szConfigErrors = "";

    if (result.error && !std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:suppress_errors")))
        g_pHyprError->queueCreate(result.getError(), CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    else if (std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("autogenerated")) == 1)
        g_pHyprError->queueCreate("Warning: You're using an autogenerated config! (config file: " + getMainConfigPath() + " )\nSUPER+Q -> kitty\nSUPER+M -> exit Hyprland",
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
    for (auto& w : g_pCompositor->m_vWorkspaces) {
        if (w->inert())
            continue;
        g_pCompositor->updateWorkspaceWindows(w->m_iID);
        g_pCompositor->updateWorkspaceSpecialRenderData(w->m_iID);
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // update layout
    g_pLayoutManager->switchToLayout(std::any_cast<Hyprlang::STRING>(m_pConfig->getConfigValue("general:layout")));

    // manual crash
    if (std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:manual_crash")) && !m_bManualCrashInitiated) {
        m_bManualCrashInitiated = true;
        g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CColor(0), 5000, ICON_INFO);
    } else if (m_bManualCrashInitiated && !std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:manual_crash"))) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    Debug::disableStdout = !std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:enable_stdout_logs"));
    if (Debug::disableStdout && isFirstLaunch)
        Debug::log(LOG, "Disabling stdout logs! Check the log for further logs.");

    Debug::coloredLogs = reinterpret_cast<int64_t* const*>(m_pConfig->getConfigValuePtr("debug:colored_stdout_logs")->getDataStaticPtr());

    for (auto& m : g_pCompositor->m_vMonitors) {
        // mark blur dirty
        g_pHyprOpenGL->markBlurDirtyForMonitor(m.get());

        g_pCompositor->scheduleFrameForMonitor(m.get());

        // Force the compositor to fully re-render all monitors
        m->forceFullFrames = 2;

        // also force mirrors, as the aspect ratio could've changed
        for (auto& mirror : m->mirrors)
            mirror->forceFullFrames = 3;
    }

    // Reset no monitor reload
    m_bNoMonitorReload = false;

    // update plugins
    handlePluginLoads();

    EMIT_HOOK_EVENT("configReloaded", nullptr);
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::init() {

    const std::string CONFIGPATH = getMainConfigPath();
    reload();

    struct stat fileStat;
    int         err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at statting config, error {}", errno);
    }

    configModifyTimes[CONFIGPATH] = fileStat.st_mtime;

    isFirstLaunch = false;
}

std::string CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE) {
    const auto RET = m_pConfig->parseDynamic(COMMAND.c_str(), VALUE.c_str());

    // invalidate layouts if they changed
    if (COMMAND == "monitor" || COMMAND.contains("gaps_") || COMMAND.starts_with("dwindle:") || COMMAND.starts_with("master:")) {
        for (auto& m : g_pCompositor->m_vMonitors)
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // manual crash
    if (std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:manual_crash")) && !m_bManualCrashInitiated) {
        m_bManualCrashInitiated = true;
        if (g_pHyprNotificationOverlay) {
            g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CColor(0), 5000,
                                                        ICON_INFO);
        }
    } else if (m_bManualCrashInitiated && !std::any_cast<Hyprlang::INT>(m_pConfig->getConfigValue("debug:manual_crash"))) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    return RET.error ? RET.getError() : "";
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

        reload();
    }
}

Hyprlang::CConfigValue* CConfigManager::getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback) {

    const auto VAL = m_pConfig->getSpecialConfigValuePtr("device", val.c_str(), dev.c_str());

    if ((!VAL || !VAL->m_bSetByUser) && !fallback.empty()) {
        return m_pConfig->getConfigValuePtr(fallback.c_str());
    }

    return VAL;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& v, const std::string& fallback) {
    return std::any_cast<Hyprlang::INT>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& v, const std::string& fallback) {
    return std::any_cast<Hyprlang::FLOAT>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& v, const std::string& fallback) {
    return std::any_cast<Hyprlang::VEC2>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& v, const std::string& fallback) {
    const auto VAL = std::string{std::any_cast<Hyprlang::STRING>(getConfigValueSafeDevice(dev, v, fallback)->getValue())};

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

SMonitorRule CConfigManager::getMonitorRuleFor(const CMonitor& PMONITOR) {
    for (auto& r : m_dMonitorRules | std::views::reverse) {
        if (PMONITOR.matchesStaticSelector(r.name)) {
            return r;
        }
    }

    Debug::log(WARN, "No rule found for {}, trying to use the first.", PMONITOR.szName);

    for (auto& r : m_dMonitorRules) {
        if (r.name.empty()) {
            return r;
        }
    }

    Debug::log(WARN, "No rules configured. Using the default hardcoded one.");

    return SMonitorRule{.autoDir    = eAutoDirs::DIR_AUTO_RIGHT,
                        .name       = "",
                        .resolution = Vector2D(0, 0),
                        .offset     = Vector2D(-INT32_MAX, -INT32_MAX),
                        .scale      = -1}; // 0, 0 is preferred and -1, -1 is auto
}

SWorkspaceRule CConfigManager::getWorkspaceRuleFor(PHLWORKSPACE pWorkspace) {
    SWorkspaceRule mergedRule{};
    for (auto& rule : m_dWorkspaceRules) {
        if (!pWorkspace->matchesStaticSelector(rule.workspaceString))
            continue;

        mergedRule = mergeWorkspaceRules(mergedRule, rule);
    }

    return mergedRule;
}

SWorkspaceRule CConfigManager::mergeWorkspaceRules(const SWorkspaceRule& rule1, const SWorkspaceRule& rule2) {
    SWorkspaceRule mergedRule = rule1;

    if (rule1.monitor.empty())
        mergedRule.monitor = rule2.monitor;
    if (rule1.workspaceString.empty())
        mergedRule.workspaceString = rule2.workspaceString;
    if (rule1.workspaceName.empty())
        mergedRule.workspaceName = rule2.workspaceName;
    if (rule1.workspaceId == WORKSPACE_INVALID)
        mergedRule.workspaceId = rule2.workspaceId;

    if (rule2.isDefault)
        mergedRule.isDefault = true;
    if (rule2.isPersistent)
        mergedRule.isPersistent = true;
    if (rule2.gapsIn.has_value())
        mergedRule.gapsIn = rule2.gapsIn;
    if (rule2.gapsOut.has_value())
        mergedRule.gapsOut = rule2.gapsOut;
    if (rule2.borderSize.has_value())
        mergedRule.borderSize = rule2.borderSize;
    if (rule2.border.has_value())
        mergedRule.border = rule2.border;
    if (rule2.rounding.has_value())
        mergedRule.rounding = rule2.rounding;
    if (rule2.decorate.has_value())
        mergedRule.decorate = rule2.decorate;
    if (rule2.shadow.has_value())
        mergedRule.shadow = rule2.shadow;
    if (rule2.onCreatedEmptyRunCmd.has_value())
        mergedRule.onCreatedEmptyRunCmd = rule2.onCreatedEmptyRunCmd;
    if (rule2.defaultName.has_value())
        mergedRule.defaultName = rule2.defaultName;
    if (!rule2.layoutopts.empty()) {
        for (const auto& layoutopt : rule2.layoutopts) {
            mergedRule.layoutopts[layoutopt.first] = layoutopt.second;
        }
    }
    return mergedRule;
}

std::vector<SWindowRule> CConfigManager::getMatchingRules(PHLWINDOW pWindow, bool dynamic, bool shadowExec) {
    if (!valid(pWindow))
        return std::vector<SWindowRule>();

    // if the window is unmapped, don't process exec rules yet.
    shadowExec = shadowExec || !pWindow->m_bIsMapped;

    std::vector<SWindowRule> returns;

    std::string              title      = pWindow->m_szTitle;
    std::string              appidclass = pWindow->m_szClass;

    Debug::log(LOG, "Searching for matching rules for {} (title: {})", appidclass, title);

    // since some rules will be applied later, we need to store some flags
    bool hasFloating   = pWindow->m_bIsFloating;
    bool hasFullscreen = pWindow->m_bIsFullscreen;

    // local tags for dynamic tag rule match
    auto tags = pWindow->m_tags;

    for (auto& rule : m_dWindowRules) {
        // check if we have a matching rule
        if (!rule.v2) {
            try {
                if (rule.szValue.starts_with("tag:") && !tags.isTagged(rule.szValue.substr(4)))
                    continue;

                if (rule.szValue.starts_with("title:")) {
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
                if (!rule.szTag.empty() && !tags.isTagged(rule.szTag))
                    continue;

                if (!rule.szClass.empty()) {
                    std::regex RULECHECK(rule.szClass);

                    if (!std::regex_search(appidclass, RULECHECK))
                        continue;
                }

                if (!rule.szTitle.empty()) {
                    std::regex RULECHECK(rule.szTitle);

                    if (!std::regex_search(title, RULECHECK))
                        continue;
                }

                if (!rule.szInitialTitle.empty()) {
                    std::regex RULECHECK(rule.szInitialTitle);

                    if (!std::regex_search(pWindow->m_szInitialTitle, RULECHECK))
                        continue;
                }

                if (!rule.szInitialClass.empty()) {
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
                    if (rule.bFocus != (g_pCompositor->m_pLastWindow.lock() == pWindow))
                        continue;
                }

                if (!rule.szOnWorkspace.empty()) {
                    const auto PWORKSPACE = pWindow->m_pWorkspace;
                    if (!PWORKSPACE || !PWORKSPACE->matchesStaticSelector(rule.szOnWorkspace))
                        continue;
                }

                if (!rule.szWorkspace.empty()) {
                    const auto PWORKSPACE = pWindow->m_pWorkspace;

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

        // apply tag with local tags
        if (rule.szRule.starts_with("tag")) {
            CVarList vars{rule.szRule, 0, 's', true};
            if (vars.size() == 2 && vars[0] == "tag")
                tags.applyTag(vars[1], true);
        }

        if (dynamic)
            continue;

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

    if (anyExecFound && !shadowExec) // remove exec rules to unclog searches in the future, why have the garbage here.
        execRequestedRules.erase(std::remove_if(execRequestedRules.begin(), execRequestedRules.end(),
                                                [&](const SExecRequestedRule& other) { return std::ranges::any_of(PIDs, [&](const auto& pid) { return pid == other.iPid; }); }));

    return returns;
}

std::vector<SLayerRule> CConfigManager::getMatchingRules(PHLLS pLS) {
    std::vector<SLayerRule> returns;

    if (!pLS->layerSurface || pLS->fadingOut)
        return returns;

    for (auto& lr : m_dLayerRules) {
        if (lr.targetNamespace.starts_with("address:0x")) {
            if (std::format("address:0x{:x}", (uintptr_t)pLS.get()) != lr.targetNamespace)
                continue;
        } else {
            std::regex NSCHECK(lr.targetNamespace);

            if (!std::regex_search(pLS->layerSurface->layerNamespace, NSCHECK))
                continue;
        }

        // hit
        returns.push_back(lr);
    }

    if (shouldBlurLS(pLS->layerSurface->layerNamespace))
        returns.push_back({pLS->layerSurface->layerNamespace, "blur"});

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

void CConfigManager::appendMonitorRule(const SMonitorRule& r) {
    m_dMonitorRules.emplace_back(r);
}

bool CConfigManager::replaceMonitorRule(const SMonitorRule& newrule) {
    // Looks for an existing monitor rule (compared by name).
    // If the rule exists, it is replaced with the input rule.
    for (auto& r : m_dMonitorRules) {
        if (r.name == newrule.name) {
            r = newrule;
            return true;
        }
    }
    return false;
}

void CConfigManager::performMonitorReload() {

    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (!m->output || m->isUnsafeFallback)
            continue;

        auto rule = getMonitorRuleFor(*m);

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

void* const* CConfigManager::getConfigValuePtr(const std::string& val) {
    const auto VAL = m_pConfig->getConfigValuePtr(val.c_str());
    if (!VAL)
        return nullptr;
    return VAL->getDataStaticPtr();
}

Hyprlang::CConfigValue* CConfigManager::getHyprlangConfigValuePtr(const std::string& name, const std::string& specialCat) {
    if (!specialCat.empty())
        return m_pConfig->getSpecialConfigValuePtr(specialCat.c_str(), name.c_str(), nullptr);

    return m_pConfig->getConfigValuePtr(name.c_str());
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    auto copy = dev;
    std::replace(copy.begin(), copy.end(), ' ', '-');

    return m_pConfig->specialCategoryExistsForKey("device", copy.c_str());
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

        auto rule = getMonitorRuleFor(*rm);

        if (rule.disabled == rm->m_bEnabled)
            g_pHyprRenderer->applyMonitorRule(rm.get(), &rule);
    }
}

void CConfigManager::ensureVRR(CMonitor* pMonitor) {
    static auto PVRR = reinterpret_cast<Hyprlang::INT* const*>(getConfigValuePtr("misc:vrr"));

    static auto ensureVRRForDisplay = [&](CMonitor* m) -> void {
        if (!m->output || m->createdByUser)
            return;

        const auto USEVRR = m->activeMonitorRule.vrr.has_value() ? m->activeMonitorRule.vrr.value() : **PVRR;

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

            const auto PWORKSPACE = m->activeWorkspace;

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
    g_pHyprError->queueCreate(err + "\nHyprland may not work correctly.", CColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
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

        if (WSNAME == wsname)
            return wr.monitor;
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

void onPluginLoadUnload(const std::string& name, bool load) {
    //
}

void CConfigManager::addPluginConfigVar(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value) {
    if (!name.starts_with("plugin:"))
        return;

    std::string field = name.substr(7);

    m_pConfig->addSpecialConfigValue("plugin", field.c_str(), value);
    pluginVariables.push_back({handle, field});
}

void CConfigManager::addPluginKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fn, Hyprlang::SHandlerOptions opts) {
    pluginKeywords.emplace_back(SPluginKeyword{handle, name, fn});
    m_pConfig->registerHandler(fn, name.c_str(), opts);
}

void CConfigManager::removePluginConfig(HANDLE handle) {
    for (auto& k : pluginKeywords) {
        if (k.handle != handle)
            continue;

        m_pConfig->unregisterHandler(k.name.c_str());
    }

    std::erase_if(pluginKeywords, [&](const auto& other) { return other.handle == handle; });
    for (auto& [h, n] : pluginVariables) {
        if (h != handle)
            continue;

        m_pConfig->removeSpecialConfigValue("plugin", n.c_str());
    }
    std::erase_if(pluginVariables, [handle](const auto& other) { return other.handle == handle; });
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

std::optional<std::string> CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    if (isFirstLaunch) {
        firstExecRequests.push_back(args);
        return {};
    }

    g_pKeybindManager->spawn(args);
    return {};
}

std::optional<std::string> CConfigManager::handleExecOnce(const std::string& command, const std::string& args) {
    if (isFirstLaunch)
        firstExecRequests.push_back(args);

    return {};
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

std::optional<std::string> CConfigManager::handleMonitor(const std::string& command, const std::string& args) {

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
                return "invalid transform";
            }

            const auto TRANSFORM = (wl_output_transform)TSF;

            // overwrite if exists
            for (auto& r : m_dMonitorRules) {
                if (r.name == newrule.name) {
                    r.transform = TRANSFORM;
                    return {};
                }
            }

            return {};
        } else if (ARGS[1] == "addreserved") {
            int top = std::stoi(ARGS[2]);

            int bottom = std::stoi(ARGS[3]);

            int left = std::stoi(ARGS[4]);

            int right = std::stoi(ARGS[5]);

            m_mAdditionalReservedAreas[newrule.name] = {top, bottom, left, right};

            return {};
        } else {
            Debug::log(ERR, "ConfigManager parseMonitor, curitem bogus???");
            return "parse error: curitem bogus";
        }

        std::erase_if(m_dMonitorRules, [&](const auto& other) { return other.name == newrule.name; });

        m_dMonitorRules.push_back(newrule);

        return {};
    }

    std::string error = "";

    if (ARGS[1].starts_with("pref")) {
        newrule.resolution = Vector2D();
    } else if (ARGS[1].starts_with("highrr")) {
        newrule.resolution = Vector2D(-1, -1);
    } else if (ARGS[1].starts_with("highres")) {
        newrule.resolution = Vector2D(-1, -2);
    } else if (parseModeLine(ARGS[1], newrule.drmMode)) {
        newrule.resolution  = Vector2D(newrule.drmMode.hdisplay, newrule.drmMode.vdisplay);
        newrule.refreshRate = float(newrule.drmMode.vrefresh) / 1000;
    } else {

        if (!ARGS[1].contains("x")) {
            error += "invalid resolution ";
            newrule.resolution = Vector2D();
        } else {
            newrule.resolution.x = stoi(ARGS[1].substr(0, ARGS[1].find_first_of('x')));
            newrule.resolution.y = stoi(ARGS[1].substr(ARGS[1].find_first_of('x') + 1, ARGS[1].find_first_of('@')));

            if (ARGS[1].contains("@"))
                newrule.refreshRate = stof(ARGS[1].substr(ARGS[1].find_first_of('@') + 1));
        }
    }

    if (ARGS[2].starts_with("auto")) {
        newrule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
        // If this is the first monitor rule needs to be on the right.
        if (ARGS[2] == "auto-right" || ARGS[2] == "auto" || m_dMonitorRules.empty())
            newrule.autoDir = eAutoDirs::DIR_AUTO_RIGHT;
        else if (ARGS[2] == "auto-left")
            newrule.autoDir = eAutoDirs::DIR_AUTO_LEFT;
        else if (ARGS[2] == "auto-up")
            newrule.autoDir = eAutoDirs::DIR_AUTO_UP;
        else if (ARGS[2] == "auto-down")
            newrule.autoDir = eAutoDirs::DIR_AUTO_DOWN;
        else {
            Debug::log(WARN,
                       "Invalid auto direction. Valid options are 'auto',"
                       "'auto-up', 'auto-down', 'auto-left', and 'auto-right'.");
            error += "invalid auto direction ";
        }
    } else {
        if (!ARGS[2].contains("x")) {
            error += "invalid offset ";
            newrule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
        } else {
            newrule.offset.x = stoi(ARGS[2].substr(0, ARGS[2].find_first_of('x')));
            newrule.offset.y = stoi(ARGS[2].substr(ARGS[2].find_first_of('x') + 1));
        }
    }

    if (ARGS[3].starts_with("auto")) {
        newrule.scale = -1;
    } else {
        if (!isNumber(ARGS[3], true))
            error += "invalid scale ";
        else {
            newrule.scale = stof(ARGS[3]);

            if (newrule.scale < 0.25f) {
                error += "invalid scale ";
                newrule.scale = 1;
            }
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
            if (!isNumber(ARGS[argno + 1])) {
                error = "invalid transform ";
                argno++;
                continue;
            }

            const auto NUM = std::stoi(ARGS[argno + 1]);

            if (NUM < 0 || NUM > 7) {
                error = "invalid transform ";
                argno++;
                continue;
            }

            newrule.transform = (wl_output_transform)std::stoi(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "vrr") {
            if (!isNumber(ARGS[argno + 1])) {
                error = "invalid vrr ";
                argno++;
                continue;
            }

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
            return "invalid syntax at \"" + ARGS[argno] + "\"";
        }

        argno++;
    }

    std::erase_if(m_dMonitorRules, [&](const auto& other) { return other.name == newrule.name; });

    m_dMonitorRules.push_back(newrule);

    if (error.empty())
        return {};
    return error;
}

std::optional<std::string> CConfigManager::handleBezier(const std::string& command, const std::string& args) {
    const auto  ARGS = CVarList(args);

    std::string bezierName = ARGS[0];

    if (ARGS[1] == "")
        return "too few arguments";
    float p1x = std::stof(ARGS[1]);

    if (ARGS[2] == "")
        return "too few arguments";
    float p1y = std::stof(ARGS[2]);

    if (ARGS[3] == "")
        return "too few arguments";
    float p2x = std::stof(ARGS[3]);

    if (ARGS[4] == "")
        return "too few arguments";
    float p2y = std::stof(ARGS[4]);

    if (ARGS[5] != "")
        return "too many arguments";

    g_pAnimationManager->addBezierWithName(bezierName, Vector2D(p1x, p1y), Vector2D(p2x, p2y));

    return {};
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

std::optional<std::string> CConfigManager::handleAnimation(const std::string& command, const std::string& args) {
    const auto ARGS = CVarList(args);

    // Master on/off

    // anim name
    const auto ANIMNAME = ARGS[0];

    const auto PANIM = animationConfig.find(ANIMNAME);

    if (PANIM == animationConfig.end())
        return "no such animation";

    PANIM->second.overridden = true;
    PANIM->second.pValues    = &PANIM->second;

    // This helper casts strings like "1", "true", "off", "yes"... to int.
    int64_t enabledInt = configStringToInt(ARGS[1]) == 1;

    // Checking that the int is 1 or 0 because the helper can return integers out of range.
    if (enabledInt != 0 && enabledInt != 1)
        return "invalid animation on/off state";

    PANIM->second.internalEnabled = configStringToInt(ARGS[1]) == 1;

    if (PANIM->second.internalEnabled) {
        // speed
        if (isNumber(ARGS[2], true)) {
            PANIM->second.internalSpeed = std::stof(ARGS[2]);

            if (PANIM->second.internalSpeed <= 0) {
                PANIM->second.internalSpeed = 1.f;
                return "invalid speed";
            }
        } else {
            PANIM->second.internalSpeed = 10.f;
            return "invalid speed";
        }

        // curve
        PANIM->second.internalBezier = ARGS[3];

        if (!g_pAnimationManager->bezierExists(ARGS[3])) {
            PANIM->second.internalBezier = "default";
            return "no such bezier";
        }

        // style
        PANIM->second.internalStyle = ARGS[4];

        if (ARGS[4] != "") {
            const auto ERR = g_pAnimationManager->styleValidInConfigVar(ANIMNAME, ARGS[4]);

            if (ERR != "")
                return ERR;
        }
    }

    // now, check for children, recursively
    setAnimForChildren(&PANIM->second);

    return {};
}

SParsedKey parseKey(const std::string& key) {
    if (isNumber(key) && std::stoi(key) > 9)
        return {.keycode = std::stoi(key)};
    else if (key.starts_with("code:") && isNumber(key.substr(5)))
        return {.keycode = std::stoi(key.substr(5))};
    else if (key == "catchall")
        return {.catchAll = true};
    else
        return {.key = key};
}

std::optional<std::string> CConfigManager::handleBind(const std::string& command, const std::string& value) {
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
    bool       multiKey     = false;
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
        } else if (arg == 's') {
            multiKey = true;
        } else {
            return "bind: invalid flag";
        }
    }

    if (release && repeat)
        return "flags r and e are mutually exclusive";

    if (mouse && (repeat || release || locked))
        return "flag m is exclusive";

    const auto ARGS = CVarList(value, 4);

    if ((ARGS.size() < 3 && !mouse) || (ARGS.size() < 3 && mouse))
        return "bind: too few args";
    else if ((ARGS.size() > 4 && !mouse) || (ARGS.size() > 3 && mouse))
        return "bind: too many args";

    std::set<xkb_keysym_t> KEYSYMS;
    std::set<xkb_keysym_t> MODS;

    if (multiKey) {
        for (auto splitKey : CVarList(ARGS[1], 8, '&')) {
            KEYSYMS.insert(xkb_keysym_from_name(splitKey.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
        for (auto splitMod : CVarList(ARGS[0], 8, '&')) {
            MODS.insert(xkb_keysym_from_name(splitMod.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
    }
    const auto MOD    = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto MODSTR = ARGS[0];

    const auto KEY = multiKey ? "" : ARGS[1];

    auto       HANDLER = ARGS[2];

    const auto COMMAND = mouse ? HANDLER : ARGS[3];

    if (mouse)
        HANDLER = "mouse";

    // to lower
    std::transform(HANDLER.begin(), HANDLER.end(), HANDLER.begin(), ::tolower);

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(HANDLER);

    if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end()) {
        Debug::log(ERR, "Invalid dispatcher!");
        return "Invalid dispatcher, requested \"" + HANDLER + "\" does not exist";
    }

    if (MOD == 0 && MODSTR != "") {
        Debug::log(ERR, "Invalid mod!");
        return "Invalid mod, requested mod \"" + MODSTR + "\" is not a valid mod.";
    }

    if ((KEY != "") || multiKey) {
        SParsedKey parsedKey = parseKey(KEY);

        if (parsedKey.catchAll && m_szCurrentSubmap.empty()) {
            Debug::log(ERR, "Catchall not allowed outside of submap!");
            return "Invalid catchall, catchall keybinds are only allowed in submaps.";
        }

        g_pKeybindManager->addKeybind(SKeybind{parsedKey.key, KEYSYMS, parsedKey.keycode, parsedKey.catchAll, MOD, MODS, HANDLER, COMMAND, locked, m_szCurrentSubmap, release,
                                               repeat, mouse, nonConsuming, transparent, ignoreMods, multiKey});
    }

    return {};
}

std::optional<std::string> CConfigManager::handleUnbind(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    const auto MOD = g_pKeybindManager->stringToModMask(ARGS[0]);

    const auto KEY = parseKey(ARGS[1]);

    g_pKeybindManager->removeKeybind(MOD, KEY);

    return {};
}

bool windowRuleValid(const std::string& RULE) {
    static const auto rules = std::unordered_set<std::string>{
        "dimaround",       "fakefullscreen", "float",           "focusonactivate", "forceinput", "forcergbx",   "fullscreen", "immediate",
        "keepaspectratio", "maximize",       "nearestneighbor", "noanim",          "noblur",     "noborder",    "nodim",      "nofocus",
        "noinitialfocus",  "nomaxsize",      "noshadow",        "opaque",          "pin",        "stayfocused", "tile",       "windowdance",
    };
    static const auto rulesPrefix = std::vector<std::string>{
        "animation", "bordercolor", "bordersize", "center",   "group", "idleinhibit",   "maxsize", "minsize",   "monitor", "move",
        "opacity",   "plugin:",     "pseudo",     "rounding", "size",  "suppressevent", "tag",     "workspace", "xray",
    };

    return rules.contains(RULE) || std::any_of(rulesPrefix.begin(), rulesPrefix.end(), [&RULE](auto prefix) { return RULE.starts_with(prefix); });
}

bool layerRuleValid(const std::string& RULE) {
    static const auto rules       = std::unordered_set<std::string>{"noanim", "blur", "blurpopups", "dimaround"};
    static const auto rulesPrefix = std::vector<std::string>{"ignorealpha", "ignorezero", "xray", "animation"};

    return rules.contains(RULE) || std::any_of(rulesPrefix.begin(), rulesPrefix.end(), [&RULE](auto prefix) { return RULE.starts_with(prefix); });
}

std::optional<std::string> CConfigManager::handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = removeBeginEndSpacesTabs(value.substr(value.find_first_of(',') + 1));

    // check rule and value
    if (RULE.empty() || VALUE.empty())
        return "empty rule?";

    if (RULE == "unset") {
        std::erase_if(m_dWindowRules, [&](const SWindowRule& other) { return other.szValue == VALUE; });
        return {};
    }

    // verify we support a rule
    if (!windowRuleValid(RULE)) {
        Debug::log(ERR, "Invalid rule found: {}", RULE);
        return "Invalid rule: " + RULE;
    }

    if (RULE.starts_with("size") || RULE.starts_with("maxsize") || RULE.starts_with("minsize"))
        m_dWindowRules.push_front({RULE, VALUE});
    else
        m_dWindowRules.push_back({RULE, VALUE});

    return {};
}

std::optional<std::string> CConfigManager::handleLayerRule(const std::string& command, const std::string& value) {
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = removeBeginEndSpacesTabs(value.substr(value.find_first_of(',') + 1));

    // check rule and value
    if (RULE.empty() || VALUE.empty())
        return "empty rule?";

    if (RULE == "unset") {
        std::erase_if(m_dLayerRules, [&](const SLayerRule& other) { return other.targetNamespace == VALUE; });
        return {};
    }

    if (!layerRuleValid(RULE)) {
        Debug::log(ERR, "Invalid rule found: {}", RULE);
        return "Invalid rule found: " + RULE;
    }

    m_dLayerRules.push_back({VALUE, RULE});

    for (auto& m : g_pCompositor->m_vMonitors)
        for (auto& lsl : m->m_aLayerSurfaceLayers)
            for (auto& ls : lsl)
                ls->applyRules();

    return {};
}

std::optional<std::string> CConfigManager::handleWindowRuleV2(const std::string& command, const std::string& value) {
    const auto RULE  = removeBeginEndSpacesTabs(value.substr(0, value.find_first_of(',')));
    const auto VALUE = value.substr(value.find_first_of(',') + 1);

    if (!windowRuleValid(RULE) && RULE != "unset") {
        Debug::log(ERR, "Invalid rulev2 found: {}", RULE);
        return "Invalid rulev2 found: " + RULE;
    }

    // now we estract shit from the value
    SWindowRule rule;
    rule.v2      = true;
    rule.szRule  = RULE;
    rule.szValue = VALUE;

    const auto TAGPOS          = VALUE.find("tag:");
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

    const auto checkPos = std::unordered_set{
        TAGPOS, TITLEPOS, CLASSPOS, INITIALTITLEPOS, INITIALCLASSPOS, X11POS, FLOATPOS, FULLSCREENPOS, PINNEDPOS, WORKSPACEPOS, FOCUSPOS, ONWORKSPACEPOS,
    };
    if (checkPos.size() == 1 && checkPos.contains(std::string::npos)) {
        Debug::log(ERR, "Invalid rulev2 syntax: {}", VALUE);
        return "Invalid rulev2 syntax: " + VALUE;
    }

    auto extract = [&](size_t pos) -> std::string {
        std::string result;
        result = VALUE.substr(pos);

        size_t min = 999999;
        if (TAGPOS > pos && TAGPOS < min)
            min = TAGPOS;
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

        if (!result.empty() && result.back() == ',')
            result.pop_back();

        return result;
    };

    if (TAGPOS != std::string::npos)
        rule.szTag = extract(TAGPOS + 4);

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
        rule.szOnWorkspace = extract(ONWORKSPACEPOS + 12);

    if (RULE == "unset") {
        std::erase_if(m_dWindowRules, [&](const SWindowRule& other) {
            if (!other.v2) {
                return other.szClass == rule.szClass && !rule.szClass.empty();
            } else {
                if (!rule.szTag.empty() && rule.szTag != other.szTag)
                    return false;

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

                if (!rule.szOnWorkspace.empty() && rule.szOnWorkspace != other.szOnWorkspace)
                    return false;

                return true;
            }
        });
        return {};
    }

    if (RULE.starts_with("size") || RULE.starts_with("maxsize") || RULE.starts_with("minsize"))
        m_dWindowRules.push_front(rule);
    else
        m_dWindowRules.push_back(rule);

    return {};
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

std::optional<std::string> CConfigManager::handleBlurLS(const std::string& command, const std::string& value) {
    if (value.starts_with("remove,")) {
        const auto TOREMOVE = removeBeginEndSpacesTabs(value.substr(7));
        if (std::erase_if(m_dBlurLSNamespaces, [&](const auto& other) { return other == TOREMOVE; }))
            updateBlurredLS(TOREMOVE, false);
        return {};
    }

    m_dBlurLSNamespaces.emplace_back(value);
    updateBlurredLS(value, true);

    return {};
}

std::optional<std::string> CConfigManager::handleWorkspaceRules(const std::string& command, const std::string& value) {
    // This can either be the monitor or the workspace identifier
    const auto     FIRST_DELIM = value.find_first_of(',');

    std::string    name        = "";
    auto           first_ident = removeBeginEndSpacesTabs(value.substr(0, FIRST_DELIM));
    int            id          = getWorkspaceIDFromString(first_ident, name);

    auto           rules = value.substr(FIRST_DELIM + 1);
    SWorkspaceRule wsRule;
    wsRule.workspaceString = first_ident;
    // if (id == WORKSPACE_INVALID) {
    //     // it could be the monitor. If so, second value MUST be
    //     // the workspace.
    //     const auto WORKSPACE_DELIM = value.find_first_of(',', FIRST_DELIM + 1);
    //     auto       wsIdent         = removeBeginEndSpacesTabs(value.substr(FIRST_DELIM + 1, (WORKSPACE_DELIM - FIRST_DELIM - 1)));
    //     id                         = getWorkspaceIDFromString(wsIdent, name);
    //     if (id == WORKSPACE_INVALID) {
    //         Debug::log(ERR, "Invalid workspace identifier found: {}", wsIdent);
    //         return "Invalid workspace identifier found: " + wsIdent;
    //     }
    //     wsRule.monitor         = first_ident;
    //     wsRule.workspaceString = wsIdent;
    //     wsRule.isDefault       = true; // backwards compat
    //     rules                  = value.substr(WORKSPACE_DELIM + 1);
    // }

    const static std::string ruleOnCreatedEmpty    = "on-created-empty:";
    const static int         ruleOnCreatedEmptyLen = ruleOnCreatedEmpty.length();

    auto                     assignRule = [&](std::string rule) -> std::optional<std::string> {
        size_t delim = std::string::npos;
        if ((delim = rule.find("gapsin:")) != std::string::npos) {
            CVarList varlist = CVarList(rule.substr(delim + 7), 0, ' ');
            wsRule.gapsIn    = CCssGapData();
            try {
                wsRule.gapsIn->parseGapData(varlist);
            } catch (...) { return "Error parsing workspace rule gaps: {}", rule.substr(delim + 7); }
        } else if ((delim = rule.find("gapsout:")) != std::string::npos) {
            CVarList varlist = CVarList(rule.substr(delim + 8), 0, ' ');
            wsRule.gapsOut   = CCssGapData();
            try {
                wsRule.gapsOut->parseGapData(varlist);
            } catch (...) { return "Error parsing workspace rule gaps: {}", rule.substr(delim + 8); }
        } else if ((delim = rule.find("bordersize:")) != std::string::npos)
            try {
                wsRule.borderSize = std::stoi(rule.substr(delim + 11));
            } catch (...) { return "Error parsing workspace rule bordersize: {}", rule.substr(delim + 11); }
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
        else if ((delim = rule.find("defaultName:")) != std::string::npos)
            wsRule.defaultName = rule.substr(delim + 12);
        else if ((delim = rule.find(ruleOnCreatedEmpty)) != std::string::npos)
            wsRule.onCreatedEmptyRunCmd = cleanCmdForWorkspace(name, rule.substr(delim + ruleOnCreatedEmptyLen));
        else if ((delim = rule.find("layoutopt:")) != std::string::npos) {
            std::string opt = rule.substr(delim + 10);
            if (!opt.contains(":")) {
                // invalid
                Debug::log(ERR, "Invalid workspace rule found: {}", rule);
                return "Invalid workspace rule found: " + rule;
            }

            std::string val = opt.substr(opt.find(":") + 1);
            opt             = opt.substr(0, opt.find(":"));

            wsRule.layoutopts[opt] = val;
        }

        return {};
    };

    CVarList rulesList{rules, 0, ',', true};
    for (auto& r : rulesList) {
        const auto R = assignRule(r);
        if (R.has_value())
            return R;
    }

    wsRule.workspaceId   = id;
    wsRule.workspaceName = name;

    const auto IT = std::find_if(m_dWorkspaceRules.begin(), m_dWorkspaceRules.end(), [&](const auto& other) { return other.workspaceString == wsRule.workspaceString; });

    if (IT == m_dWorkspaceRules.end())
        m_dWorkspaceRules.emplace_back(wsRule);
    else
        *IT = mergeWorkspaceRules(*IT, wsRule);

    return {};
}

std::optional<std::string> CConfigManager::handleSubmap(const std::string& command, const std::string& submap) {
    if (submap == "reset")
        m_szCurrentSubmap = "";
    else
        m_szCurrentSubmap = submap;

    return {};
}

std::optional<std::string> CConfigManager::handleSource(const std::string& command, const std::string& rawpath) {
    if (rawpath.length() < 2) {
        Debug::log(ERR, "source= path garbage");
        return "source path " + rawpath + " bogus!";
    }
    std::unique_ptr<glob_t, void (*)(glob_t*)> glob_buf{new glob_t, [](glob_t* g) { globfree(g); }};
    memset(glob_buf.get(), 0, sizeof(glob_t));

    if (auto r = glob(absolutePath(rawpath, configCurrentPath).c_str(), GLOB_TILDE, nullptr, glob_buf.get()); r != 0) {
        std::string err = std::format("source= globbing error: {}", r == GLOB_NOMATCH ? "found no match" : GLOB_ABORTED ? "read error" : "out of memory");
        Debug::log(ERR, "{}", err);
        return err;
    }

    std::string errorsFromParsing;

    for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
        auto value = absolutePath(glob_buf->gl_pathv[i], configCurrentPath);

        if (!std::filesystem::is_regular_file(value)) {
            if (std::filesystem::exists(value)) {
                Debug::log(WARN, "source= skipping non-file {}", value);
                continue;
            }

            Debug::log(ERR, "source= file doesnt exist");
            return "source file " + value + " doesn't exist!";
        }
        configPaths.push_back(value);

        struct stat fileStat;
        int         err = stat(value.c_str(), &fileStat);
        if (err != 0) {
            Debug::log(WARN, "Error at ticking config at {}, error {}: {}", value, err, strerror(err));
            return {};
        }

        configModifyTimes[value]     = fileStat.st_mtime;
        auto configCurrentPathBackup = configCurrentPath;
        configCurrentPath            = value;

        const auto THISRESULT = m_pConfig->parseFile(value.c_str());

        configCurrentPath = configCurrentPathBackup;

        if (THISRESULT.error && errorsFromParsing.empty())
            errorsFromParsing += THISRESULT.getError();
    }

    if (errorsFromParsing.empty())
        return {};
    return errorsFromParsing;
}

std::optional<std::string> CConfigManager::handleEnv(const std::string& command, const std::string& value) {
    if (!isFirstLaunch)
        return {};

    const auto ARGS = CVarList(value, 2);

    if (ARGS[0].empty())
        return "env empty";

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

    return {};
}

std::optional<std::string> CConfigManager::handlePlugin(const std::string& command, const std::string& path) {
    if (std::find(m_vDeclaredPlugins.begin(), m_vDeclaredPlugins.end(), path) != m_vDeclaredPlugins.end())
        return "plugin '" + path + "' declared twice";

    m_vDeclaredPlugins.push_back(path);

    return {};
}
