#include <re2/re2.h>

#include "ConfigManager.hpp"
#include "ConfigWatcher.hpp"
#include "../managers/KeybindManager.hpp"
#include "../Compositor.hpp"

#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "config/ConfigDataValues.hpp"
#include "config/ConfigValue.hpp"
#include "helpers/varlist/VarList.hpp"
#include "../protocols/LayerShell.hpp"
#include "../xwayland/XWayland.hpp"
#include "../protocols/OutputManagement.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../desktop/LayerSurface.hpp"
#include "defaultConfig.hpp"

#include "../render/Renderer.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/permissions/DynamicPermissionManager.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "../plugins/PluginSystem.hpp"

#include "../managers/input/trackpad/TrackpadGestures.hpp"
#include "../managers/input/trackpad/gestures/DispatcherGesture.hpp"
#include "../managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp"
#include "../managers/input/trackpad/gestures/ResizeGesture.hpp"
#include "../managers/input/trackpad/gestures/MoveGesture.hpp"
#include "../managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"
#include "../managers/input/trackpad/gestures/CloseGesture.hpp"
#include "../managers/input/trackpad/gestures/FloatGesture.hpp"
#include "../managers/input/trackpad/gestures/FullscreenGesture.hpp"

#include "../managers/HookSystemManager.hpp"
#include "../protocols/types/ContentType.hpp"
#include <cstddef>
#include <cstdint>
#include <hyprutils/path/Path.hpp>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ranges>
#include <unordered_set>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <filesystem>
#include <memory>
using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using enum NContentType::eContentType;

//NOLINTNEXTLINE
extern "C" char** environ;

#include "ConfigDescriptions.hpp"

static Hyprlang::CParseResult configHandleGradientSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new CGradientValueData();

    const auto DATA = sc<CGradientValueData*>(*data);

    CVarList   varlist(V, 0, ' ');
    DATA->m_colors.clear();

    std::string parseError = "";

    for (auto const& var : varlist) {
        if (var.find("deg") != std::string::npos) {
            // last arg
            try {
                DATA->m_angle = std::stoi(var.substr(0, var.find("deg"))) * (PI / 180.0); // radians
            } catch (...) {
                Debug::log(WARN, "Error parsing gradient {}", V);
                parseError = "Error parsing gradient " + V;
            }

            break;
        }

        if (DATA->m_colors.size() >= 10) {
            Debug::log(WARN, "Error parsing gradient {}: max colors is 10.", V);
            parseError = "Error parsing gradient " + V + ": max colors is 10.";
            break;
        }

        try {
            const auto COL = configStringToInt(var);
            if (!COL)
                throw std::runtime_error(std::format("failed to parse {} as a color", var));
            DATA->m_colors.emplace_back(COL.value());
        } catch (std::exception& e) {
            Debug::log(WARN, "Error parsing gradient {}", V);
            parseError = "Error parsing gradient " + V + ": " + e.what();
        }
    }

    if (DATA->m_colors.empty()) {
        Debug::log(WARN, "Error parsing gradient {}", V);
        if (parseError.empty())
            parseError = "Error parsing gradient " + V + ": No colors?";

        DATA->m_colors.emplace_back(0); // transparent
    }

    DATA->updateColorsOk();

    Hyprlang::CParseResult result;
    if (!parseError.empty())
        result.setError(parseError.c_str());

    return result;
}

static void configHandleGradientDestroy(void** data) {
    if (*data)
        delete sc<CGradientValueData*>(*data);
}

static Hyprlang::CParseResult configHandleGapSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new CCssGapData();

    const auto             DATA = sc<CCssGapData*>(*data);
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
        delete sc<CCssGapData*>(*data);
}

static Hyprlang::CParseResult configHandleFontWeightSet(const char* VALUE, void** data) {
    if (!*data)
        *data = new CFontWeightConfigValueData();

    const auto             DATA = sc<CFontWeightConfigValueData*>(*data);
    Hyprlang::CParseResult result;

    try {
        DATA->parseWeight(VALUE);
    } catch (...) {
        std::string parseError = std::format("{} is not a valid font weight", VALUE);
        result.setError(parseError.c_str());
    }

    return result;
}

static void configHandleFontWeightDestroy(void** data) {
    if (*data)
        delete sc<CFontWeightConfigValueData*>(*data);
}

static Hyprlang::CParseResult handleExec(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleExec(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
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

static Hyprlang::CParseResult handleExecRawOnce(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleExecRawOnce(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleExecShutdown(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleExecShutdown(COMMAND, VALUE);

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

    const auto             RESULT = g_pConfigManager->handleWindowRule(COMMAND, VALUE);

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

static Hyprlang::CParseResult handlePermission(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handlePermission(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleGesture(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = g_pConfigManager->handleGesture(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

void CConfigManager::registerConfigVar(const char* name, const Hyprlang::INT& val) {
    m_configValueNumber++;
    m_config->addConfigValue(name, val);
}

void CConfigManager::registerConfigVar(const char* name, const Hyprlang::FLOAT& val) {
    m_configValueNumber++;
    m_config->addConfigValue(name, val);
}

void CConfigManager::registerConfigVar(const char* name, const Hyprlang::VEC2& val) {
    m_configValueNumber++;
    m_config->addConfigValue(name, val);
}

void CConfigManager::registerConfigVar(const char* name, const Hyprlang::STRING& val) {
    m_configValueNumber++;
    m_config->addConfigValue(name, val);
}

void CConfigManager::registerConfigVar(const char* name, Hyprlang::CUSTOMTYPE&& val) {
    m_configValueNumber++;
    m_config->addConfigValue(name, std::move(val));
}

CConfigManager::CConfigManager() {
    const auto ERR = verifyConfigExists();

    m_configPaths.emplace_back(getMainConfigPath());
    m_config = makeUnique<Hyprlang::CConfig>(m_configPaths.begin()->c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true});

    registerConfigVar("general:border_size", Hyprlang::INT{1});
    registerConfigVar("general:no_border_on_floating", Hyprlang::INT{0});
    registerConfigVar("general:gaps_in", Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, "5"});
    registerConfigVar("general:gaps_out", Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, "20"});
    registerConfigVar("general:float_gaps", Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, "0"});
    registerConfigVar("general:gaps_workspaces", Hyprlang::INT{0});
    registerConfigVar("general:no_focus_fallback", Hyprlang::INT{0});
    registerConfigVar("general:resize_on_border", Hyprlang::INT{0});
    registerConfigVar("general:extend_border_grab_area", Hyprlang::INT{15});
    registerConfigVar("general:hover_icon_on_border", Hyprlang::INT{1});
    registerConfigVar("general:layout", {"dwindle"});
    registerConfigVar("general:allow_tearing", Hyprlang::INT{0});
    registerConfigVar("general:resize_corner", Hyprlang::INT{0});
    registerConfigVar("general:snap:enabled", Hyprlang::INT{0});
    registerConfigVar("general:snap:window_gap", Hyprlang::INT{10});
    registerConfigVar("general:snap:monitor_gap", Hyprlang::INT{10});
    registerConfigVar("general:snap:border_overlap", Hyprlang::INT{0});
    registerConfigVar("general:snap:respect_gaps", Hyprlang::INT{0});
    registerConfigVar("general:col.active_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffffffff"});
    registerConfigVar("general:col.inactive_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xff444444"});
    registerConfigVar("general:col.nogroup_border", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffffaaff"});
    registerConfigVar("general:col.nogroup_border_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0xffff00ff"});

    registerConfigVar("misc:disable_hyprland_logo", Hyprlang::INT{0});
    registerConfigVar("misc:disable_splash_rendering", Hyprlang::INT{0});
    registerConfigVar("misc:col.splash", Hyprlang::INT{0x55ffffff});
    registerConfigVar("misc:splash_font_family", {STRVAL_EMPTY});
    registerConfigVar("misc:font_family", {"Sans"});
    registerConfigVar("misc:force_default_wallpaper", Hyprlang::INT{-1});
    registerConfigVar("misc:vfr", Hyprlang::INT{1});
    registerConfigVar("misc:vrr", Hyprlang::INT{0});
    registerConfigVar("misc:mouse_move_enables_dpms", Hyprlang::INT{0});
    registerConfigVar("misc:key_press_enables_dpms", Hyprlang::INT{0});
    registerConfigVar("misc:name_vk_after_proc", Hyprlang::INT{1});
    registerConfigVar("misc:always_follow_on_dnd", Hyprlang::INT{1});
    registerConfigVar("misc:layers_hog_keyboard_focus", Hyprlang::INT{1});
    registerConfigVar("misc:animate_manual_resizes", Hyprlang::INT{0});
    registerConfigVar("misc:animate_mouse_windowdragging", Hyprlang::INT{0});
    registerConfigVar("misc:disable_autoreload", Hyprlang::INT{0});
    registerConfigVar("misc:enable_swallow", Hyprlang::INT{0});
    registerConfigVar("misc:swallow_regex", {STRVAL_EMPTY});
    registerConfigVar("misc:swallow_exception_regex", {STRVAL_EMPTY});
    registerConfigVar("misc:focus_on_activate", Hyprlang::INT{0});
    registerConfigVar("misc:mouse_move_focuses_monitor", Hyprlang::INT{1});
    registerConfigVar("misc:allow_session_lock_restore", Hyprlang::INT{0});
    registerConfigVar("misc:session_lock_xray", Hyprlang::INT{0});
    registerConfigVar("misc:close_special_on_empty", Hyprlang::INT{1});
    registerConfigVar("misc:background_color", Hyprlang::INT{0xff111111});
    registerConfigVar("misc:new_window_takes_over_fullscreen", Hyprlang::INT{0});
    registerConfigVar("misc:exit_window_retains_fullscreen", Hyprlang::INT{0});
    registerConfigVar("misc:initial_workspace_tracking", Hyprlang::INT{1});
    registerConfigVar("misc:middle_click_paste", Hyprlang::INT{1});
    registerConfigVar("misc:render_unfocused_fps", Hyprlang::INT{15});
    registerConfigVar("misc:disable_xdg_env_checks", Hyprlang::INT{0});
    registerConfigVar("misc:disable_hyprland_qtutils_check", Hyprlang::INT{0});
    registerConfigVar("misc:lockdead_screen_delay", Hyprlang::INT{1000});
    registerConfigVar("misc:enable_anr_dialog", Hyprlang::INT{1});
    registerConfigVar("misc:anr_missed_pings", Hyprlang::INT{1});
    registerConfigVar("misc:screencopy_force_8b", Hyprlang::INT{1});
    registerConfigVar("misc:disable_scale_notification", Hyprlang::INT{0});

    registerConfigVar("group:insert_after_current", Hyprlang::INT{1});
    registerConfigVar("group:focus_removed_window", Hyprlang::INT{1});
    registerConfigVar("group:merge_groups_on_drag", Hyprlang::INT{1});
    registerConfigVar("group:merge_groups_on_groupbar", Hyprlang::INT{1});
    registerConfigVar("group:merge_floated_into_tiled_on_groupbar", Hyprlang::INT{0});
    registerConfigVar("group:auto_group", Hyprlang::INT{1});
    registerConfigVar("group:drag_into_group", Hyprlang::INT{1});
    registerConfigVar("group:group_on_movetoworkspace", Hyprlang::INT{0});
    registerConfigVar("group:groupbar:enabled", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:font_family", {STRVAL_EMPTY});
    registerConfigVar("group:groupbar:font_weight_active", Hyprlang::CConfigCustomValueType{&configHandleFontWeightSet, configHandleFontWeightDestroy, "normal"});
    registerConfigVar("group:groupbar:font_weight_inactive", Hyprlang::CConfigCustomValueType{&configHandleFontWeightSet, configHandleFontWeightDestroy, "normal"});
    registerConfigVar("group:groupbar:font_size", Hyprlang::INT{8});
    registerConfigVar("group:groupbar:gradients", Hyprlang::INT{0});
    registerConfigVar("group:groupbar:height", Hyprlang::INT{14});
    registerConfigVar("group:groupbar:indicator_gap", Hyprlang::INT{0});
    registerConfigVar("group:groupbar:indicator_height", Hyprlang::INT{3});
    registerConfigVar("group:groupbar:priority", Hyprlang::INT{3});
    registerConfigVar("group:groupbar:render_titles", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:scrolling", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:text_color", Hyprlang::INT{0xffffffff});
    registerConfigVar("group:groupbar:text_color_inactive", Hyprlang::INT{-1});
    registerConfigVar("group:groupbar:text_color_locked_active", Hyprlang::INT{-1});
    registerConfigVar("group:groupbar:text_color_locked_inactive", Hyprlang::INT{-1});
    registerConfigVar("group:groupbar:stacked", Hyprlang::INT{0});
    registerConfigVar("group:groupbar:rounding", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:rounding_power", {2.F});
    registerConfigVar("group:groupbar:gradient_rounding", Hyprlang::INT{2});
    registerConfigVar("group:groupbar:gradient_rounding_power", {2.F});
    registerConfigVar("group:groupbar:round_only_edges", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:gradient_round_only_edges", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:gaps_out", Hyprlang::INT{2});
    registerConfigVar("group:groupbar:gaps_in", Hyprlang::INT{2});
    registerConfigVar("group:groupbar:keep_upper_gap", Hyprlang::INT{1});
    registerConfigVar("group:groupbar:text_offset", Hyprlang::INT{0});

    registerConfigVar("debug:log_damage", Hyprlang::INT{0});
    registerConfigVar("debug:overlay", Hyprlang::INT{0});
    registerConfigVar("debug:damage_blink", Hyprlang::INT{0});
    registerConfigVar("debug:pass", Hyprlang::INT{0});
    registerConfigVar("debug:disable_logs", Hyprlang::INT{1});
    registerConfigVar("debug:disable_time", Hyprlang::INT{1});
    registerConfigVar("debug:enable_stdout_logs", Hyprlang::INT{0});
    registerConfigVar("debug:damage_tracking", {sc<Hyprlang::INT>(DAMAGE_TRACKING_FULL)});
    registerConfigVar("debug:manual_crash", Hyprlang::INT{0});
    registerConfigVar("debug:suppress_errors", Hyprlang::INT{0});
    registerConfigVar("debug:error_limit", Hyprlang::INT{5});
    registerConfigVar("debug:error_position", Hyprlang::INT{0});
    registerConfigVar("debug:disable_scale_checks", Hyprlang::INT{0});
    registerConfigVar("debug:colored_stdout_logs", Hyprlang::INT{1});
    registerConfigVar("debug:full_cm_proto", Hyprlang::INT{0});

    registerConfigVar("decoration:rounding", Hyprlang::INT{0});
    registerConfigVar("decoration:rounding_power", {2.F});
    registerConfigVar("decoration:blur:enabled", Hyprlang::INT{1});
    registerConfigVar("decoration:blur:size", Hyprlang::INT{8});
    registerConfigVar("decoration:blur:passes", Hyprlang::INT{1});
    registerConfigVar("decoration:blur:ignore_opacity", Hyprlang::INT{1});
    registerConfigVar("decoration:blur:new_optimizations", Hyprlang::INT{1});
    registerConfigVar("decoration:blur:xray", Hyprlang::INT{0});
    registerConfigVar("decoration:blur:contrast", {0.8916F});
    registerConfigVar("decoration:blur:brightness", {1.0F});
    registerConfigVar("decoration:blur:vibrancy", {0.1696F});
    registerConfigVar("decoration:blur:vibrancy_darkness", {0.0F});
    registerConfigVar("decoration:blur:noise", {0.0117F});
    registerConfigVar("decoration:blur:special", Hyprlang::INT{0});
    registerConfigVar("decoration:blur:popups", Hyprlang::INT{0});
    registerConfigVar("decoration:blur:popups_ignorealpha", {0.2F});
    registerConfigVar("decoration:blur:input_methods", Hyprlang::INT{0});
    registerConfigVar("decoration:blur:input_methods_ignorealpha", {0.2F});
    registerConfigVar("decoration:active_opacity", {1.F});
    registerConfigVar("decoration:inactive_opacity", {1.F});
    registerConfigVar("decoration:fullscreen_opacity", {1.F});
    registerConfigVar("decoration:shadow:enabled", Hyprlang::INT{1});
    registerConfigVar("decoration:shadow:range", Hyprlang::INT{4});
    registerConfigVar("decoration:shadow:render_power", Hyprlang::INT{3});
    registerConfigVar("decoration:shadow:ignore_window", Hyprlang::INT{1});
    registerConfigVar("decoration:shadow:offset", Hyprlang::VEC2{0, 0});
    registerConfigVar("decoration:shadow:scale", {1.f});
    registerConfigVar("decoration:shadow:sharp", Hyprlang::INT{0});
    registerConfigVar("decoration:shadow:color", Hyprlang::INT{0xee1a1a1a});
    registerConfigVar("decoration:shadow:color_inactive", Hyprlang::INT{-1});
    registerConfigVar("decoration:dim_inactive", Hyprlang::INT{0});
    registerConfigVar("decoration:dim_modal", Hyprlang::INT{1});
    registerConfigVar("decoration:dim_strength", {0.5f});
    registerConfigVar("decoration:dim_special", {0.2f});
    registerConfigVar("decoration:dim_around", {0.4f});
    registerConfigVar("decoration:screen_shader", {STRVAL_EMPTY});
    registerConfigVar("decoration:border_part_of_window", Hyprlang::INT{1});

    registerConfigVar("dwindle:pseudotile", Hyprlang::INT{0});
    registerConfigVar("dwindle:force_split", Hyprlang::INT{0});
    registerConfigVar("dwindle:permanent_direction_override", Hyprlang::INT{0});
    registerConfigVar("dwindle:preserve_split", Hyprlang::INT{0});
    registerConfigVar("dwindle:special_scale_factor", {1.f});
    registerConfigVar("dwindle:split_width_multiplier", {1.0f});
    registerConfigVar("dwindle:use_active_for_splits", Hyprlang::INT{1});
    registerConfigVar("dwindle:default_split_ratio", {1.f});
    registerConfigVar("dwindle:split_bias", Hyprlang::INT{0});
    registerConfigVar("dwindle:smart_split", Hyprlang::INT{0});
    registerConfigVar("dwindle:smart_resizing", Hyprlang::INT{1});
    registerConfigVar("dwindle:precise_mouse_move", Hyprlang::INT{0});
    registerConfigVar("dwindle:single_window_aspect_ratio", Hyprlang::VEC2{0, 0});
    registerConfigVar("dwindle:single_window_aspect_ratio_tolerance", {0.1f});

    registerConfigVar("master:special_scale_factor", {1.f});
    registerConfigVar("master:mfact", {0.55f});
    registerConfigVar("master:new_status", {"slave"});
    registerConfigVar("master:slave_count_for_center_master", Hyprlang::INT{2});
    registerConfigVar("master:center_master_fallback", {"left"});
    registerConfigVar("master:center_ignores_reserved", Hyprlang::INT{0});
    registerConfigVar("master:new_on_active", {"none"});
    registerConfigVar("master:new_on_top", Hyprlang::INT{0});
    registerConfigVar("master:orientation", {"left"});
    registerConfigVar("master:inherit_fullscreen", Hyprlang::INT{1});
    registerConfigVar("master:allow_small_split", Hyprlang::INT{0});
    registerConfigVar("master:smart_resizing", Hyprlang::INT{1});
    registerConfigVar("master:drop_at_cursor", Hyprlang::INT{1});
    registerConfigVar("master:always_keep_position", Hyprlang::INT{0});

    registerConfigVar("animations:enabled", Hyprlang::INT{1});
    registerConfigVar("animations:workspace_wraparound", Hyprlang::INT{0});

    registerConfigVar("input:follow_mouse", Hyprlang::INT{1});
    registerConfigVar("input:follow_mouse_threshold", Hyprlang::FLOAT{0});
    registerConfigVar("input:focus_on_close", Hyprlang::INT{0});
    registerConfigVar("input:mouse_refocus", Hyprlang::INT{1});
    registerConfigVar("input:special_fallthrough", Hyprlang::INT{0});
    registerConfigVar("input:off_window_axis_events", Hyprlang::INT{1});
    registerConfigVar("input:sensitivity", {0.f});
    registerConfigVar("input:accel_profile", {STRVAL_EMPTY});
    registerConfigVar("input:kb_file", {STRVAL_EMPTY});
    registerConfigVar("input:kb_layout", {"us"});
    registerConfigVar("input:kb_variant", {STRVAL_EMPTY});
    registerConfigVar("input:kb_options", {STRVAL_EMPTY});
    registerConfigVar("input:kb_rules", {STRVAL_EMPTY});
    registerConfigVar("input:kb_model", {STRVAL_EMPTY});
    registerConfigVar("input:repeat_rate", Hyprlang::INT{25});
    registerConfigVar("input:repeat_delay", Hyprlang::INT{600});
    registerConfigVar("input:natural_scroll", Hyprlang::INT{0});
    registerConfigVar("input:numlock_by_default", Hyprlang::INT{0});
    registerConfigVar("input:resolve_binds_by_sym", Hyprlang::INT{0});
    registerConfigVar("input:force_no_accel", Hyprlang::INT{0});
    registerConfigVar("input:float_switch_override_focus", Hyprlang::INT{1});
    registerConfigVar("input:left_handed", Hyprlang::INT{0});
    registerConfigVar("input:scroll_method", {STRVAL_EMPTY});
    registerConfigVar("input:scroll_button", Hyprlang::INT{0});
    registerConfigVar("input:scroll_button_lock", Hyprlang::INT{0});
    registerConfigVar("input:scroll_factor", {1.f});
    registerConfigVar("input:scroll_points", {STRVAL_EMPTY});
    registerConfigVar("input:emulate_discrete_scroll", Hyprlang::INT{1});
    registerConfigVar("input:touchpad:natural_scroll", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:disable_while_typing", Hyprlang::INT{1});
    registerConfigVar("input:touchpad:clickfinger_behavior", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:tap_button_map", {STRVAL_EMPTY});
    registerConfigVar("input:touchpad:middle_button_emulation", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:tap-to-click", Hyprlang::INT{1});
    registerConfigVar("input:touchpad:tap-and-drag", Hyprlang::INT{1});
    registerConfigVar("input:touchpad:drag_lock", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:scroll_factor", {1.f});
    registerConfigVar("input:touchpad:flip_x", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:flip_y", Hyprlang::INT{0});
    registerConfigVar("input:touchpad:drag_3fg", Hyprlang::INT{0});
    registerConfigVar("input:touchdevice:transform", Hyprlang::INT{-1});
    registerConfigVar("input:touchdevice:output", {"[[Auto]]"});
    registerConfigVar("input:touchdevice:enabled", Hyprlang::INT{1});
    registerConfigVar("input:virtualkeyboard:share_states", Hyprlang::INT{2});
    registerConfigVar("input:virtualkeyboard:release_pressed_on_close", Hyprlang::INT{0});
    registerConfigVar("input:tablet:transform", Hyprlang::INT{0});
    registerConfigVar("input:tablet:output", {STRVAL_EMPTY});
    registerConfigVar("input:tablet:region_position", Hyprlang::VEC2{0, 0});
    registerConfigVar("input:tablet:absolute_region_position", Hyprlang::INT{0});
    registerConfigVar("input:tablet:region_size", Hyprlang::VEC2{0, 0});
    registerConfigVar("input:tablet:relative_input", Hyprlang::INT{0});
    registerConfigVar("input:tablet:left_handed", Hyprlang::INT{0});
    registerConfigVar("input:tablet:active_area_position", Hyprlang::VEC2{0, 0});
    registerConfigVar("input:tablet:active_area_size", Hyprlang::VEC2{0, 0});

    registerConfigVar("binds:pass_mouse_when_bound", Hyprlang::INT{0});
    registerConfigVar("binds:scroll_event_delay", Hyprlang::INT{300});
    registerConfigVar("binds:workspace_back_and_forth", Hyprlang::INT{0});
    registerConfigVar("binds:hide_special_on_workspace_change", Hyprlang::INT{0});
    registerConfigVar("binds:allow_workspace_cycles", Hyprlang::INT{0});
    registerConfigVar("binds:workspace_center_on", Hyprlang::INT{1});
    registerConfigVar("binds:focus_preferred_method", Hyprlang::INT{0});
    registerConfigVar("binds:ignore_group_lock", Hyprlang::INT{0});
    registerConfigVar("binds:movefocus_cycles_fullscreen", Hyprlang::INT{0});
    registerConfigVar("binds:movefocus_cycles_groupfirst", Hyprlang::INT{0});
    registerConfigVar("binds:disable_keybind_grabbing", Hyprlang::INT{0});
    registerConfigVar("binds:window_direction_monitor_fallback", Hyprlang::INT{1});
    registerConfigVar("binds:allow_pin_fullscreen", Hyprlang::INT{0});
    registerConfigVar("binds:drag_threshold", Hyprlang::INT{0});

    registerConfigVar("gestures:workspace_swipe_distance", Hyprlang::INT{300});
    registerConfigVar("gestures:workspace_swipe_invert", Hyprlang::INT{1});
    registerConfigVar("gestures:workspace_swipe_min_speed_to_force", Hyprlang::INT{30});
    registerConfigVar("gestures:workspace_swipe_cancel_ratio", {0.5f});
    registerConfigVar("gestures:workspace_swipe_create_new", Hyprlang::INT{1});
    registerConfigVar("gestures:workspace_swipe_direction_lock", Hyprlang::INT{1});
    registerConfigVar("gestures:workspace_swipe_direction_lock_threshold", Hyprlang::INT{10});
    registerConfigVar("gestures:workspace_swipe_forever", Hyprlang::INT{0});
    registerConfigVar("gestures:workspace_swipe_use_r", Hyprlang::INT{0});
    registerConfigVar("gestures:workspace_swipe_touch", Hyprlang::INT{0});
    registerConfigVar("gestures:workspace_swipe_touch_invert", Hyprlang::INT{0});
    registerConfigVar("gestures:close_max_timeout", Hyprlang::INT{1000});

    registerConfigVar("xwayland:enabled", Hyprlang::INT{1});
    registerConfigVar("xwayland:use_nearest_neighbor", Hyprlang::INT{1});
    registerConfigVar("xwayland:force_zero_scaling", Hyprlang::INT{0});
    registerConfigVar("xwayland:create_abstract_socket", Hyprlang::INT{0});

    registerConfigVar("opengl:nvidia_anti_flicker", Hyprlang::INT{1});

    registerConfigVar("cursor:invisible", Hyprlang::INT{0});
    registerConfigVar("cursor:no_hardware_cursors", Hyprlang::INT{2});
    registerConfigVar("cursor:no_break_fs_vrr", Hyprlang::INT{2});
    registerConfigVar("cursor:min_refresh_rate", Hyprlang::INT{24});
    registerConfigVar("cursor:hotspot_padding", Hyprlang::INT{0});
    registerConfigVar("cursor:inactive_timeout", {0.f});
    registerConfigVar("cursor:no_warps", Hyprlang::INT{0});
    registerConfigVar("cursor:persistent_warps", Hyprlang::INT{0});
    registerConfigVar("cursor:warp_on_change_workspace", Hyprlang::INT{0});
    registerConfigVar("cursor:warp_on_toggle_special", Hyprlang::INT{0});
    registerConfigVar("cursor:default_monitor", {STRVAL_EMPTY});
    registerConfigVar("cursor:zoom_factor", {1.f});
    registerConfigVar("cursor:zoom_rigid", Hyprlang::INT{0});
    registerConfigVar("cursor:enable_hyprcursor", Hyprlang::INT{1});
    registerConfigVar("cursor:sync_gsettings_theme", Hyprlang::INT{1});
    registerConfigVar("cursor:hide_on_key_press", Hyprlang::INT{0});
    registerConfigVar("cursor:hide_on_touch", Hyprlang::INT{1});
    registerConfigVar("cursor:use_cpu_buffer", Hyprlang::INT{2});
    registerConfigVar("cursor:warp_back_after_non_mouse_input", Hyprlang::INT{0});

    registerConfigVar("autogenerated", Hyprlang::INT{0});

    registerConfigVar("group:col.border_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ffff00"});
    registerConfigVar("group:col.border_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66777700"});
    registerConfigVar("group:col.border_locked_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ff5500"});
    registerConfigVar("group:col.border_locked_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66775500"});

    registerConfigVar("group:groupbar:col.active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ffff00"});
    registerConfigVar("group:groupbar:col.inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66777700"});
    registerConfigVar("group:groupbar:col.locked_active", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66ff5500"});
    registerConfigVar("group:groupbar:col.locked_inactive", Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy, "0x66775500"});

    registerConfigVar("render:direct_scanout", Hyprlang::INT{0});
    registerConfigVar("render:expand_undersized_textures", Hyprlang::INT{1});
    registerConfigVar("render:xp_mode", Hyprlang::INT{0});
    registerConfigVar("render:ctm_animation", Hyprlang::INT{2});
    registerConfigVar("render:cm_fs_passthrough", Hyprlang::INT{2});
    registerConfigVar("render:cm_enabled", Hyprlang::INT{1});
    registerConfigVar("render:send_content_type", Hyprlang::INT{1});
    registerConfigVar("render:cm_auto_hdr", Hyprlang::INT{1});
    registerConfigVar("render:new_render_scheduling", Hyprlang::INT{0});

    registerConfigVar("ecosystem:no_update_news", Hyprlang::INT{0});
    registerConfigVar("ecosystem:no_donation_nag", Hyprlang::INT{0});
    registerConfigVar("ecosystem:enforce_permissions", Hyprlang::INT{0});

    registerConfigVar("experimental:xx_color_management_v4", Hyprlang::INT{0});

    // devices
    m_config->addSpecialCategory("device", {"name"});
    m_config->addSpecialConfigValue("device", "sensitivity", {0.F});
    m_config->addSpecialConfigValue("device", "accel_profile", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "kb_file", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "kb_layout", {"us"});
    m_config->addSpecialConfigValue("device", "kb_variant", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "kb_options", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "kb_rules", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "kb_model", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "repeat_rate", Hyprlang::INT{25});
    m_config->addSpecialConfigValue("device", "repeat_delay", Hyprlang::INT{600});
    m_config->addSpecialConfigValue("device", "natural_scroll", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "tap_button_map", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "numlock_by_default", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "resolve_binds_by_sym", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "disable_while_typing", Hyprlang::INT{1});
    m_config->addSpecialConfigValue("device", "clickfinger_behavior", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "middle_button_emulation", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "tap-to-click", Hyprlang::INT{1});
    m_config->addSpecialConfigValue("device", "tap-and-drag", Hyprlang::INT{1});
    m_config->addSpecialConfigValue("device", "drag_lock", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "left_handed", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "scroll_method", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "scroll_button", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "scroll_button_lock", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("device", "scroll_points", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "scroll_factor", Hyprlang::FLOAT{-1});
    m_config->addSpecialConfigValue("device", "transform", Hyprlang::INT{-1});
    m_config->addSpecialConfigValue("device", "output", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "enabled", Hyprlang::INT{1});                  // only for mice, touchpads, and touchdevices
    m_config->addSpecialConfigValue("device", "region_position", Hyprlang::VEC2{0, 0});      // only for tablets
    m_config->addSpecialConfigValue("device", "absolute_region_position", Hyprlang::INT{0}); // only for tablets
    m_config->addSpecialConfigValue("device", "region_size", Hyprlang::VEC2{0, 0});          // only for tablets
    m_config->addSpecialConfigValue("device", "relative_input", Hyprlang::INT{0});           // only for tablets
    m_config->addSpecialConfigValue("device", "active_area_position", Hyprlang::VEC2{0, 0}); // only for tablets
    m_config->addSpecialConfigValue("device", "active_area_size", Hyprlang::VEC2{0, 0});     // only for tablets
    m_config->addSpecialConfigValue("device", "flip_x", Hyprlang::INT{0});                   // only for touchpads
    m_config->addSpecialConfigValue("device", "flip_y", Hyprlang::INT{0});                   // only for touchpads
    m_config->addSpecialConfigValue("device", "drag_3fg", Hyprlang::INT{0});                 // only for touchpads
    m_config->addSpecialConfigValue("device", "keybinds", Hyprlang::INT{1});                 // enable/disable keybinds
    m_config->addSpecialConfigValue("device", "share_states", Hyprlang::INT{0});             // only for virtualkeyboards
    m_config->addSpecialConfigValue("device", "release_pressed_on_close", Hyprlang::INT{0}); // only for virtualkeyboards

    m_config->addSpecialCategory("monitorv2", {.key = "output"});
    m_config->addSpecialConfigValue("monitorv2", "disabled", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("monitorv2", "mode", {"preferred"});
    m_config->addSpecialConfigValue("monitorv2", "position", {"auto"});
    m_config->addSpecialConfigValue("monitorv2", "scale", {"auto"});
    m_config->addSpecialConfigValue("monitorv2", "addreserved", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("monitorv2", "mirror", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("monitorv2", "bitdepth", {STRVAL_EMPTY}); // TODO use correct type
    m_config->addSpecialConfigValue("monitorv2", "cm", {"auto"});
    m_config->addSpecialConfigValue("monitorv2", "sdrbrightness", Hyprlang::FLOAT{1.0});
    m_config->addSpecialConfigValue("monitorv2", "sdrsaturation", Hyprlang::FLOAT{1.0});
    m_config->addSpecialConfigValue("monitorv2", "vrr", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("monitorv2", "transform", {STRVAL_EMPTY}); // TODO use correct type
    m_config->addSpecialConfigValue("monitorv2", "supports_wide_color", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("monitorv2", "supports_hdr", Hyprlang::INT{0});
    m_config->addSpecialConfigValue("monitorv2", "sdr_min_luminance", Hyprlang::FLOAT{0.2});
    m_config->addSpecialConfigValue("monitorv2", "sdr_max_luminance", Hyprlang::INT{80});
    m_config->addSpecialConfigValue("monitorv2", "min_luminance", Hyprlang::FLOAT{-1.0});
    m_config->addSpecialConfigValue("monitorv2", "max_luminance", Hyprlang::INT{-1});
    m_config->addSpecialConfigValue("monitorv2", "max_avg_luminance", Hyprlang::INT{-1});

    // keywords
    m_config->registerHandler(&::handleExec, "exec", {false});
    m_config->registerHandler(&::handleRawExec, "execr", {false});
    m_config->registerHandler(&::handleExecOnce, "exec-once", {false});
    m_config->registerHandler(&::handleExecRawOnce, "execr-once", {false});
    m_config->registerHandler(&::handleExecShutdown, "exec-shutdown", {false});
    m_config->registerHandler(&::handleMonitor, "monitor", {false});
    m_config->registerHandler(&::handleBind, "bind", {true});
    m_config->registerHandler(&::handleUnbind, "unbind", {false});
    m_config->registerHandler(&::handleWorkspaceRules, "workspace", {false});
    m_config->registerHandler(&::handleWindowRule, "windowrule", {false});
    m_config->registerHandler(&::handleLayerRule, "layerrule", {false});
    m_config->registerHandler(&::handleWindowRuleV2, "windowrulev2", {false});
    m_config->registerHandler(&::handleBezier, "bezier", {false});
    m_config->registerHandler(&::handleAnimation, "animation", {false});
    m_config->registerHandler(&::handleSource, "source", {false});
    m_config->registerHandler(&::handleSubmap, "submap", {false});
    m_config->registerHandler(&::handleBlurLS, "blurls", {false});
    m_config->registerHandler(&::handlePlugin, "plugin", {false});
    m_config->registerHandler(&::handlePermission, "permission", {false});
    m_config->registerHandler(&::handleGesture, "gesture", {false});
    m_config->registerHandler(&::handleEnv, "env", {true});

    // pluginza
    m_config->addSpecialCategory("plugin", {nullptr, true});

    m_config->commence();

    resetHLConfig();

    if (CONFIG_OPTIONS.size() != m_configValueNumber - 1 /* autogenerated is special */)
        Debug::log(LOG, "Warning: config descriptions have {} entries, but there are {} config values. This should fail tests!!", CONFIG_OPTIONS.size(), m_configValueNumber);

    if (!g_pCompositor->m_onlyConfigVerification) {
        Debug::log(
            INFO,
            "!!!!HEY YOU, YES YOU!!!!: further logs to stdout / logfile are disabled by default. BEFORE SENDING THIS LOG, ENABLE THEM. Use debug:disable_logs = false to do so: "
            "https://wiki.hypr.land/Configuring/Variables/#debug");
    }

    Debug::m_disableLogs = rc<int64_t* const*>(m_config->getConfigValuePtr("debug:disable_logs")->getDataStaticPtr());
    Debug::m_disableTime = rc<int64_t* const*>(m_config->getConfigValuePtr("debug:disable_time")->getDataStaticPtr());

    if (g_pEventLoopManager && ERR.has_value())
        g_pEventLoopManager->doLater([ERR] { g_pHyprError->queueCreate(ERR.value(), CHyprColor{1.0, 0.1, 0.1, 1.0}); });
}

std::optional<std::string> CConfigManager::generateConfig(std::string configPath) {
    std::string parentPath = std::filesystem::path(configPath).parent_path();

    if (!parentPath.empty()) {
        std::error_code ec;
        bool            created = std::filesystem::create_directories(parentPath, ec);
        if (ec) {
            Debug::log(ERR, "Couldn't create config home directory ({}): {}", ec.message(), parentPath);
            return "Config could not be generated.";
        }
        if (created)
            Debug::log(WARN, "Creating config home directory");
    }

    Debug::log(WARN, "No config file found; attempting to generate.");
    std::ofstream ofs;
    ofs.open(configPath, std::ios::trunc);
    ofs << AUTOGENERATED_PREFIX << EXAMPLE_CONFIG;
    ofs.close();

    if (ofs.fail())
        return "Config could not be generated.";

    return configPath;
}

std::string CConfigManager::getMainConfigPath() {
    static std::string CONFIG_PATH = [this]() -> std::string {
        if (!g_pCompositor->m_explicitConfigPath.empty())
            return g_pCompositor->m_explicitConfigPath;

        if (const auto CFG_ENV = getenv("HYPRLAND_CONFIG"); CFG_ENV)
            return CFG_ENV;

        const auto PATHS = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland");
        if (PATHS.first.has_value()) {
            return PATHS.first.value();
        } else if (PATHS.second.has_value()) {
            const auto CONFIGPATH = Hyprutils::Path::fullConfigPath(PATHS.second.value(), ISDEBUG ? "hyprlandd" : "hyprland");
            return generateConfig(CONFIGPATH).value();
        } else
            throw std::runtime_error("Neither HOME nor XDG_CONFIG_HOME are set in the environment. Could not find config in XDG_CONFIG_DIRS or /etc/xdg.");
    }();

    return CONFIG_PATH;
}

std::optional<std::string> CConfigManager::verifyConfigExists() {
    std::string mainConfigPath = getMainConfigPath();

    if (!std::filesystem::exists(mainConfigPath))
        return "broken config dir?";

    return {};
}

std::string CConfigManager::getConfigString() {
    std::string configString;
    std::string currFileContent;

    for (const auto& path : m_configPaths) {
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
    return m_configErrors;
}

void CConfigManager::reload() {
    EMIT_HOOK_EVENT("preConfigReload", nullptr);
    setDefaultAnimationVars();
    resetHLConfig();
    m_configCurrentPath                   = getMainConfigPath();
    const auto ERR                        = m_config->parse();
    const auto monitorError               = handleMonitorv2();
    m_lastConfigVerificationWasSuccessful = !ERR.error && !monitorError.error;
    postConfigReload(ERR.error || !monitorError.error ? ERR : monitorError);
}

std::string CConfigManager::verify() {
    setDefaultAnimationVars();
    resetHLConfig();
    m_configCurrentPath                   = getMainConfigPath();
    const auto ERR                        = m_config->parse();
    m_lastConfigVerificationWasSuccessful = !ERR.error;
    if (ERR.error)
        return ERR.getError();
    return "config ok";
}

void CConfigManager::setDefaultAnimationVars() {
    m_animationTree.createNode("__internal_fadeCTM");
    m_animationTree.createNode("global");

    // global
    m_animationTree.createNode("windows", "global");
    m_animationTree.createNode("layers", "global");
    m_animationTree.createNode("fade", "global");
    m_animationTree.createNode("border", "global");
    m_animationTree.createNode("borderangle", "global");
    m_animationTree.createNode("workspaces", "global");
    m_animationTree.createNode("zoomFactor", "global");
    m_animationTree.createNode("monitorAdded", "global");

    // layer
    m_animationTree.createNode("layersIn", "layers");
    m_animationTree.createNode("layersOut", "layers");

    // windows
    m_animationTree.createNode("windowsIn", "windows");
    m_animationTree.createNode("windowsOut", "windows");
    m_animationTree.createNode("windowsMove", "windows");

    // fade
    m_animationTree.createNode("fadeIn", "fade");
    m_animationTree.createNode("fadeOut", "fade");
    m_animationTree.createNode("fadeSwitch", "fade");
    m_animationTree.createNode("fadeShadow", "fade");
    m_animationTree.createNode("fadeDim", "fade");
    m_animationTree.createNode("fadeLayers", "fade");
    m_animationTree.createNode("fadeLayersIn", "fadeLayers");
    m_animationTree.createNode("fadeLayersOut", "fadeLayers");
    m_animationTree.createNode("fadePopups", "fade");
    m_animationTree.createNode("fadePopupsIn", "fadePopups");
    m_animationTree.createNode("fadePopupsOut", "fadePopups");
    m_animationTree.createNode("fadeDpms", "fade");

    // workspaces
    m_animationTree.createNode("workspacesIn", "workspaces");
    m_animationTree.createNode("workspacesOut", "workspaces");
    m_animationTree.createNode("specialWorkspace", "workspaces");
    m_animationTree.createNode("specialWorkspaceIn", "specialWorkspace");
    m_animationTree.createNode("specialWorkspaceOut", "specialWorkspace");

    // init the root nodes
    m_animationTree.setConfigForNode("global", 1, 8.f, "default");
    m_animationTree.setConfigForNode("__internal_fadeCTM", 1, 5.f, "linear");
    m_animationTree.setConfigForNode("borderangle", 0, 1, "default");
}

std::optional<std::string> CConfigManager::resetHLConfig() {
    m_monitorRules.clear();
    m_windowRules.clear();
    g_pKeybindManager->clearKeybinds();
    g_pAnimationManager->removeAllBeziers();
    g_pAnimationManager->addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));
    g_pTrackpadGestures->clearGestures();

    m_mAdditionalReservedAreas.clear();
    m_blurLSNamespaces.clear();
    m_workspaceRules.clear();
    setDefaultAnimationVars(); // reset anims
    m_declaredPlugins.clear();
    m_layerRules.clear();
    m_failedPluginConfigValues.clear();
    m_finalExecRequests.clear();

    // paths
    m_configPaths.clear();
    std::string mainConfigPath = getMainConfigPath();
    Debug::log(LOG, "Using config: {}", mainConfigPath);
    m_configPaths.emplace_back(mainConfigPath);

    const auto RET = verifyConfigExists();

    return RET;
}

void CConfigManager::updateWatcher() {
    static const auto PDISABLEAUTORELOAD = CConfigValue<Hyprlang::INT>("misc:disable_autoreload");
    g_pConfigWatcher->setWatchList(*PDISABLEAUTORELOAD ? std::vector<std::string>{} : m_configPaths);
}

std::optional<std::string> CConfigManager::handleMonitorv2(const std::string& output) {
    auto parser = CMonitorRuleParser(output);
    auto VAL    = m_config->getSpecialConfigValuePtr("monitorv2", "disabled", output.c_str());
    if (VAL && VAL->m_bSetByUser && std::any_cast<Hyprlang::INT>(VAL->getValue()))
        parser.setDisabled();
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "mode", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseMode(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "position", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parsePosition(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "scale", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseScale(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "addreserved", output.c_str());
    if (VAL && VAL->m_bSetByUser) {
        const auto ARGS = CVarList(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
        parser.setReserved({.top = std::stoi(ARGS[0]), .bottom = std::stoi(ARGS[1]), .left = std::stoi(ARGS[2]), .right = std::stoi(ARGS[3])});
    }
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "mirror", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.setMirror(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "bitdepth", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseBitdepth(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "cm", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseCM(std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdrbrightness", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().sdrBrightness = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdrsaturation", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().sdrSaturation = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "vrr", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().vrr = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "transform", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseTransform(std::any_cast<Hyprlang::STRING>(VAL->getValue()));

    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "supports_wide_color", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().supportsWideColor = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "supports_hdr", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().supportsHDR = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdr_min_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().sdrMinLuminance = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdr_max_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().sdrMaxLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());

    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "min_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().minLuminance = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "max_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().maxLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "max_avg_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().maxAvgLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());

    auto newrule = parser.rule();

    std::erase_if(m_monitorRules, [&](const auto& other) { return other.name == newrule.name; });

    m_monitorRules.push_back(newrule);

    return parser.getError();
}

Hyprlang::CParseResult CConfigManager::handleMonitorv2() {
    Hyprlang::CParseResult result;
    for (const auto& output : m_config->listKeysForSpecialCategory("monitorv2")) {
        const auto error = handleMonitorv2(output);
        if (error.has_value()) {
            result.setError(error.value().c_str());
            return result;
        }
    }
    return result;
}

void CConfigManager::postConfigReload(const Hyprlang::CParseResult& result) {
    updateWatcher();

    for (auto const& w : g_pCompositor->m_windows) {
        w->uncacheWindowDecos();
    }

    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    for (auto const& m : g_pCompositor->m_monitors) {
        *(m->m_cursorZoom) = *PZOOMFACTOR;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->m_id);
    }

    // Update the keyboard layout to the cfg'd one if this is not the first launch
    if (!m_isFirstLaunch) {
        g_pInputManager->setKeyboardLayout();
        g_pInputManager->setPointerConfigs();
        g_pInputManager->setTouchDeviceConfigs();
        g_pInputManager->setTabletConfigs();

        g_pHyprOpenGL->m_reloadScreenShader = true;
    }

    // parseError will be displayed next frame

    if (result.error)
        m_configErrors = result.getError();
    else
        m_configErrors = "";

    if (result.error && !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:suppress_errors")))
        g_pHyprError->queueCreate(result.getError(), CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
    else if (std::any_cast<Hyprlang::INT>(m_config->getConfigValue("autogenerated")) == 1)
        g_pHyprError->queueCreate(
            "Warning: You're using an autogenerated config! Edit the config file to get rid of this message. (config file: " + getMainConfigPath() +
                " )\nSUPER+Q -> kitty (if it doesn't launch, make sure it's installed or choose a different terminal in the config)\nSUPER+M -> exit Hyprland",
            CHyprColor(1.0, 1.0, 70.0 / 255.0, 1.0));
    else
        g_pHyprError->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    // ignore if nomonitorreload is set
    if (!m_isFirstLaunch && !m_noMonitorReload) {
        // check
        performMonitorReload();
        ensureMonitorStatus();
        ensureVRR();
    }

#ifndef NO_XWAYLAND
    const auto PENABLEXWAYLAND     = std::any_cast<Hyprlang::INT>(m_config->getConfigValue("xwayland:enabled"));
    g_pCompositor->m_wantsXwayland = PENABLEXWAYLAND;
    // enable/disable xwayland usage
    if (!m_isFirstLaunch &&
        g_pXWayland /* XWayland has to be initialized by CCompositor::initManagers for this to make sense, and it doesn't have to be (e.g. very early plugin load) */) {
        bool prevEnabledXwayland = g_pXWayland->enabled();
        if (g_pCompositor->m_wantsXwayland != prevEnabledXwayland)
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_wantsXwayland);
    } else
        g_pCompositor->m_wantsXwayland = PENABLEXWAYLAND;
#endif

    if (!m_isFirstLaunch && !g_pCompositor->m_unsafeState)
        refreshGroupBarGradients();

    // Updates dynamic window and workspace rules
    for (auto const& w : g_pCompositor->getWorkspaces()) {
        if (w->inert())
            continue;
        w->updateWindows();
        w->updateWindowData();
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // update layout
    g_pLayoutManager->switchToLayout(std::any_cast<Hyprlang::STRING>(m_config->getConfigValue("general:layout")));

    // manual crash
    if (std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash")) && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CHyprColor(0), 5000,
                                                    ICON_INFO);
    } else if (m_manualCrashInitiated && !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash"))) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    Debug::m_disableStdout = !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:enable_stdout_logs"));
    if (Debug::m_disableStdout && m_isFirstLaunch)
        Debug::log(LOG, "Disabling stdout logs! Check the log for further logs.");

    Debug::m_coloredLogs = rc<int64_t* const*>(m_config->getConfigValuePtr("debug:colored_stdout_logs")->getDataStaticPtr());

    for (auto const& m : g_pCompositor->m_monitors) {
        // mark blur dirty
        g_pHyprOpenGL->markBlurDirtyForMonitor(m);

        g_pCompositor->scheduleFrameForMonitor(m);

        // Force the compositor to fully re-render all monitors
        m->m_forceFullFrames = 2;

        // also force mirrors, as the aspect ratio could've changed
        for (auto const& mirror : m->m_mirrors)
            mirror->m_forceFullFrames = 3;
    }

    // Reset no monitor reload
    m_noMonitorReload = false;

    // update plugins
    handlePluginLoads();

    // update persistent workspaces
    if (!m_isFirstLaunch)
        ensurePersistentWorkspacesPresent();

    EMIT_HOOK_EVENT("configReloaded", nullptr);
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::init() {

    g_pConfigWatcher->setOnChange([this](const CConfigWatcher::SConfigWatchEvent& e) {
        Debug::log(LOG, "CConfigManager: file {} modified, reloading", e.file);
        reload();
    });

    const std::string CONFIGPATH = getMainConfigPath();
    reload();

    m_isFirstLaunch = false;
}

std::string CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE) {
    const auto RET = m_config->parseDynamic(COMMAND.c_str(), VALUE.c_str());

    // invalidate layouts if they changed
    if (COMMAND == "monitor" || COMMAND.contains("gaps_") || COMMAND.starts_with("dwindle:") || COMMAND.starts_with("master:")) {
        for (auto const& m : g_pCompositor->m_monitors)
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->m_id);
    }

    // Update window border colors
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // manual crash
    if (std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash")) && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        if (g_pHyprNotificationOverlay) {
            g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CHyprColor(0), 5000,
                                                        ICON_INFO);
        }
    } else if (m_manualCrashInitiated && !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash"))) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    return RET.error ? RET.getError() : "";
}

Hyprlang::CConfigValue* CConfigManager::getConfigValueSafeDevice(const std::string& dev, const std::string& val, const std::string& fallback) {

    const auto VAL = m_config->getSpecialConfigValuePtr("device", val.c_str(), dev.c_str());

    if ((!VAL || !VAL->m_bSetByUser) && !fallback.empty())
        return m_config->getConfigValuePtr(fallback.c_str());

    return VAL;
}

bool CConfigManager::deviceConfigExplicitlySet(const std::string& dev, const std::string& val) {
    const auto VAL = m_config->getSpecialConfigValuePtr("device", val.c_str(), dev.c_str());

    return VAL && VAL->m_bSetByUser;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& v, const std::string& fallback) {
    return std::any_cast<Hyprlang::INT>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& v, const std::string& fallback) {
    return std::any_cast<Hyprlang::FLOAT>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& v, const std::string& fallback) {
    auto vec = std::any_cast<Hyprlang::VEC2>(getConfigValueSafeDevice(dev, v, fallback)->getValue());
    return {vec.x, vec.y};
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& v, const std::string& fallback) {
    auto VAL = std::string{std::any_cast<Hyprlang::STRING>(getConfigValueSafeDevice(dev, v, fallback)->getValue())};

    if (VAL == STRVAL_EMPTY)
        return "";

    return VAL;
}

SMonitorRule CConfigManager::getMonitorRuleFor(const PHLMONITOR PMONITOR) {
    auto applyWlrOutputConfig = [PMONITOR](SMonitorRule rule) -> SMonitorRule {
        const auto CONFIG = PROTO::outputManagement->getOutputStateFor(PMONITOR);

        if (!CONFIG)
            return rule;

        Debug::log(LOG, "CConfigManager::getMonitorRuleFor: found a wlr_output_manager override for {}", PMONITOR->m_name);

        Debug::log(LOG, " > overriding enabled: {} -> {}", !rule.disabled, !CONFIG->enabled);
        rule.disabled = !CONFIG->enabled;

        if ((CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_MODE) || (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_CUSTOM_MODE)) {
            Debug::log(LOG, " > overriding mode: {:.0f}x{:.0f}@{:.2f}Hz -> {:.0f}x{:.0f}@{:.2f}Hz", rule.resolution.x, rule.resolution.y, rule.refreshRate, CONFIG->resolution.x,
                       CONFIG->resolution.y, CONFIG->refresh / 1000.F);
            rule.resolution  = CONFIG->resolution;
            rule.refreshRate = CONFIG->refresh / 1000.F;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_POSITION) {
            Debug::log(LOG, " > overriding offset: {:.0f}, {:.0f} -> {:.0f}, {:.0f}", rule.offset.x, rule.offset.y, CONFIG->position.x, CONFIG->position.y);
            rule.offset = CONFIG->position;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_TRANSFORM) {
            Debug::log(LOG, " > overriding transform: {} -> {}", sc<uint8_t>(rule.transform), sc<uint8_t>(CONFIG->transform));
            rule.transform = CONFIG->transform;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_SCALE) {
            Debug::log(LOG, " > overriding scale: {} -> {}", sc<uint8_t>(rule.scale), sc<uint8_t>(CONFIG->scale));
            rule.scale = CONFIG->scale;
        }

        if (CONFIG->committedProperties & OUTPUT_HEAD_COMMITTED_ADAPTIVE_SYNC) {
            Debug::log(LOG, " > overriding vrr: {} -> {}", rule.vrr.value_or(0), CONFIG->adaptiveSync);
            rule.vrr = sc<int>(CONFIG->adaptiveSync);
        }

        return rule;
    };

    for (auto const& r : m_monitorRules | std::views::reverse) {
        if (PMONITOR->matchesStaticSelector(r.name)) {
            return applyWlrOutputConfig(r);
        }
    }

    Debug::log(WARN, "No rule found for {}, trying to use the first.", PMONITOR->m_name);

    for (auto const& r : m_monitorRules) {
        if (r.name.empty()) {
            return applyWlrOutputConfig(r);
        }
    }

    Debug::log(WARN, "No rules configured. Using the default hardcoded one.");

    return applyWlrOutputConfig(SMonitorRule{.autoDir    = eAutoDirs::DIR_AUTO_RIGHT,
                                             .name       = "",
                                             .resolution = Vector2D(0, 0),
                                             .offset     = Vector2D(-INT32_MAX, -INT32_MAX),
                                             .scale      = -1}); // 0, 0 is preferred and -1, -1 is auto
}

SWorkspaceRule CConfigManager::getWorkspaceRuleFor(PHLWORKSPACE pWorkspace) {
    SWorkspaceRule mergedRule{};
    for (auto const& rule : m_workspaceRules) {
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
    if (rule2.floatGaps)
        mergedRule.floatGaps = rule2.floatGaps;
    if (rule2.borderSize.has_value())
        mergedRule.borderSize = rule2.borderSize;
    if (rule2.noBorder.has_value())
        mergedRule.noBorder = rule2.noBorder;
    if (rule2.noRounding.has_value())
        mergedRule.noRounding = rule2.noRounding;
    if (rule2.decorate.has_value())
        mergedRule.decorate = rule2.decorate;
    if (rule2.noShadow.has_value())
        mergedRule.noShadow = rule2.noShadow;
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

std::vector<SP<CWindowRule>> CConfigManager::getMatchingRules(PHLWINDOW pWindow, bool dynamic, bool shadowExec) {
    if (!valid(pWindow))
        return std::vector<SP<CWindowRule>>();

    // if the window is unmapped, don't process exec rules yet.
    shadowExec = shadowExec || !pWindow->m_isMapped;

    std::vector<SP<CWindowRule>> returns;

    Debug::log(LOG, "Searching for matching rules for {} (title: {})", pWindow->m_class, pWindow->m_title);

    // since some rules will be applied later, we need to store some flags
    bool hasFloating   = pWindow->m_isFloating;
    bool hasFullscreen = pWindow->isFullscreen();
    bool isGrouped     = pWindow->m_groupData.pNextWindow;

    // local tags for dynamic tag rule match
    auto tags = pWindow->m_tags;

    for (auto const& rule : m_windowRules) {
        // check if we have a matching rule
        if (!rule->m_v2) {
            try {
                if (rule->m_value.starts_with("tag:") && !tags.isTagged(rule->m_value.substr(4)))
                    continue;

                if (rule->m_value.starts_with("title:") && !rule->m_v1Regex.passes(pWindow->m_title))
                    continue;

                if (!rule->m_v1Regex.passes(pWindow->m_class))
                    continue;

            } catch (...) {
                Debug::log(ERR, "Regex error at {}", rule->m_value);
                continue;
            }
        } else {
            try {
                if (rule->m_X11 != -1) {
                    if (pWindow->m_isX11 != rule->m_X11)
                        continue;
                }

                if (rule->m_floating != -1) {
                    if (hasFloating != rule->m_floating)
                        continue;
                }

                if (rule->m_fullscreen != -1) {
                    if (hasFullscreen != rule->m_fullscreen)
                        continue;
                }

                if (rule->m_pinned != -1) {
                    if (pWindow->m_pinned != rule->m_pinned)
                        continue;
                }

                if (rule->m_focus != -1) {
                    if (rule->m_focus != (g_pCompositor->m_lastWindow.lock() == pWindow))
                        continue;
                }

                if (rule->m_group != -1) {
                    if (rule->m_group != isGrouped)
                        continue;
                }

                if (!rule->m_fullscreenState.empty()) {
                    const auto ARGS = CVarList(rule->m_fullscreenState, 2, ' ');
                    //
                    std::optional<eFullscreenMode> internalMode, clientMode;

                    if (ARGS[0] == "*")
                        internalMode = std::nullopt;
                    else if (isNumber(ARGS[0]))
                        internalMode = sc<eFullscreenMode>(std::stoi(ARGS[0]));
                    else
                        throw std::runtime_error("szFullscreenState internal mode not valid");

                    if (ARGS[1] == "*")
                        clientMode = std::nullopt;
                    else if (isNumber(ARGS[1]))
                        clientMode = sc<eFullscreenMode>(std::stoi(ARGS[1]));
                    else
                        throw std::runtime_error("szFullscreenState client mode not valid");

                    if (internalMode.has_value() && pWindow->m_fullscreenState.internal != internalMode)
                        continue;

                    if (clientMode.has_value() && pWindow->m_fullscreenState.client != clientMode)
                        continue;
                }

                if (!rule->m_onWorkspace.empty()) {
                    const auto PWORKSPACE = pWindow->m_workspace;
                    if (!PWORKSPACE || !PWORKSPACE->matchesStaticSelector(rule->m_onWorkspace))
                        continue;
                }

                if (!rule->m_contentType.empty()) {
                    try {
                        const auto contentType = NContentType::fromString(rule->m_contentType);
                        if (pWindow->getContentType() != contentType)
                            continue;
                    } catch (std::exception& e) { Debug::log(ERR, "Rule \"content:{}\" failed with: {}", rule->m_contentType, e.what()); }
                }

                if (!rule->m_xdgTag.empty()) {
                    if (pWindow->xdgTag().value_or("") != rule->m_xdgTag)
                        continue;
                }

                if (!rule->m_workspace.empty()) {
                    const auto PWORKSPACE = pWindow->m_workspace;

                    if (!PWORKSPACE)
                        continue;

                    if (rule->m_workspace.starts_with("name:")) {
                        if (PWORKSPACE->m_name != rule->m_workspace.substr(5))
                            continue;
                    } else {
                        // number
                        if (!isNumber(rule->m_workspace))
                            throw std::runtime_error("szWorkspace not name: or number");

                        const int64_t ID = std::stoll(rule->m_workspace);

                        if (PWORKSPACE->m_id != ID)
                            continue;
                    }
                }

                if (!rule->m_tag.empty() && !tags.isTagged(rule->m_tag))
                    continue;

                if (!rule->m_class.empty() && !rule->m_classRegex.passes(pWindow->m_class))
                    continue;

                if (!rule->m_title.empty() && !rule->m_titleRegex.passes(pWindow->m_title))
                    continue;

                if (!rule->m_initialTitle.empty() && !rule->m_initialTitleRegex.passes(pWindow->m_initialTitle))
                    continue;

                if (!rule->m_initialClass.empty() && !rule->m_initialClassRegex.passes(pWindow->m_initialClass))
                    continue;

            } catch (std::exception& e) {
                Debug::log(ERR, "Regex error at {} ({})", rule->m_value, e.what());
                continue;
            }
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule {} -> {} matched {}", rule->m_rule, rule->m_value, pWindow);

        returns.emplace_back(rule);

        // apply tag with local tags
        if (rule->m_ruleType == CWindowRule::RULE_TAG) {
            CVarList vars{rule->m_rule, 0, 's', true};
            if (vars.size() == 2 && vars[0] == "tag")
                tags.applyTag(vars[1], true);
        }

        if (dynamic)
            continue;

        if (rule->m_rule == "float")
            hasFloating = true;
        else if (rule->m_rule == "fullscreen")
            hasFullscreen = true;
    }

    std::vector<uint64_t> PIDs = {sc<uint64_t>(pWindow->getPID())};
    while (getPPIDof(PIDs.back()) > 10)
        PIDs.push_back(getPPIDof(PIDs.back()));

    bool anyExecFound = false;

    for (auto const& er : m_execRequestedRules) {
        if (std::ranges::any_of(PIDs, [&](const auto& pid) { return pid == er.iPid; })) {
            returns.emplace_back(makeShared<CWindowRule>(er.szRule, "", false, true));
            anyExecFound = true;
        }
    }

    if (anyExecFound && !shadowExec) // remove exec rules to unclog searches in the future, why have the garbage here.
        std::erase_if(m_execRequestedRules, [&](const SExecRequestedRule& other) { return std::ranges::any_of(PIDs, [&](const auto& pid) { return pid == other.iPid; }); });

    return returns;
}

std::vector<SP<CLayerRule>> CConfigManager::getMatchingRules(PHLLS pLS) {
    std::vector<SP<CLayerRule>> returns;

    if (!pLS->m_layerSurface || pLS->m_fadingOut)
        return returns;

    for (auto const& lr : m_layerRules) {
        if (lr->m_targetNamespace.starts_with("address:0x")) {
            if (std::format("address:0x{:x}", rc<uintptr_t>(pLS.get())) != lr->m_targetNamespace)
                continue;
        } else if (!lr->m_targetNamespaceRegex.passes(pLS->m_layerSurface->m_layerNamespace))
            continue;

        // hit
        returns.emplace_back(lr);
    }

    if (shouldBlurLS(pLS->m_layerSurface->m_layerNamespace))
        returns.emplace_back(makeShared<CLayerRule>(pLS->m_layerSurface->m_layerNamespace, "blur"));

    return returns;
}

void CConfigManager::dispatchExecOnce() {
    if (m_firstExecDispatched || m_isFirstLaunch)
        return;

    // update dbus env
    if (g_pCompositor->m_aqBackend->hasSession())
        handleRawExec("",
#ifdef USES_SYSTEMD
                      "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
                      "dbus-update-activation-environment 2>/dev/null && "
#endif
                      "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS");

    m_firstExecDispatched = true;
    m_isLaunchingExecOnce = true;

    for (auto const& c : m_firstExecRequests) {
        c.withRules ? handleExec("", c.exec) : handleRawExec("", c.exec);
    }

    m_firstExecRequests.clear(); // free some kb of memory :P
    m_isLaunchingExecOnce = false;

    // set input, fixes some certain issues
    g_pInputManager->setKeyboardLayout();
    g_pInputManager->setPointerConfigs();
    g_pInputManager->setTouchDeviceConfigs();
    g_pInputManager->setTabletConfigs();

    // check for user's possible errors with their setup and notify them if needed
    g_pCompositor->performUserChecks();
}

void CConfigManager::dispatchExecShutdown() {
    if (m_finalExecRequests.empty()) {
        g_pCompositor->m_finalRequests = false;
        return;
    }

    g_pCompositor->m_finalRequests = true;

    for (auto const& c : m_finalExecRequests) {
        handleExecShutdown("", c);
    }

    m_finalExecRequests.clear();

    // Actually exit now
    handleExecShutdown("", "hyprctl dispatch exit");
}

void CConfigManager::performMonitorReload() {
    handleMonitorv2();

    bool overAgain = false;

    for (auto const& m : g_pCompositor->m_realMonitors) {
        if (!m->m_output || m->m_isUnsafeFallback)
            continue;

        auto rule = getMonitorRuleFor(m);

        if (!m->applyMonitorRule(&rule)) {
            overAgain = true;
            break;
        }

        // ensure mirror
        m->setMirror(rule.mirrorOf);

        g_pHyprRenderer->arrangeLayersForMonitor(m->m_id);
    }

    if (overAgain)
        performMonitorReload();

    m_wantsMonitorReload = false;

    EMIT_HOOK_EVENT("monitorLayoutChanged", nullptr);
}

void* const* CConfigManager::getConfigValuePtr(const std::string& val) {
    const auto VAL = m_config->getConfigValuePtr(val.c_str());
    if (!VAL)
        return nullptr;
    return VAL->getDataStaticPtr();
}

Hyprlang::CConfigValue* CConfigManager::getHyprlangConfigValuePtr(const std::string& name, const std::string& specialCat) {
    if (!specialCat.empty())
        return m_config->getSpecialConfigValuePtr(specialCat.c_str(), name.c_str(), nullptr);

    if (name.starts_with("plugin:"))
        return m_config->getSpecialConfigValuePtr("plugin", name.substr(7).c_str(), nullptr);

    return m_config->getConfigValuePtr(name.c_str());
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    auto copy = dev;
    std::ranges::replace(copy, ' ', '-');

    return m_config->specialCategoryExistsForKey("device", copy.c_str());
}

bool CConfigManager::shouldBlurLS(const std::string& ns) {
    for (auto const& bls : m_blurLSNamespaces) {
        if (bls == ns) {
            return true;
        }
    }

    return false;
}

void CConfigManager::ensureMonitorStatus() {
    for (auto const& rm : g_pCompositor->m_realMonitors) {
        if (!rm->m_output || rm->m_isUnsafeFallback)
            continue;

        auto rule = getMonitorRuleFor(rm);

        if (rule.disabled == rm->m_enabled)
            rm->applyMonitorRule(&rule);
    }
}

void CConfigManager::ensureVRR(PHLMONITOR pMonitor) {
    static auto PVRR = rc<Hyprlang::INT* const*>(getConfigValuePtr("misc:vrr"));

    static auto ensureVRRForDisplay = [&](PHLMONITOR m) -> void {
        if (!m->m_output || m->m_createdByUser)
            return;

        const auto USEVRR = m->m_activeMonitorRule.vrr.has_value() ? m->m_activeMonitorRule.vrr.value() : **PVRR;

        if (USEVRR == 0) {
            if (m->m_vrrActive) {
                m->m_output->state->resetExplicitFences();
                m->m_output->state->setAdaptiveSync(false);

                if (!m->m_state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> false", m->m_output->name);
            }
            m->m_vrrActive = false;
            return;
        } else if (USEVRR == 1) {
            if (!m->m_vrrActive) {
                m->m_output->state->resetExplicitFences();
                m->m_output->state->setAdaptiveSync(true);

                if (!m->m_state.test()) {
                    Debug::log(LOG, "Pending output {} does not accept VRR.", m->m_output->name);
                    m->m_output->state->setAdaptiveSync(false);
                }

                if (!m->m_state.commit())
                    Debug::log(ERR, "Couldn't commit output {} in ensureVRR -> true", m->m_output->name);
            }
            m->m_vrrActive = true;
            return;
        } else if (USEVRR == 2 || USEVRR == 3) {
            const auto PWORKSPACE = m->m_activeWorkspace;

            if (!PWORKSPACE)
                return; // ???

            bool wantVRR = PWORKSPACE->m_hasFullscreenWindow && (PWORKSPACE->m_fullscreenMode & FSMODE_FULLSCREEN);
            if (wantVRR && PWORKSPACE->getFullscreenWindow()->m_windowData.noVRR.valueOrDefault())
                wantVRR = false;

            if (wantVRR && USEVRR == 3) {
                const auto contentType = PWORKSPACE->getFullscreenWindow()->getContentType();
                wantVRR                = contentType == CONTENT_TYPE_GAME || contentType == CONTENT_TYPE_VIDEO;
            }

            if (wantVRR) {
                /* fullscreen */
                m->m_vrrActive = true;

                if (!m->m_output->state->state().adaptiveSync) {
                    m->m_output->state->setAdaptiveSync(true);

                    if (!m->m_state.test()) {
                        Debug::log(LOG, "Pending output {} does not accept VRR.", m->m_output->name);
                        m->m_output->state->setAdaptiveSync(false);
                    }
                }
            } else {
                m->m_vrrActive = false;

                m->m_output->state->setAdaptiveSync(false);
            }
        }
    };

    if (pMonitor) {
        ensureVRRForDisplay(pMonitor);
        return;
    }

    for (auto const& m : g_pCompositor->m_monitors) {
        ensureVRRForDisplay(m);
    }
}

SP<SAnimationPropertyConfig> CConfigManager::getAnimationPropertyConfig(const std::string& name) {
    return m_animationTree.getConfig(name);
}

void CConfigManager::addParseError(const std::string& err) {
    g_pHyprError->queueCreate(err + "\nHyprland may not work correctly.", CHyprColor(1.0, 50.0 / 255.0, 50.0 / 255.0, 1.0));
}

PHLMONITOR CConfigManager::getBoundMonitorForWS(const std::string& wsname) {
    auto monitor = getBoundMonitorStringForWS(wsname);
    if (monitor.starts_with("desc:"))
        return g_pCompositor->getMonitorFromDesc(trim(monitor.substr(5)));
    else
        return g_pCompositor->getMonitorFromName(monitor);
}

std::string CConfigManager::getBoundMonitorStringForWS(const std::string& wsname) {
    for (auto const& wr : m_workspaceRules) {
        const auto WSNAME = wr.workspaceName.starts_with("name:") ? wr.workspaceName.substr(5) : wr.workspaceName;

        if (WSNAME == wsname)
            return wr.monitor;
    }

    return "";
}

const std::vector<SWorkspaceRule>& CConfigManager::getAllWorkspaceRules() {
    return m_workspaceRules;
}

void CConfigManager::addExecRule(const SExecRequestedRule& rule) {
    m_execRequestedRules.push_back(rule);
}

void CConfigManager::handlePluginLoads() {
    if (!g_pPluginSystem)
        return;

    bool pluginsChanged = false;
    g_pPluginSystem->updateConfigPlugins(m_declaredPlugins, pluginsChanged);

    if (pluginsChanged) {
        g_pHyprError->destroy();
        reload();
    }
}

const std::unordered_map<std::string, SP<SAnimationPropertyConfig>>& CConfigManager::getAnimationConfig() {
    return m_animationTree.getFullConfig();
}

void CConfigManager::addPluginConfigVar(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value) {
    if (!name.starts_with("plugin:"))
        return;

    std::string field = name.substr(7);

    m_config->addSpecialConfigValue("plugin", field.c_str(), value);
    m_pluginVariables.push_back({handle, field});
}

void CConfigManager::addPluginKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fn, Hyprlang::SHandlerOptions opts) {
    m_pluginKeywords.emplace_back(SPluginKeyword{handle, name, fn});
    m_config->registerHandler(fn, name.c_str(), opts);
}

void CConfigManager::removePluginConfig(HANDLE handle) {
    for (auto const& k : m_pluginKeywords) {
        if (k.handle != handle)
            continue;

        m_config->unregisterHandler(k.name.c_str());
    }

    std::erase_if(m_pluginKeywords, [&](const auto& other) { return other.handle == handle; });
    for (auto const& [h, n] : m_pluginVariables) {
        if (h != handle)
            continue;

        m_config->removeSpecialConfigValue("plugin", n.c_str());
    }
    std::erase_if(m_pluginVariables, [handle](const auto& other) { return other.handle == handle; });
}

std::string CConfigManager::getDefaultWorkspaceFor(const std::string& name) {
    for (auto other = m_workspaceRules.begin(); other != m_workspaceRules.end(); ++other) {
        if (other->isDefault) {
            if (other->monitor == name)
                return other->workspaceString;
            if (other->monitor.starts_with("desc:")) {
                auto const monitor = g_pCompositor->getMonitorFromDesc(trim(other->monitor.substr(5)));
                if (monitor && monitor->m_name == name)
                    return other->workspaceString;
            }
        }
    }
    return "";
}

std::optional<std::string> CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch) {
        m_firstExecRequests.push_back({args, false});
        return {};
    }

    g_pKeybindManager->spawnRaw(args);
    return {};
}

std::optional<std::string> CConfigManager::handleExec(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch) {
        m_firstExecRequests.push_back({args, true});
        return {};
    }

    g_pKeybindManager->spawn(args);
    return {};
}

std::optional<std::string> CConfigManager::handleExecOnce(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch)
        m_firstExecRequests.push_back({args, true});

    return {};
}

std::optional<std::string> CConfigManager::handleExecRawOnce(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch)
        m_firstExecRequests.push_back({args, false});

    return {};
}

std::optional<std::string> CConfigManager::handleExecShutdown(const std::string& command, const std::string& args) {
    if (g_pCompositor->m_finalRequests) {
        g_pKeybindManager->spawn(args);
        return {};
    }

    m_finalExecRequests.push_back(args);
    return {};
}

static bool parseModeLine(const std::string& modeline, drmModeModeInfo& mode) {
    auto args = CVarList(modeline, 0, 's');

    auto keyword = args[0];
    std::ranges::transform(keyword, keyword.begin(), ::tolower);

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

    for (; argno < sc<int>(args.size()); argno++) {
        auto key = args[argno];
        std::ranges::transform(key, key.begin(), ::tolower);

        auto it = flagsmap.find(key);

        if (it != flagsmap.end())
            mode.flags |= it->second;
        else
            Debug::log(ERR, "Invalid flag {} in modeline", key);
    }

    snprintf(mode.name, sizeof(mode.name), "%dx%d@%d", mode.hdisplay, mode.vdisplay, mode.vrefresh / 1000);

    return true;
}

CMonitorRuleParser::CMonitorRuleParser(const std::string& name) {
    m_rule.name = name;
}

const std::string& CMonitorRuleParser::name() {
    return m_rule.name;
}

SMonitorRule& CMonitorRuleParser::rule() {
    return m_rule;
}

std::optional<std::string> CMonitorRuleParser::getError() {
    if (m_error.empty())
        return {};
    return m_error;
}

bool CMonitorRuleParser::parseMode(const std::string& value) {
    if (value.starts_with("pref"))
        m_rule.resolution = Vector2D();
    else if (value.starts_with("highrr"))
        m_rule.resolution = Vector2D(-1, -1);
    else if (value.starts_with("highres"))
        m_rule.resolution = Vector2D(-1, -2);
    else if (value.starts_with("maxwidth"))
        m_rule.resolution = Vector2D(-1, -3);
    else if (parseModeLine(value, m_rule.drmMode)) {
        m_rule.resolution  = Vector2D(m_rule.drmMode.hdisplay, m_rule.drmMode.vdisplay);
        m_rule.refreshRate = sc<float>(m_rule.drmMode.vrefresh) / 1000;
    } else {

        if (!value.contains("x")) {
            m_error += "invalid resolution ";
            m_rule.resolution = Vector2D();
            return false;
        } else {
            try {
                m_rule.resolution.x = stoi(value.substr(0, value.find_first_of('x')));
                m_rule.resolution.y = stoi(value.substr(value.find_first_of('x') + 1, value.find_first_of('@')));

                if (value.contains("@"))
                    m_rule.refreshRate = stof(value.substr(value.find_first_of('@') + 1));
            } catch (...) {
                m_error += "invalid resolution ";
                m_rule.resolution = Vector2D();
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parsePosition(const std::string& value, bool isFirst) {
    if (value.starts_with("auto")) {
        m_rule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
        // If this is the first monitor rule needs to be on the right.
        if (value == "auto-right" || value == "auto" || isFirst)
            m_rule.autoDir = eAutoDirs::DIR_AUTO_RIGHT;
        else if (value == "auto-left")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_LEFT;
        else if (value == "auto-up")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_UP;
        else if (value == "auto-down")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_DOWN;
        else if (value == "auto-center-right")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_CENTER_RIGHT;
        else if (value == "auto-center-left")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_CENTER_LEFT;
        else if (value == "auto-center-up")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_CENTER_UP;
        else if (value == "auto-center-down")
            m_rule.autoDir = eAutoDirs::DIR_AUTO_CENTER_DOWN;
        else {
            Debug::log(WARN,
                       "Invalid auto direction. Valid options are 'auto',"
                       "'auto-up', 'auto-down', 'auto-left', 'auto-right',"
                       "'auto-center-up', 'auto-center-down',"
                       "'auto-center-left', and 'auto-center-right'.");
            m_error += "invalid auto direction ";
            return false;
        }
    } else {
        if (!value.contains("x")) {
            m_error += "invalid offset ";
            m_rule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
            return false;
        } else {
            try {
                m_rule.offset.x = stoi(value.substr(0, value.find_first_of('x')));
                m_rule.offset.y = stoi(value.substr(value.find_first_of('x') + 1));
            } catch (...) {
                m_error += "invalid offset ";
                m_rule.offset = Vector2D(-INT32_MAX, -INT32_MAX);
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parseScale(const std::string& value) {
    if (value.starts_with("auto"))
        m_rule.scale = -1;
    else {
        if (!isNumber(value, true)) {
            m_error += "invalid scale ";
            return false;
        } else {
            m_rule.scale = stof(value);

            if (m_rule.scale < 0.25f) {
                m_error += "invalid scale ";
                m_rule.scale = 1;
                return false;
            }
        }
    }
    return true;
}

bool CMonitorRuleParser::parseTransform(const std::string& value) {
    const auto TSF = std::stoi(value);
    if (std::clamp(TSF, 0, 7) != TSF) {
        Debug::log(ERR, "Invalid transform {} in monitor", TSF);
        m_error += "invalid transform ";
        return false;
    }
    m_rule.transform = sc<wl_output_transform>(TSF);
    return true;
}

bool CMonitorRuleParser::parseBitdepth(const std::string& value) {
    m_rule.enable10bit = value == "10";
    return true;
}

bool CMonitorRuleParser::parseCM(const std::string& value) {
    if (value == "auto")
        m_rule.cmType = CM_AUTO;
    else if (value == "srgb")
        m_rule.cmType = CM_SRGB;
    else if (value == "wide")
        m_rule.cmType = CM_WIDE;
    else if (value == "edid")
        m_rule.cmType = CM_EDID;
    else if (value == "hdr")
        m_rule.cmType = CM_HDR;
    else if (value == "hdredid")
        m_rule.cmType = CM_HDR_EDID;
    else {
        m_error += "invalid cm ";
        return false;
    }
    return true;
}

bool CMonitorRuleParser::parseSDRBrightness(const std::string& value) {
    try {
        m_rule.sdrBrightness = stof(value);
    } catch (...) {
        m_error += "invalid sdrbrightness ";
        return false;
    }
    return true;
}

bool CMonitorRuleParser::parseSDRSaturation(const std::string& value) {
    try {
        m_rule.sdrSaturation = stof(value);
    } catch (...) {
        m_error += "invalid sdrsaturation ";
        return false;
    }
    return true;
}

bool CMonitorRuleParser::parseVRR(const std::string& value) {
    if (!isNumber(value)) {
        m_error += "invalid vrr ";
        return false;
    }

    m_rule.vrr = std::stoi(value);
    return true;
}

void CMonitorRuleParser::setDisabled() {
    m_rule.disabled = true;
}

void CMonitorRuleParser::setMirror(const std::string& value) {
    m_rule.mirrorOf = value;
}

bool CMonitorRuleParser::setReserved(const SMonitorAdditionalReservedArea& value) {
    g_pConfigManager->m_mAdditionalReservedAreas[name()] = value;
    return true;
}

std::optional<std::string> CConfigManager::handleMonitor(const std::string& command, const std::string& args) {

    // get the monitor config
    const auto ARGS = CVarList(args);

    auto       parser = CMonitorRuleParser(ARGS[0]);

    if (ARGS[1] == "disable" || ARGS[1] == "disabled" || ARGS[1] == "addreserved" || ARGS[1] == "transform") {
        if (ARGS[1] == "disable" || ARGS[1] == "disabled")
            parser.setDisabled();
        else if (ARGS[1] == "transform") {
            if (!parser.parseTransform(ARGS[2]))
                return parser.getError();

            const auto TRANSFORM = parser.rule().transform;

            // overwrite if exists
            for (auto& r : m_monitorRules) {
                if (r.name == parser.name()) {
                    r.transform = TRANSFORM;
                    return {};
                }
            }

            return {};
        } else if (ARGS[1] == "addreserved") {
            parser.setReserved({.top = std::stoi(ARGS[2]), .bottom = std::stoi(ARGS[3]), .left = std::stoi(ARGS[4]), .right = std::stoi(ARGS[5])});
            return {};
        } else {
            Debug::log(ERR, "ConfigManager parseMonitor, curitem bogus???");
            return "parse error: curitem bogus";
        }

        std::erase_if(m_monitorRules, [&](const auto& other) { return other.name == parser.name(); });

        m_monitorRules.push_back(parser.rule());

        return {};
    }

    parser.parseMode(ARGS[1]);
    parser.parsePosition(ARGS[2]);
    parser.parseScale(ARGS[3]);

    int argno = 4;

    while (!ARGS[argno].empty()) {
        if (ARGS[argno] == "mirror") {
            parser.setMirror(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "bitdepth") {
            parser.parseBitdepth(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "cm") {
            parser.parseCM(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "sdrsaturation") {
            parser.parseSDRSaturation(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "sdrbrightness") {
            parser.parseSDRBrightness(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "transform") {
            parser.parseTransform(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "vrr") {
            parser.parseVRR(ARGS[argno + 1]);
            argno++;
        } else if (ARGS[argno] == "workspace") {
            const auto& [id, name] = getWorkspaceIDNameFromString(ARGS[argno + 1]);

            SWorkspaceRule wsRule;
            wsRule.monitor         = parser.name();
            wsRule.workspaceString = ARGS[argno + 1];
            wsRule.workspaceId     = id;
            wsRule.workspaceName   = name;

            m_workspaceRules.emplace_back(wsRule);
            argno++;
        } else {
            Debug::log(ERR, "Config error: invalid monitor syntax at \"{}\"", ARGS[argno]);
            return "invalid syntax at \"" + ARGS[argno] + "\"";
        }

        argno++;
    }

    auto newrule = parser.rule();

    std::erase_if(m_monitorRules, [&](const auto& other) { return other.name == newrule.name; });

    m_monitorRules.push_back(newrule);

    return parser.getError();
}

std::optional<std::string> CConfigManager::handleBezier(const std::string& command, const std::string& args) {
    const auto  ARGS = CVarList(args);

    std::string bezierName = ARGS[0];

    if (ARGS[1].empty())
        return "too few arguments";
    float p1x = std::stof(ARGS[1]);

    if (ARGS[2].empty())
        return "too few arguments";
    float p1y = std::stof(ARGS[2]);

    if (ARGS[3].empty())
        return "too few arguments";
    float p2x = std::stof(ARGS[3]);

    if (ARGS[4].empty())
        return "too few arguments";
    float p2y = std::stof(ARGS[4]);

    if (!ARGS[5].empty())
        return "too many arguments";

    g_pAnimationManager->addBezierWithName(bezierName, Vector2D(p1x, p1y), Vector2D(p2x, p2y));

    return {};
}

std::optional<std::string> CConfigManager::handleAnimation(const std::string& command, const std::string& args) {
    const auto ARGS = CVarList(args);

    // Master on/off

    // anim name
    const auto ANIMNAME = ARGS[0];

    if (!m_animationTree.nodeExists(ANIMNAME))
        return "no such animation";

    // This helper casts strings like "1", "true", "off", "yes"... to int.
    int64_t enabledInt = configStringToInt(ARGS[1]).value_or(0) == 1;

    // Checking that the int is 1 or 0 because the helper can return integers out of range.
    if (enabledInt != 0 && enabledInt != 1)
        return "invalid animation on/off state";

    if (!enabledInt) {
        m_animationTree.setConfigForNode(ANIMNAME, enabledInt, 1, "default");
        return {};
    }

    float speed = -1;

    // speed
    if (isNumber(ARGS[2], true)) {
        speed = std::stof(ARGS[2]);

        if (speed <= 0) {
            speed = 1.f;
            return "invalid speed";
        }
    } else {
        speed = 10.f;
        return "invalid speed";
    }

    std::string bezierName = ARGS[3];
    m_animationTree.setConfigForNode(ANIMNAME, enabledInt, speed, ARGS[3], ARGS[4]);

    if (!g_pAnimationManager->bezierExists(bezierName)) {
        const auto PANIMNODE      = m_animationTree.getConfig(ANIMNAME);
        PANIMNODE->internalBezier = "default";
        return "no such bezier";
    }

    if (!ARGS[4].empty()) {
        auto ERR = g_pAnimationManager->styleValidInConfigVar(ANIMNAME, ARGS[4]);

        if (!ERR.empty())
            return ERR;
    }

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
    bool       locked         = false;
    bool       release        = false;
    bool       repeat         = false;
    bool       mouse          = false;
    bool       nonConsuming   = false;
    bool       transparent    = false;
    bool       ignoreMods     = false;
    bool       multiKey       = false;
    bool       longPress      = false;
    bool       hasDescription = false;
    bool       dontInhibit    = false;
    bool       click          = false;
    bool       drag           = false;
    const auto BINDARGS       = command.substr(4);

    for (auto const& arg : BINDARGS) {
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
        } else if (arg == 'o') {
            longPress = true;
        } else if (arg == 'd') {
            hasDescription = true;
        } else if (arg == 'p') {
            dontInhibit = true;
        } else if (arg == 'c') {
            click   = true;
            release = true;
        } else if (arg == 'g') {
            drag    = true;
            release = true;
        } else {
            return "bind: invalid flag";
        }
    }

    if ((longPress || release) && repeat)
        return "flags e is mutually exclusive with r and o";

    if (mouse && (repeat || release || locked))
        return "flag m is exclusive";

    if (click && drag)
        return "flags c and g are mutually exclusive";

    const int  numbArgs = hasDescription ? 5 : 4;
    const auto ARGS     = CVarList(value, numbArgs);

    const int  DESCR_OFFSET = hasDescription ? 1 : 0;
    if ((ARGS.size() < 3 && !mouse) || (ARGS.size() < 3 && mouse))
        return "bind: too few args";
    else if ((ARGS.size() > sc<size_t>(4) + DESCR_OFFSET && !mouse) || (ARGS.size() > sc<size_t>(3) + DESCR_OFFSET && mouse))
        return "bind: too many args";

    std::set<xkb_keysym_t> KEYSYMS;
    std::set<xkb_keysym_t> MODS;

    if (multiKey) {
        for (const auto& splitKey : CVarList(ARGS[1], 8, '&')) {
            KEYSYMS.insert(xkb_keysym_from_name(splitKey.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
        for (const auto& splitMod : CVarList(ARGS[0], 8, '&')) {
            MODS.insert(xkb_keysym_from_name(splitMod.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
    }
    const auto MOD    = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto MODSTR = ARGS[0];

    const auto KEY = multiKey ? "" : ARGS[1];

    const auto DESCRIPTION = hasDescription ? ARGS[2] : "";

    auto       HANDLER = ARGS[2 + DESCR_OFFSET];

    const auto COMMAND = mouse ? HANDLER : ARGS[3 + DESCR_OFFSET];

    if (mouse)
        HANDLER = "mouse";

    // to lower
    std::ranges::transform(HANDLER, HANDLER.begin(), ::tolower);

    const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(HANDLER);

    if (DISPATCHER == g_pKeybindManager->m_dispatchers.end()) {
        Debug::log(ERR, "Invalid dispatcher: {}", HANDLER);
        return "Invalid dispatcher, requested \"" + HANDLER + "\" does not exist";
    }

    if (MOD == 0 && !MODSTR.empty()) {
        Debug::log(ERR, "Invalid mod: {}", MODSTR);
        return "Invalid mod, requested mod \"" + MODSTR + "\" is not a valid mod.";
    }

    if ((!KEY.empty()) || multiKey) {
        SParsedKey parsedKey = parseKey(KEY);

        if (parsedKey.catchAll && m_currentSubmap.empty()) {
            Debug::log(ERR, "Catchall not allowed outside of submap!");
            return "Invalid catchall, catchall keybinds are only allowed in submaps.";
        }

        g_pKeybindManager->addKeybind(SKeybind{parsedKey.key, KEYSYMS,      parsedKey.keycode, parsedKey.catchAll, MOD,      MODS,           HANDLER,
                                               COMMAND,       locked,       m_currentSubmap,   DESCRIPTION,        release,  repeat,         longPress,
                                               mouse,         nonConsuming, transparent,       ignoreMods,         multiKey, hasDescription, dontInhibit,
                                               click,         drag});
    }

    return {};
}

std::optional<std::string> CConfigManager::handleUnbind(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value);

    if (ARGS.size() == 1 && ARGS[0] == "all") {
        g_pKeybindManager->m_keybinds.clear();
        g_pKeybindManager->m_activeKeybinds.clear();
        g_pKeybindManager->m_lastLongPressKeybind.reset();
        return {};
    }

    const auto MOD = g_pKeybindManager->stringToModMask(ARGS[0]);

    const auto KEY = parseKey(ARGS[1]);

    g_pKeybindManager->removeKeybind(MOD, KEY);

    return {};
}

std::optional<std::string> CConfigManager::handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE  = trim(value.substr(0, value.find_first_of(',')));
    const auto VALUE = value.substr(value.find_first_of(',') + 1);

    auto       rule = makeShared<CWindowRule>(RULE, VALUE, true);

    if (rule->m_ruleType == CWindowRule::RULE_INVALID && RULE != "unset") {
        Debug::log(ERR, "Invalid rulev2 found: {}", RULE);
        return "Invalid rulev2 found: " + RULE;
    }

    // now we estract shit from the value
    const auto TAGPOS             = VALUE.find("tag:");
    const auto TITLEPOS           = VALUE.find("title:");
    const auto CLASSPOS           = VALUE.find("class:");
    const auto INITIALTITLEPOS    = VALUE.find("initialTitle:");
    const auto INITIALCLASSPOS    = VALUE.find("initialClass:");
    const auto X11POS             = VALUE.find("xwayland:");
    const auto FLOATPOS           = VALUE.find("floating:");
    const auto FULLSCREENPOS      = VALUE.find("fullscreen:");
    const auto PINNEDPOS          = VALUE.find("pinned:");
    const auto FOCUSPOS           = VALUE.find("focus:");
    const auto FULLSCREENSTATEPOS = VALUE.find("fullscreenstate:");
    const auto ONWORKSPACEPOS     = VALUE.find("onworkspace:");
    const auto CONTENTTYPEPOS     = VALUE.find("content:");
    const auto XDGTAGPOS          = VALUE.find("xdgTag:");
    const auto GROUPPOS           = VALUE.find("group:");

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

    const auto checkPos = std::unordered_set{TAGPOS,    TITLEPOS,           CLASSPOS,     INITIALTITLEPOS, INITIALCLASSPOS, X11POS,         FLOATPOS,  FULLSCREENPOS,
                                             PINNEDPOS, FULLSCREENSTATEPOS, WORKSPACEPOS, FOCUSPOS,        ONWORKSPACEPOS,  CONTENTTYPEPOS, XDGTAGPOS, GROUPPOS};
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
        if (FULLSCREENSTATEPOS > pos && FULLSCREENSTATEPOS < min)
            min = FULLSCREENSTATEPOS;
        if (ONWORKSPACEPOS > pos && ONWORKSPACEPOS < min)
            min = ONWORKSPACEPOS;
        if (WORKSPACEPOS > pos && WORKSPACEPOS < min)
            min = WORKSPACEPOS;
        if (FOCUSPOS > pos && FOCUSPOS < min)
            min = FOCUSPOS;
        if (CONTENTTYPEPOS > pos && CONTENTTYPEPOS < min)
            min = CONTENTTYPEPOS;
        if (XDGTAGPOS > pos && XDGTAGPOS < min)
            min = XDGTAGPOS;
        if (GROUPPOS > pos && GROUPPOS < min)
            min = GROUPPOS;

        result = result.substr(0, min - pos);

        result = trim(result);

        if (!result.empty() && result.back() == ',')
            result.pop_back();

        return result;
    };

    if (TAGPOS != std::string::npos)
        rule->m_tag = extract(TAGPOS + 4);

    if (CLASSPOS != std::string::npos) {
        rule->m_class      = extract(CLASSPOS + 6);
        rule->m_classRegex = {rule->m_class};
    }

    if (TITLEPOS != std::string::npos) {
        rule->m_title      = extract(TITLEPOS + 6);
        rule->m_titleRegex = {rule->m_title};
    }

    if (INITIALCLASSPOS != std::string::npos) {
        rule->m_initialClass      = extract(INITIALCLASSPOS + 13);
        rule->m_initialClassRegex = {rule->m_initialClass};
    }

    if (INITIALTITLEPOS != std::string::npos) {
        rule->m_initialTitle      = extract(INITIALTITLEPOS + 13);
        rule->m_initialTitleRegex = {rule->m_initialTitle};
    }

    if (X11POS != std::string::npos)
        rule->m_X11 = extract(X11POS + 9) == "1" ? 1 : 0;

    if (FLOATPOS != std::string::npos)
        rule->m_floating = extract(FLOATPOS + 9) == "1" ? 1 : 0;

    if (FULLSCREENPOS != std::string::npos)
        rule->m_fullscreen = extract(FULLSCREENPOS + 11) == "1" ? 1 : 0;

    if (PINNEDPOS != std::string::npos)
        rule->m_pinned = extract(PINNEDPOS + 7) == "1" ? 1 : 0;

    if (FULLSCREENSTATEPOS != std::string::npos)
        rule->m_fullscreenState = extract(FULLSCREENSTATEPOS + 16);

    if (WORKSPACEPOS != std::string::npos)
        rule->m_workspace = extract(WORKSPACEPOS + 10);

    if (FOCUSPOS != std::string::npos)
        rule->m_focus = extract(FOCUSPOS + 6) == "1" ? 1 : 0;

    if (ONWORKSPACEPOS != std::string::npos)
        rule->m_onWorkspace = extract(ONWORKSPACEPOS + 12);

    if (CONTENTTYPEPOS != std::string::npos)
        rule->m_contentType = extract(CONTENTTYPEPOS + 8);

    if (XDGTAGPOS != std::string::npos)
        rule->m_xdgTag = extract(XDGTAGPOS + 8);

    if (GROUPPOS != std::string::npos)
        rule->m_group = extract(GROUPPOS + 6) == "1" ? 1 : 0;

    if (RULE == "unset") {
        std::erase_if(m_windowRules, [&](const auto& other) {
            if (!other->m_v2)
                return other->m_class == rule->m_class && !rule->m_class.empty();
            else {
                if (!rule->m_tag.empty() && rule->m_tag != other->m_tag)
                    return false;

                if (!rule->m_class.empty() && rule->m_class != other->m_class)
                    return false;

                if (!rule->m_title.empty() && rule->m_title != other->m_title)
                    return false;

                if (!rule->m_initialClass.empty() && rule->m_initialClass != other->m_initialClass)
                    return false;

                if (!rule->m_initialTitle.empty() && rule->m_initialTitle != other->m_initialTitle)
                    return false;

                if (rule->m_X11 != -1 && rule->m_X11 != other->m_X11)
                    return false;

                if (rule->m_floating != -1 && rule->m_floating != other->m_floating)
                    return false;

                if (rule->m_fullscreen != -1 && rule->m_fullscreen != other->m_fullscreen)
                    return false;

                if (rule->m_pinned != -1 && rule->m_pinned != other->m_pinned)
                    return false;

                if (!rule->m_fullscreenState.empty() && rule->m_fullscreenState != other->m_fullscreenState)
                    return false;

                if (!rule->m_workspace.empty() && rule->m_workspace != other->m_workspace)
                    return false;

                if (rule->m_focus != -1 && rule->m_focus != other->m_focus)
                    return false;

                if (!rule->m_onWorkspace.empty() && rule->m_onWorkspace != other->m_onWorkspace)
                    return false;

                if (!rule->m_contentType.empty() && rule->m_contentType != other->m_contentType)
                    return false;

                if (rule->m_group != -1 && rule->m_group != other->m_group)
                    return false;

                return true;
            }
        });
        return {};
    }

    if (RULE.starts_with("size") || RULE.starts_with("maxsize") || RULE.starts_with("minsize"))
        m_windowRules.insert(m_windowRules.begin(), rule);
    else
        m_windowRules.push_back(rule);

    return {};
}

std::optional<std::string> CConfigManager::handleLayerRule(const std::string& command, const std::string& value) {
    const auto RULE  = trim(value.substr(0, value.find_first_of(',')));
    const auto VALUE = trim(value.substr(value.find_first_of(',') + 1));

    // check rule and value
    if (RULE.empty() || VALUE.empty())
        return "empty rule?";

    if (RULE == "unset") {
        std::erase_if(m_layerRules, [&](const auto& other) { return other->m_targetNamespace == VALUE; });
        return {};
    }

    auto rule = makeShared<CLayerRule>(RULE, VALUE);

    if (rule->m_ruleType == CLayerRule::RULE_INVALID) {
        Debug::log(ERR, "Invalid rule found: {}", RULE);
        return "Invalid rule found: " + RULE;
    }

    rule->m_targetNamespaceRegex = {VALUE};

    m_layerRules.emplace_back(rule);

    for (auto const& m : g_pCompositor->m_monitors)
        for (auto const& lsl : m->m_layerSurfaceLayers)
            for (auto const& ls : lsl)
                ls->applyRules();

    return {};
}

void CConfigManager::updateBlurredLS(const std::string& name, const bool forceBlur) {
    const bool  BYADDRESS = name.starts_with("address:");
    std::string matchName = name;

    if (BYADDRESS)
        matchName = matchName.substr(8);

    for (auto const& m : g_pCompositor->m_monitors) {
        for (auto const& lsl : m->m_layerSurfaceLayers) {
            for (auto const& ls : lsl) {
                if (BYADDRESS) {
                    if (std::format("0x{:x}", rc<uintptr_t>(ls.get())) == matchName)
                        ls->m_forceBlur = forceBlur;
                } else if (ls->m_namespace == matchName)
                    ls->m_forceBlur = forceBlur;
            }
        }
    }
}

std::optional<std::string> CConfigManager::handleBlurLS(const std::string& command, const std::string& value) {
    if (value.starts_with("remove,")) {
        const auto TOREMOVE = trim(value.substr(7));
        if (std::erase_if(m_blurLSNamespaces, [&](const auto& other) { return other == TOREMOVE; }))
            updateBlurredLS(TOREMOVE, false);
        return {};
    }

    m_blurLSNamespaces.emplace_back(value);
    updateBlurredLS(value, true);

    return {};
}

std::optional<std::string> CConfigManager::handleWorkspaceRules(const std::string& command, const std::string& value) {
    // This can either be the monitor or the workspace identifier
    const auto FIRST_DELIM = value.find_first_of(',');

    auto       first_ident = trim(value.substr(0, FIRST_DELIM));

    const auto& [id, name] = getWorkspaceIDNameFromString(first_ident);

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
    const static auto        ruleOnCreatedEmptyLen = ruleOnCreatedEmpty.length();

#define CHECK_OR_THROW(expr)                                                                                                                                                       \
                                                                                                                                                                                   \
    auto X = expr;                                                                                                                                                                 \
    if (!X) {                                                                                                                                                                      \
        return "Failed parsing a workspace rule";                                                                                                                                  \
    }

    auto assignRule = [&](std::string rule) -> std::optional<std::string> {
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
        else if ((delim = rule.find("border:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 7)))
            wsRule.noBorder = !*X;
        } else if ((delim = rule.find("shadow:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 7)))
            wsRule.noShadow = !*X;
        } else if ((delim = rule.find("rounding:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 9)))
            wsRule.noRounding = !*X;
        } else if ((delim = rule.find("decorate:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 9)))
            wsRule.decorate = *X;
        } else if ((delim = rule.find("monitor:")) != std::string::npos)
            wsRule.monitor = rule.substr(delim + 8);
        else if ((delim = rule.find("default:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 8)))
            wsRule.isDefault = *X;
        } else if ((delim = rule.find("persistent:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 11)))
            wsRule.isPersistent = *X;
        } else if ((delim = rule.find("defaultName:")) != std::string::npos)
            wsRule.defaultName = rule.substr(delim + 12);
        else if ((delim = rule.find(ruleOnCreatedEmpty)) != std::string::npos) {
            CHECK_OR_THROW(cleanCmdForWorkspace(name, rule.substr(delim + ruleOnCreatedEmptyLen)))
            wsRule.onCreatedEmptyRunCmd = *X;
        } else if ((delim = rule.find("layoutopt:")) != std::string::npos) {
            std::string opt = rule.substr(delim + 10);
            if (!opt.contains(":")) {
                // invalid
                Debug::log(ERR, "Invalid workspace rule found: {}", rule);
                return "Invalid workspace rule found: " + rule;
            }

            std::string val = opt.substr(opt.find(':') + 1);
            opt             = opt.substr(0, opt.find(':'));

            wsRule.layoutopts[opt] = val;
        }

        return {};
    };

#undef CHECK_OR_THROW

    CVarList rulesList{rules, 0, ',', true};
    for (auto const& r : rulesList) {
        const auto R = assignRule(r);
        if (R.has_value())
            return R;
    }

    wsRule.workspaceId   = id;
    wsRule.workspaceName = name;

    const auto IT = std::ranges::find_if(m_workspaceRules, [&](const auto& other) { return other.workspaceString == wsRule.workspaceString; });

    if (IT == m_workspaceRules.end())
        m_workspaceRules.emplace_back(wsRule);
    else
        *IT = mergeWorkspaceRules(*IT, wsRule);

    return {};
}

std::optional<std::string> CConfigManager::handleSubmap(const std::string& command, const std::string& submap) {
    if (submap == "reset")
        m_currentSubmap = "";
    else
        m_currentSubmap = submap;

    return {};
}

std::optional<std::string> CConfigManager::handleSource(const std::string& command, const std::string& rawpath) {
    if (rawpath.length() < 2) {
        Debug::log(ERR, "source= path garbage");
        return "source= path " + rawpath + " bogus!";
    }

    std::unique_ptr<glob_t, void (*)(glob_t*)> glob_buf{sc<glob_t*>(calloc(1, sizeof(glob_t))), // allocate and zero-initialize
                                                        [](glob_t* g) {
                                                            if (g) {
                                                                globfree(g); // free internal resources allocated by glob()
                                                                free(g);     // free the memory for the glob_t structure
                                                            }
                                                        }};

    if (auto r = glob(absolutePath(rawpath, m_configCurrentPath).c_str(), GLOB_TILDE, nullptr, glob_buf.get()); r != 0) {
        std::string err = std::format("source= globbing error: {}", r == GLOB_NOMATCH ? "found no match" : GLOB_ABORTED ? "read error" : "out of memory");
        Debug::log(ERR, "{}", err);
        return err;
    }

    std::string errorsFromParsing;

    for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
        auto            value = absolutePath(glob_buf->gl_pathv[i], m_configCurrentPath);

        std::error_code ec;
        auto            file_status = std::filesystem::status(value, ec);

        if (ec) {
            Debug::log(ERR, "source= file from glob result is inaccessible ({}): {}", ec.message(), value);
            return "source= file " + value + " is inaccessible!";
        }

        if (std::filesystem::is_regular_file(file_status)) {
            m_configPaths.emplace_back(value);
            auto configCurrentPathBackup = m_configCurrentPath;
            m_configCurrentPath          = value;
            const auto THISRESULT        = m_config->parseFile(value.c_str());
            m_configCurrentPath          = configCurrentPathBackup;
            if (THISRESULT.error && errorsFromParsing.empty())
                errorsFromParsing += THISRESULT.getError();
        } else if (std::filesystem::is_directory(file_status)) {
            Debug::log(WARN, "source= skipping directory {}", value);
            continue;
        } else {
            Debug::log(WARN, "source= skipping non-regular-file {}", value);
            continue;
        }
    }

    if (errorsFromParsing.empty())
        return {};
    return errorsFromParsing;
}

std::optional<std::string> CConfigManager::handleEnv(const std::string& command, const std::string& value) {
    const auto ARGS = CVarList(value, 2);

    if (ARGS[0].empty())
        return "env empty";

    if (!m_isFirstLaunch) {
        // check if env changed at all. If it didn't, ignore. If it did, update it.
        const auto* ENV = getenv(ARGS[0].c_str());
        if (ENV && ENV == ARGS[1])
            return {}; // env hasn't changed
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

    return {};
}

std::optional<std::string> CConfigManager::handlePlugin(const std::string& command, const std::string& path) {
    if (std::ranges::find(m_declaredPlugins, path) != m_declaredPlugins.end())
        return "plugin '" + path + "' declared twice";

    m_declaredPlugins.push_back(path);

    return {};
}

std::optional<std::string> CConfigManager::handlePermission(const std::string& command, const std::string& value) {
    CVarList                    data(value);

    eDynamicPermissionType      type = PERMISSION_TYPE_UNKNOWN;
    eDynamicPermissionAllowMode mode = PERMISSION_RULE_ALLOW_MODE_UNKNOWN;

    if (data[1] == "screencopy")
        type = PERMISSION_TYPE_SCREENCOPY;
    else if (data[1] == "plugin")
        type = PERMISSION_TYPE_PLUGIN;
    else if (data[1] == "keyboard" || data[1] == "keeb")
        type = PERMISSION_TYPE_KEYBOARD;

    if (data[2] == "ask")
        mode = PERMISSION_RULE_ALLOW_MODE_ASK;
    else if (data[2] == "allow")
        mode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
    else if (data[2] == "deny")
        mode = PERMISSION_RULE_ALLOW_MODE_DENY;

    if (type == PERMISSION_TYPE_UNKNOWN)
        return "unknown permission type";
    if (mode == PERMISSION_RULE_ALLOW_MODE_UNKNOWN)
        return "unknown permission allow mode";

    if (m_isFirstLaunch)
        g_pDynamicPermissionManager->addConfigPermissionRule(data[0], type, mode);

    return {};
}

std::optional<std::string> CConfigManager::handleGesture(const std::string& command, const std::string& value) {
    CConstVarList             data(value);

    size_t                    fingerCount = 0;
    eTrackpadGestureDirection direction   = TRACKPAD_GESTURE_DIR_NONE;

    try {
        fingerCount = std::stoul(std::string{data[0]});
    } catch (...) { return std::format("Invalid value {} for finger count", data[0]); }

    if (fingerCount <= 1 || fingerCount >= 10)
        return std::format("Invalid value {} for finger count", data[0]);

    direction = g_pTrackpadGestures->dirForString(data[1]);

    if (direction == TRACKPAD_GESTURE_DIR_NONE)
        return std::format("Invalid direction: {}", data[1]);

    int      startDataIdx = 2;
    uint32_t modMask      = 0;
    float    deltaScale   = 1.F;

    while (true) {

        if (data[startDataIdx].starts_with("mod:")) {
            modMask = g_pKeybindManager->stringToModMask(std::string{data[startDataIdx].substr(4)});
            startDataIdx++;
            continue;
        } else if (data[startDataIdx].starts_with("scale:")) {
            try {
                deltaScale = std::clamp(std::stof(std::string{data[startDataIdx].substr(6)}), 0.1F, 10.F);
                startDataIdx++;
                continue;
            } catch (...) { return std::format("Invalid delta scale: {}", std::string{data[startDataIdx].substr(6)}); }
        }

        break;
    }

    std::expected<void, std::string> result;

    if (data[startDataIdx] == "dispatcher") {
        auto dispatcherArgsIt = value.begin();
        for (int i = 0; i < startDataIdx + 2 && dispatcherArgsIt < value.end(); ++i) {
            dispatcherArgsIt = std::find(dispatcherArgsIt, value.end(), ',') + 1;
        }
        result = g_pTrackpadGestures->addGesture(makeUnique<CDispatcherTrackpadGesture>(std::string{data[startDataIdx + 1]}, std::string(dispatcherArgsIt, value.end())),
                                                 fingerCount, direction, modMask, deltaScale);
    } else if (data[startDataIdx] == "workspace")
        result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "resize")
        result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "move")
        result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "special")
        result = g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "close")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "float")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "fullscreen")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "unset")
        result = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale);
    else
        return std::format("Invalid gesture: {}", data[startDataIdx]);

    if (!result)
        return result.error();

    return std::nullopt;
}

const std::vector<SConfigOptionDescription>& CConfigManager::getAllDescriptions() {
    return CONFIG_OPTIONS;
}

bool CConfigManager::shouldUseSoftwareCursors(PHLMONITOR pMonitor) {
    static auto PNOHW      = CConfigValue<Hyprlang::INT>("cursor:no_hardware_cursors");
    static auto PINVISIBLE = CConfigValue<Hyprlang::INT>("cursor:invisible");

    if (pMonitor->m_tearingState.activelyTearing)
        return true;

    if (*PINVISIBLE != 0)
        return true;

    switch (*PNOHW) {
        case 0: return false;
        case 1: return true;
        case 2: return g_pHyprRenderer->isNvidia() && g_pHyprRenderer->isMgpu();
        default: break;
    }

    return true;
}

std::string SConfigOptionDescription::jsonify() const {
    auto parseData = [this]() -> std::string {
        return std::visit(
            [this](auto&& val) {
                const auto PTR = g_pConfigManager->m_config->getConfigValuePtr(value.c_str());
                if (!PTR) {
                    Debug::log(ERR, "invalid SConfigOptionDescription: no config option {} exists", value);
                    return std::string{""};
                }
                const char* const EXPLICIT = PTR->m_bSetByUser ? "true" : "false";

                std::string       currentValue = "undefined";

                const auto        CONFIGVALUE = PTR->getValue();

                if (typeid(Hyprlang::INT) == std::type_index(CONFIGVALUE.type()))
                    currentValue = std::format("{}", std::any_cast<Hyprlang::INT>(CONFIGVALUE));
                else if (typeid(Hyprlang::FLOAT) == std::type_index(CONFIGVALUE.type()))
                    currentValue = std::format("{:.2f}", std::any_cast<Hyprlang::FLOAT>(CONFIGVALUE));
                else if (typeid(Hyprlang::STRING) == std::type_index(CONFIGVALUE.type()))
                    currentValue = std::any_cast<Hyprlang::STRING>(CONFIGVALUE);
                else if (typeid(Hyprlang::VEC2) == std::type_index(CONFIGVALUE.type())) {
                    const auto V = std::any_cast<Hyprlang::VEC2>(CONFIGVALUE);
                    currentValue = std::format("{}, {}", V.x, V.y);
                } else if (typeid(void*) == std::type_index(CONFIGVALUE.type())) {
                    const auto DATA = sc<ICustomConfigValueData*>(std::any_cast<void*>(CONFIGVALUE));
                    currentValue    = DATA->toString();
                }

                try {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, SStringData>) {
                        return std::format(R"#(     "value": "{}",
        "current": "{}",
        "explicit": {})#",
                                           val.value, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SRangeData>) {
                        return std::format(R"#(     "value": {},
        "min": {},
        "max": {},
        "current": {},
        "explicit": {})#",
                                           val.value, val.min, val.max, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SFloatData>) {
                        return std::format(R"#(     "value": {},
        "min": {},
        "max": {},
        "current": {},
        "explicit": {})#",
                                           val.value, val.min, val.max, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SColorData>) {
                        return std::format(R"#(     "value": "{}",
        "current": "{}",
        "explicit": {})#",
                                           val.color.getAsHex(), currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SBoolData>) {
                        return std::format(R"#(     "value": {},
        "current": {},
        "explicit": {})#",
                                           val.value, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SChoiceData>) {
                        return std::format(R"#(     "value": "{}",
        "firstIndex": {},
        "current": {},
        "explicit": {})#",
                                           val.choices, val.firstIndex, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SVectorData>) {
                        return std::format(R"#(     "x": {},
        "y": {},
        "min_x": {},
        "min_y": {},
        "max_x": {},
        "max_y": {},
        "current": "{}",
        "explicit": {})#",
                                           val.vec.x, val.vec.y, val.min.x, val.min.y, val.max.x, val.max.y, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SGradientData>) {
                        return std::format(R"#(     "value": "{}",
        "current": "{}",
        "explicit": {})#",
                                           val.gradient, currentValue, EXPLICIT);
                    }

                } catch (std::bad_any_cast& e) { Debug::log(ERR, "Bad any_cast on value {} in descriptions", value); }
                return std::string{""};
            },
            data);
    };

    std::string json = std::format(R"#({{
    "value": "{}",
    "description": "{}",
    "type": {},
    "flags": {},
    "data": {{
        {}
    }}
}})#",
                                   value, escapeJSONStrings(description), sc<uint16_t>(type), sc<uint32_t>(flags), parseData());

    return json;
}

void CConfigManager::ensurePersistentWorkspacesPresent() {
    g_pCompositor->ensurePersistentWorkspacesPresent(m_workspaceRules);
}

void CConfigManager::storeFloatingSize(PHLWINDOW window, const Vector2D& size) {
    Debug::log(LOG, "storing floating size {}x{} for window {}::{}", size.x, size.y, window->m_initialClass, window->m_initialTitle);
    // true -> use m_initialClass and m_initialTitle
    SFloatCache id{window, true};
    m_mStoredFloatingSizes[id] = size;
}

std::optional<Vector2D> CConfigManager::getStoredFloatingSize(PHLWINDOW window) {
    // At startup, m_initialClass and m_initialTitle are undefined
    // and m_class and m_title are just "initial" ones.
    // false -> use m_class and m_title
    SFloatCache id{window, false};
    Debug::log(LOG, "Hash for window {}::{} = {}", window->m_class, window->m_title, id.hash);
    if (m_mStoredFloatingSizes.contains(id)) {
        Debug::log(LOG, "got stored size {}x{} for window {}::{}", m_mStoredFloatingSizes[id].x, m_mStoredFloatingSizes[id].y, window->m_class, window->m_title);
        return m_mStoredFloatingSizes[id];
    }
    return std::nullopt;
}
