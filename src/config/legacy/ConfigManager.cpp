#include <re2/re2.h>

#include "ConfigManager.hpp"
#include "DefaultConfig.hpp"
#include "../values/ConfigValues.hpp"
#include "../shared/inotify/ConfigWatcher.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../Compositor.hpp"

#include "../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../shared/complex/ComplexDataTypes.hpp"
#include "../ConfigValue.hpp"
#include "../shared/monitor/MonitorRuleManager.hpp"
#include "../shared/workspace/WorkspaceRuleManager.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../shared/monitor/Parser.hpp"
#include "../supplementary/executor/Executor.hpp"
#include "../supplementary/jeremy/Jeremy.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../protocols/OutputManagement.hpp"
#include "../../managers/animation/AnimationManager.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../desktop/rule/Engine.hpp"
#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"
#include "../../debug/HyprCtl.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

#include "../../render/Renderer.hpp"
#include "../../hyprerror/HyprError.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/permissions/DynamicPermissionManager.hpp"
#include "../../debug/HyprNotificationOverlay.hpp"
#include "../../plugins/PluginSystem.hpp"
#include "../values/types/IntValue.hpp"
#include "../values/types/FloatValue.hpp"
#include "../values/types/BoolValue.hpp"
#include "../values/types/StringValue.hpp"
#include "../values/types/ColorValue.hpp"
#include "../values/types/Vec2Value.hpp"
#include "../values/types/CssGapValue.hpp"
#include "../values/types/FontWeightValue.hpp"
#include "../values/types/GradientValue.hpp"

#include "../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../managers/input/trackpad/gestures/DispatcherGesture.hpp"
#include "../../managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp"
#include "../../managers/input/trackpad/gestures/ResizeGesture.hpp"
#include "../../managers/input/trackpad/gestures/MoveGesture.hpp"
#include "../../managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"
#include "../../managers/input/trackpad/gestures/CloseGesture.hpp"
#include "../../managers/input/trackpad/gestures/FloatGesture.hpp"
#include "../../managers/input/trackpad/gestures/FullscreenGesture.hpp"
#include "../../managers/input/trackpad/gestures/CursorZoomGesture.hpp"

#include "../../event/EventBus.hpp"

#include "../../protocols/types/ContentType.hpp"
#include "render/types.hpp"
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
#include <ranges>
#include <unordered_set>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <filesystem>
#include <memory>
using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using namespace Config;
using namespace Config::Legacy;
using enum NContentType::eContentType;

//NOLINTNEXTLINE
extern "C" char**  environ;

WP<CConfigManager> Config::Legacy::mgr() {
    if (Config::mgr() && Config::mgr()->type() == CONFIG_LEGACY)
        return dynamicPointerCast<Legacy::CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));
    return nullptr;
}

static Hyprlang::CParseResult configHandleGradientSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new Config::CGradientValueData();

    const auto DATA = sc<Config::CGradientValueData*>(*data);

    CVarList2  varlist(std::string(V), 0, ' ');
    DATA->m_colors.clear();

    std::string parseError = "";

    for (auto const& var : varlist) {
        if (var.find("deg") != std::string::npos) {
            // last arg
            try {
                DATA->m_angle = std::stoi(std::string(var.substr(0, var.find("deg")))) * (PI / 180.0); // radians
            } catch (...) {
                Log::logger->log(Log::WARN, "Error parsing gradient {}", V);
                parseError = "Error parsing gradient " + V;
            }

            break;
        }

        if (DATA->m_colors.size() >= 10) {
            Log::logger->log(Log::WARN, "Error parsing gradient {}: max colors is 10.", V);
            parseError = "Error parsing gradient " + V + ": max colors is 10.";
            break;
        }

        try {
            const auto COL = configStringToInt(std::string(var));
            if (!COL)
                throw std::runtime_error(std::format("failed to parse {} as a color", var));
            DATA->m_colors.emplace_back(COL.value());
        } catch (std::exception& e) {
            Log::logger->log(Log::WARN, "Error parsing gradient {}", V);
            parseError = "Error parsing gradient " + V + ": " + e.what();
        }
    }

    if (DATA->m_colors.empty()) {
        Log::logger->log(Log::WARN, "Error parsing gradient {}", V);
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
        delete sc<Config::CGradientValueData*>(*data);
}

static Hyprlang::CParseResult configHandleGapSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new CCssGapData();

    const auto             DATA = sc<CCssGapData*>(*data);
    CVarList2              varlist((std::string(V)));
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

    const auto             RESULT = Config::Legacy::mgr()->handleExec(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleRawExec(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleRawExec(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleExecOnce(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleExecOnce(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleExecRawOnce(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleExecRawOnce(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleExecShutdown(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleExecShutdown(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleMonitor(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleMonitor(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleBezier(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleBezier(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleAnimation(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleAnimation(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleBind(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleBind(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleUnbind(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleUnbind(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWorkspaceRules(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleWorkspaceRules(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleSubmap(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleSubmap(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleSource(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleSource(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleEnv(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleEnv(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handlePlugin(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handlePlugin(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handlePermission(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handlePermission(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleGesture(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleGesture(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWindowrule(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleWindowrule(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleWindowrulev2(const char* c, const char* v) {
    Hyprlang::CParseResult res;
    res.setError("windowrulev2 is deprecated. Correct syntax can be found on the wiki.");
    return res;
}

static Hyprlang::CParseResult handleLayerrule(const char* c, const char* v) {
    const std::string      VALUE   = v;
    const std::string      COMMAND = c;

    const auto             RESULT = Config::Legacy::mgr()->handleLayerrule(COMMAND, VALUE);

    Hyprlang::CParseResult result;
    if (RESULT.has_value())
        result.setError(RESULT.value().c_str());
    return result;
}

static Hyprlang::CParseResult handleLayerrulev2(const char* c, const char* v) {
    Hyprlang::CParseResult res;
    res.setError("layerrulev2 doesn't exist. Correct syntax can be found on the wiki.");
    return res;
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

    m_mainConfigPath = Supplementary::Jeremy::getMainConfigPath()->path;

    m_configPaths.emplace_back(m_mainConfigPath);
    m_config = makeUnique<Hyprlang::CConfig>(m_configPaths.begin()->c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true});

    for (const auto& v : Values::CONFIG_VALUES) {
        const char* NAME = v->name();

        if (auto p = dc<Config::Values::CIntValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::INT{p->defaultVal()});
        else if (auto p = dc<Config::Values::CFloatValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::FLOAT{p->defaultVal()});
        else if (auto p = dc<Config::Values::CBoolValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::INT{p->defaultVal() ? 1 : 0});
        else if (auto p = dc<Config::Values::CStringValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::STRING{p->defaultVal().c_str()});
        else if (auto p = dc<Config::Values::CColorValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::INT{p->defaultVal()});
        else if (auto p = dc<Config::Values::CVec2Value*>(v.get()))
            registerConfigVar(NAME, Hyprlang::VEC2{p->defaultVal().x, p->defaultVal().y});
        else if (auto p = dc<Config::Values::CCssGapValue*>(v.get()))
            registerConfigVar(NAME, Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, std::to_string(p->defaultVal().m_top).c_str()});
        else if (auto p = dc<Config::Values::CFontWeightValue*>(v.get()))
            registerConfigVar(NAME,
                              Hyprlang::CConfigCustomValueType{&configHandleFontWeightSet, configHandleFontWeightDestroy, std::format("{}", p->defaultVal().m_value).c_str()});
        else if (auto p = dc<Config::Values::CGradientValue*>(v.get()))
            registerConfigVar(NAME,
                              Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy,
                                                               std::format("{:x}", (int64_t)p->defaultVal().m_colors.begin()->getAsHex()).c_str()});
        else
            RASSERT(false, "legacy cfg: bad value {}", NAME);
    }

    registerConfigVar("autogenerated", Hyprlang::INT{0});

    // devices
    m_config->addSpecialCategory("device", {"name"});
    m_config->addSpecialConfigValue("device", "sensitivity", {0.F});
    m_config->addSpecialConfigValue("device", "accel_profile", {STRVAL_EMPTY});
    m_config->addSpecialConfigValue("device", "rotation", Hyprlang::INT{0});
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
    m_config->addSpecialConfigValue("monitorv2", "sdr_eotf", {"default"});
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
    m_config->addSpecialConfigValue("monitorv2", "icc", Hyprlang::STRING{""});

    // windowrule v3
    m_config->addSpecialCategory("windowrule", {.key = "name"});
    m_config->addSpecialConfigValue("windowrule", "enable", Hyprlang::INT{1});

    // layerrule v2
    m_config->addSpecialCategory("layerrule", {.key = "name"});
    m_config->addSpecialConfigValue("layerrule", "enable", Hyprlang::INT{1});

    reloadRuleConfigs();

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
    m_config->registerHandler(&::handleWindowrule, "windowrule", {false});
    m_config->registerHandler(&::handleLayerrule, "layerrule", {false});
    m_config->registerHandler(&::handleBezier, "bezier", {false});
    m_config->registerHandler(&::handleAnimation, "animation", {false});
    m_config->registerHandler(&::handleSource, "source", {false});
    m_config->registerHandler(&::handleSubmap, "submap", {false});
    m_config->registerHandler(&::handlePlugin, "plugin", {false});
    m_config->registerHandler(&::handlePermission, "permission", {false});
    m_config->registerHandler(&::handleGesture, "gesture", {true});
    m_config->registerHandler(&::handleEnv, "env", {true});

    // windowrulev2 and layerrulev2 errors
    m_config->registerHandler(&::handleWindowrulev2, "windowrulev2", {false});
    m_config->registerHandler(&::handleLayerrulev2, "layerrulev2", {false});

    // pluginza
    m_config->addSpecialCategory("plugin", {nullptr, true});

    m_config->commence();

    resetHLConfig();

    if (!g_pCompositor->m_onlyConfigVerification) {
        Log::logger->log(
            Log::DEBUG,
            "!!!!HEY YOU, YES YOU!!!!: further logs to stdout / logfile are disabled by default. BEFORE SENDING THIS LOG, ENABLE THEM. Use debug:disable_logs = false to do so: "
            "https://wiki.hypr.land/Configuring/Variables/#debug");
    }

    if (g_pEventLoopManager && ERR.has_value())
        g_pEventLoopManager->doLater([ERR] { g_pHyprError->queueCreate(ERR.value(), CHyprColor{1.0, 0.1, 0.1, 1.0}); });
}

eConfigManagerType CConfigManager::type() {
    return CONFIG_LEGACY;
}

void CConfigManager::reloadRuleConfigs() {
    // FIXME: this should also remove old values if they are removed

    for (const auto& r : Desktop::Rule::allMatchPropStrings()) {
        m_config->addSpecialConfigValue("windowrule", ("match:" + r).c_str(), Hyprlang::STRING{""});
    }

    for (const auto& r : Desktop::Rule::windowEffects()->allEffectStrings()) {
        m_config->addSpecialConfigValue("windowrule", r.c_str(), Hyprlang::STRING{""});
    }

    for (const auto& r : Desktop::Rule::allMatchPropStrings()) {
        m_config->addSpecialConfigValue("layerrule", ("match:" + r).c_str(), Hyprlang::STRING{""});
    }

    for (const auto& r : Desktop::Rule::layerEffects()->allEffectStrings()) {
        m_config->addSpecialConfigValue("layerrule", r.c_str(), Hyprlang::STRING{""});
    }
}

std::optional<std::string> CConfigManager::verifyConfigExists() {
    auto mainConfigPath = Supplementary::Jeremy::getMainConfigPath();

    if (!mainConfigPath || !std::filesystem::exists(mainConfigPath->path))
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
            Log::logger->log(Log::DEBUG, "Config file not readable/found!");
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

static std::vector<const char*> HL_VERSION_VARS = {
    "HYPRLAND_V_0_53",
};

static void exportHlVersionVars() {
    for (const auto& v : HL_VERSION_VARS) {
        setenv(v, "1", 1);
    }
}

static void clearHlVersionVars() {
    for (const auto& v : HL_VERSION_VARS) {
        unsetenv(v);
    }
}

void CConfigManager::reload() {
    Event::bus()->m_events.config.preReload.emit();
    Config::animationTree()->reset();
    Config::workspaceRuleMgr()->clear();
    Config::monitorRuleMgr()->clear();
    resetHLConfig();

    auto oldConfigPath = m_mainConfigPath;

    m_mainConfigPath    = Supplementary::Jeremy::getMainConfigPath()->path;
    m_configCurrentPath = m_mainConfigPath;

    if (m_mainConfigPath != oldConfigPath)
        m_config->changeRootPath(m_mainConfigPath.c_str());

    exportHlVersionVars();

    const auto ERR = m_config->parse();

    clearHlVersionVars();

    const auto monitorError               = handleMonitorv2();
    const auto ruleError                  = reloadRules();
    m_lastConfigVerificationWasSuccessful = !ERR.error && !monitorError.error;
    postConfigReload(ERR.error || !monitorError.error ? ERR : monitorError);
}

std::string CConfigManager::verify() {
    Config::animationTree()->reset();
    resetHLConfig();
    m_configCurrentPath                   = Supplementary::Jeremy::getMainConfigPath()->path;
    const auto ERR                        = m_config->parse();
    m_lastConfigVerificationWasSuccessful = !ERR.error;
    if (ERR.error)
        return ERR.getError();
    return "config ok";
}

std::optional<std::string> CConfigManager::resetHLConfig() {
    g_pKeybindManager->clearKeybinds();
    g_pAnimationManager->removeAllBeziers();
    g_pAnimationManager->addBezierWithName("linear", Vector2D(0.0, 0.0), Vector2D(1.0, 1.0));
    g_pTrackpadGestures->clearGestures();

    Config::animationTree()->reset();
    m_declaredPlugins.clear();
    m_failedPluginConfigValues.clear();
    m_keywordRules.clear();

    // paths
    m_configPaths.clear();
    std::string mainConfigPath = getMainConfigPath();
    Log::logger->log(Log::DEBUG, "Using config: {}", mainConfigPath);
    m_configPaths.emplace_back(mainConfigPath);

    const auto RET = verifyConfigExists();

    reloadRuleConfigs();

    return RET;
}

std::optional<std::string> CConfigManager::handleMonitorv2(const std::string& output) {
    auto parser = Config::CMonitorRuleParser(output);
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
        try {
            // top, right, bottom, left
            parser.setReserved({std::stoi(ARGS[0]), std::stoi(ARGS[3]), std::stoi(ARGS[1]), std::stoi(ARGS[2])});
        } catch (...) { return "parse error: invalid reserved area"; }
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
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdr_eotf", output.c_str());
    if (VAL && VAL->m_bSetByUser) {
        const std::string value = std::any_cast<Hyprlang::STRING>(VAL->getValue());
        // remap legacy
        if (value == "0")
            parser.rule().m_sdrEotf = NTransferFunction::TF_AUTO;
        else if (value == "1")
            parser.rule().m_sdrEotf = NTransferFunction::TF_SRGB;
        else if (value == "2")
            parser.rule().m_sdrEotf = NTransferFunction::TF_GAMMA22;
        else
            parser.rule().m_sdrEotf = NTransferFunction::fromString(value);
    }
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdrbrightness", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_sdrBrightness = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdrsaturation", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_sdrSaturation = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "vrr", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_vrr = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "transform", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.parseTransform(std::any_cast<Hyprlang::STRING>(VAL->getValue()));

    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "supports_wide_color", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_supportsWideColor = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "supports_hdr", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_supportsHDR = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdr_min_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_sdrMinLuminance = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "sdr_max_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_sdrMaxLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());

    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "min_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_minLuminance = std::any_cast<Hyprlang::FLOAT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "max_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_maxLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());
    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "max_avg_luminance", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_maxAvgLuminance = std::any_cast<Hyprlang::INT>(VAL->getValue());

    VAL = m_config->getSpecialConfigValuePtr("monitorv2", "icc", output.c_str());
    if (VAL && VAL->m_bSetByUser)
        parser.rule().m_iccFile = std::any_cast<Hyprlang::STRING>(VAL->getValue());

    auto newrule = parser.rule();

    Config::monitorRuleMgr()->add(std::move(parser.rule()));

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

std::optional<std::string> CConfigManager::addRuleFromConfigKey(const std::string& name) {
    const auto ENABLED = m_config->getSpecialConfigValuePtr("windowrule", "enable", name.c_str());
    if (ENABLED && ENABLED->m_bSetByUser && std::any_cast<Hyprlang::INT>(ENABLED->getValue()) == 0)
        return std::nullopt;

    SP<Desktop::Rule::CWindowRule> rule = makeShared<Desktop::Rule::CWindowRule>(name);

    for (const auto& r : Desktop::Rule::allMatchPropStrings()) {
        auto VAL = m_config->getSpecialConfigValuePtr("windowrule", ("match:" + r).c_str(), name.c_str());
        if (VAL && VAL->m_bSetByUser)
            rule->registerMatch(Desktop::Rule::matchPropFromString(r).value_or(Desktop::Rule::RULE_PROP_NONE), std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    }

    for (const auto& e : Desktop::Rule::windowEffects()->allEffectStrings()) {
        auto VAL = m_config->getSpecialConfigValuePtr("windowrule", e.c_str(), name.c_str());
        if (VAL && VAL->m_bSetByUser)
            rule->addEffect(Desktop::Rule::windowEffects()->get(e).value_or(Desktop::Rule::WINDOW_RULE_EFFECT_NONE), std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    }

    Desktop::Rule::ruleEngine()->registerRule(std::move(rule));
    return std::nullopt;
}

std::optional<std::string> CConfigManager::addLayerRuleFromConfigKey(const std::string& name) {

    const auto ENABLED = m_config->getSpecialConfigValuePtr("layerrule", "enable", name.c_str());
    if (ENABLED && ENABLED->m_bSetByUser && std::any_cast<Hyprlang::INT>(ENABLED->getValue()) != 0)
        return std::nullopt;

    SP<Desktop::Rule::CLayerRule> rule = makeShared<Desktop::Rule::CLayerRule>(name);

    for (const auto& r : Desktop::Rule::allMatchPropStrings()) {
        auto VAL = m_config->getSpecialConfigValuePtr("layerrule", ("match:" + r).c_str(), name.c_str());
        if (VAL && VAL->m_bSetByUser)
            rule->registerMatch(Desktop::Rule::matchPropFromString(r).value_or(Desktop::Rule::RULE_PROP_NONE), std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    }

    for (const auto& e : Desktop::Rule::layerEffects()->allEffectStrings()) {
        auto VAL = m_config->getSpecialConfigValuePtr("layerrule", e.c_str(), name.c_str());
        if (VAL && VAL->m_bSetByUser)
            rule->addEffect(Desktop::Rule::layerEffects()->get(e).value_or(Desktop::Rule::LAYER_RULE_EFFECT_NONE), std::any_cast<Hyprlang::STRING>(VAL->getValue()));
    }

    Desktop::Rule::ruleEngine()->registerRule(std::move(rule));
    return std::nullopt;
}

Hyprlang::CParseResult CConfigManager::reloadRules() {
    Desktop::Rule::ruleEngine()->clearAllRules();

    Hyprlang::CParseResult result;
    for (const auto& name : m_config->listKeysForSpecialCategory("windowrule")) {
        const auto error = addRuleFromConfigKey(name);
        if (error.has_value())
            result.setError(error.value().c_str());
    }
    for (const auto& name : m_config->listKeysForSpecialCategory("layerrule")) {
        const auto error = addLayerRuleFromConfigKey(name);
        if (error.has_value())
            result.setError(error.value().c_str());
    }

    for (auto& rule : m_keywordRules) {
        Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>{rule});
    }

    Desktop::Rule::ruleEngine()->updateAllRules();

    return result;
}

void CConfigManager::postConfigReload(const Hyprlang::CParseResult& result) {
    Config::watcher()->update();

    for (auto const& w : g_pCompositor->m_windows) {
        w->uncacheWindowDecos();
    }

    static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
    for (auto const& m : g_pCompositor->m_monitors) {
        *(m->m_cursorZoom) = *PZOOMFACTOR;
        if (m->m_activeWorkspace)
            m->m_activeWorkspace->m_space->recalculate();
    }

    // Update the keyboard layout to the cfg'd one if this is not the first launch
    if (!m_isFirstLaunch) {
        g_pInputManager->setKeyboardLayout();
        g_pInputManager->setPointerConfigs();
        g_pInputManager->setTouchDeviceConfigs();
        g_pInputManager->setTabletConfigs();

        g_pHyprRenderer->m_reloadScreenShader = true;
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
    if (!m_isFirstLaunch) {
        // check
        Config::monitorRuleMgr()->scheduleReload();
        Config::monitorRuleMgr()->ensureMonitorStatus();
        Config::monitorRuleMgr()->ensureVRR();
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

    // manual crash
    if (std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash")) && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        g_pHyprNotificationOverlay->addNotification("Manual crash has been set up. Set debug:manual_crash back to 0 in order to crash the compositor.", CHyprColor(0), 5000,
                                                    ICON_INFO);
    } else if (m_manualCrashInitiated && !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:manual_crash"))) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    auto disableStdout = !std::any_cast<Hyprlang::INT>(m_config->getConfigValue("debug:enable_stdout_logs"));
    if (disableStdout && m_isFirstLaunch)
        Log::logger->log(Log::DEBUG, "Disabling stdout logs! Check the log for further logs.");

    for (auto const& m : g_pCompositor->m_monitors) {
        // mark blur dirty
        m->m_blurFBDirty = true;

        g_pCompositor->scheduleFrameForMonitor(m);

        // Force the compositor to fully re-render all monitors
        m->m_forceFullFrames = 2;

        // also force mirrors, as the aspect ratio could've changed
        for (auto const& mirror : m->m_mirrors)
            mirror->m_forceFullFrames = 3;
    }

    // update plugins
    handlePluginLoads();

    // update persistent workspaces
    if (!m_isFirstLaunch)
        g_pCompositor->ensurePersistentWorkspacesPresent();

    // update layouts
    Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();

    Event::bus()->m_events.config.reloaded.emit();
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::init() {

    Config::watcher()->setOnChange([this](const CConfigWatcher::SConfigWatchEvent& e) {
        Log::logger->log(Log::DEBUG, "CConfigManager: file {} modified, reloading", e.file);
        reload();
    });

    reload();

    m_isFirstLaunch = false;
}

std::string CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE) {
    const auto RET = m_config->parseDynamic(COMMAND.c_str(), VALUE.c_str());

    // invalidate layouts if they changed
    if (COMMAND == "monitor" || COMMAND.contains("gaps_") || COMMAND.starts_with("dwindle:") || COMMAND.starts_with("master:")) {
        for (auto const& m : g_pCompositor->m_monitors) {
            g_layoutManager->recalculateMonitor(m);
        }
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

SConfigOptionReply CConfigManager::getConfigValue(const std::string& val) {
    const auto VAL = m_config->getConfigValuePtr(val.c_str());
    if (!VAL)
        return {};

    return {.dataptr = VAL->getDataStaticPtr(), .type = &VAL->getValue().type()};
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

std::optional<std::string> CConfigManager::handleRawExec(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch) {
        Config::Supplementary::executor()->addExecOnce({args, false});
        return {};
    }

    Config::Supplementary::executor()->spawnRaw(args);
    return {};
}

std::optional<std::string> CConfigManager::handleExec(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch) {
        Config::Supplementary::executor()->addExecOnce({args, true});
        return {};
    }

    Config::Supplementary::executor()->spawn(args);
    return {};
}

std::optional<std::string> CConfigManager::handleExecOnce(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch)
        Config::Supplementary::executor()->addExecOnce({args, true});

    return {};
}

std::optional<std::string> CConfigManager::handleExecRawOnce(const std::string& command, const std::string& args) {
    if (m_isFirstLaunch)
        Config::Supplementary::executor()->addExecOnce({args, false});

    return {};
}

std::optional<std::string> CConfigManager::handleExecShutdown(const std::string& command, const std::string& args) {
    if (g_pCompositor->m_finalRequests) {
        Config::Supplementary::executor()->spawn(args);
        return {};
    }

    Config::Supplementary::executor()->addExecShutdown({args, true});
    return {};
}

std::optional<std::string> CConfigManager::handleMonitor(const std::string& command, const std::string& args) {
    // get the monitor config
    const auto ARGS = CVarList2(std::string(args));

    auto       parser = Config::CMonitorRuleParser(std::string(ARGS[0]));

    if (ARGS[1] == "disable" || ARGS[1] == "disabled" || ARGS[1] == "addreserved" || ARGS[1] == "transform") {
        if (ARGS[1] == "disable" || ARGS[1] == "disabled")
            parser.setDisabled();
        else if (ARGS[1] == "transform") {
            if (!parser.parseTransform(std::string(ARGS[2])))
                return parser.getError();

            const auto TRANSFORM = parser.rule().m_transform;

            // overwrite if exists
            for (const auto& r : Config::monitorRuleMgr()->all()) {
                if (r.m_name == parser.name()) {
                    auto cpy        = r;
                    cpy.m_transform = TRANSFORM;
                    Config::monitorRuleMgr()->add(std::move(cpy));
                    return {};
                }
            }

            return {};
        } else if (ARGS[1] == "addreserved") {
            std::optional<Desktop::CReservedArea> area;
            try {
                // top, right, bottom, left
                area = {std::stoi(std::string{ARGS[2]}), std::stoi(std::string{ARGS[5]}), std::stoi(std::string{ARGS[3]}), std::stoi(std::string{ARGS[4]})};
            } catch (...) { return "parse error: invalid reserved area"; }

            if (!area.has_value())
                return "parse error: bad addreserved";

            auto rule = std::ranges::find_if(Config::monitorRuleMgr()->allMut(), [n = ARGS[0]](const auto& other) { return other.m_name == n; });
            if (rule != Config::monitorRuleMgr()->allMut().end()) {
                rule->m_reservedArea = area.value();
                return {};
            }

            // fall
        } else {
            Log::logger->log(Log::ERR, "ConfigManager parseMonitor, curitem bogus???");
            return "parse error: curitem bogus";
        }

        Config::monitorRuleMgr()->add(std::move(parser.rule()));
        return {};
    }

    parser.parseMode(std::string(ARGS[1]));
    parser.parsePosition(std::string(ARGS[2]));
    parser.parseScale(std::string(ARGS[3]));

    int argno = 4;

    while (!ARGS[argno].empty()) {
        if (ARGS[argno] == "mirror") {
            parser.setMirror(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "bitdepth") {
            parser.parseBitdepth(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "cm") {
            parser.parseCM(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "sdrsaturation") {
            parser.parseSDRSaturation(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "sdrbrightness") {
            parser.parseSDRBrightness(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "transform") {
            parser.parseTransform(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "vrr") {
            parser.parseVRR(std::string(ARGS[argno + 1]));
            argno++;
        } else if (ARGS[argno] == "icc") {
            parser.parseICC(std::string(ARGS[argno + 1]));
            argno++;
        } else {
            Log::logger->log(Log::ERR, "Config error: invalid monitor syntax at \"{}\"", ARGS[argno]);
            return "invalid syntax at \"" + std::string(ARGS[argno]) + "\"";
        }

        argno++;
    }

    Config::monitorRuleMgr()->add(std::move(parser.rule()));

    return parser.getError();
}

std::optional<std::string> CConfigManager::handleBezier(const std::string& command, const std::string& args) {
    const auto  ARGS = CVarList(args);

    std::string bezierName = ARGS[0];

    if (ARGS[1].empty())
        return "too few arguments";
    else if (!isNumber(ARGS[1], true))
        return "invalid point";
    float p1x = std::stof(ARGS[1]);

    if (ARGS[2].empty())
        return "too few arguments";
    else if (!isNumber(ARGS[2], true))
        return "invalid point";
    float p1y = std::stof(ARGS[2]);

    if (ARGS[3].empty())
        return "too few arguments";
    else if (!isNumber(ARGS[3], true))
        return "invalid point";
    float p2x = std::stof(ARGS[3]);

    if (ARGS[4].empty())
        return "too few arguments";
    else if (!isNumber(ARGS[4], true))
        return "invalid point";
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

    if (!Config::animationTree()->nodeExists(ANIMNAME))
        return "no such animation";

    // This helper casts strings like "1", "true", "off", "yes"... to int.
    int64_t enabledInt = configStringToInt(ARGS[1]).value_or(0) == 1;

    // Checking that the int is 1 or 0 because the helper can return integers out of range.
    if (enabledInt != 0 && enabledInt != 1)
        return "invalid animation on/off state";

    if (!enabledInt) {
        Config::animationTree()->setConfigForNode(ANIMNAME, enabledInt, 1, "default");
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

    if (!g_pAnimationManager->bezierExists(bezierName))
        return "no such bezier";

    Config::animationTree()->setConfigForNode(ANIMNAME, enabledInt, speed, ARGS[3], ARGS[4]);

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
    bool       locked          = false;
    bool       release         = false;
    bool       repeat          = false;
    bool       mouse           = false;
    bool       nonConsuming    = false;
    bool       transparent     = false;
    bool       ignoreMods      = false;
    bool       multiKey        = false;
    bool       longPress       = false;
    bool       hasDescription  = false;
    bool       dontInhibit     = false;
    bool       click           = false;
    bool       drag            = false;
    bool       submapUniversal = false;
    bool       isPerDevice     = false;
    const auto BINDARGS        = command.substr(4);

    for (auto const& arg : BINDARGS) {
        switch (arg) {
            case 'l': locked = true; break;
            case 'r': release = true; break;
            case 'e': repeat = true; break;
            case 'm': mouse = true; break;
            case 'n': nonConsuming = true; break;
            case 't': transparent = true; break;
            case 'i': ignoreMods = true; break;
            case 's': multiKey = true; break;
            case 'o': longPress = true; break;
            case 'd': hasDescription = true; break;
            case 'p': dontInhibit = true; break;
            case 'c':
                click   = true;
                release = true;
                break;
            case 'g':
                drag    = true;
                release = true;
                break;
            case 'u': submapUniversal = true; break;
            case 'k': isPerDevice = true; break;
            default: return "bind: invalid flag";
        }
    }

    if ((longPress || release) && repeat)
        return "flags e is mutually exclusive with r and o";

    if (mouse && (repeat || release || locked))
        return "flag m is exclusive";

    if (click && drag)
        return "flags c and g are mutually exclusive";

    const int  numbArgs = (hasDescription ? 5 : 4) + sc<int>(isPerDevice);
    const auto ARGS     = CVarList(value, numbArgs);

    const int  DESCR_OFFSET  = hasDescription ? 1 : 0;
    const int  DEVICE_OFFSET = sc<int>(isPerDevice);
    if ((ARGS.size() < 3 && !mouse) || (ARGS.size() < 3 && mouse))
        return "bind: too few args";
    else if ((ARGS.size() > sc<size_t>(4) + DESCR_OFFSET + DEVICE_OFFSET && !mouse) || (ARGS.size() > sc<size_t>(3) + DESCR_OFFSET + DEVICE_OFFSET && mouse))
        return "bind: too many args";

    std::vector<xkb_keysym_t> KEYSYMS;
    std::vector<xkb_keysym_t> MODS;

    if (multiKey) {
        for (const auto& splitKey : CVarList(ARGS[1], 8, '&')) {
            KEYSYMS.emplace_back(xkb_keysym_from_name(splitKey.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
        for (const auto& splitMod : CVarList(ARGS[0], 8, '&')) {
            MODS.emplace_back(xkb_keysym_from_name(splitMod.c_str(), XKB_KEYSYM_CASE_INSENSITIVE));
        }
    }
    const auto MOD    = g_pKeybindManager->stringToModMask(ARGS[0]);
    const auto MODSTR = ARGS[0];

    const auto KEY = multiKey ? "" : ARGS[1];

    const auto DEVICEARGS = isPerDevice ? ARGS[2] : "";

    const auto DESCRIPTION = hasDescription ? ARGS[2 + DEVICE_OFFSET] : "";

    auto       HANDLER = ARGS[2 + DESCR_OFFSET + DEVICE_OFFSET];

    const auto COMMAND = mouse ? HANDLER : ARGS[3 + DESCR_OFFSET + DEVICE_OFFSET];

    if (mouse)
        HANDLER = "mouse";

    // to lower
    std::ranges::transform(HANDLER, HANDLER.begin(), ::tolower);

    const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(HANDLER);

    if (DISPATCHER == g_pKeybindManager->m_dispatchers.end()) {
        Log::logger->log(Log::ERR, "Invalid dispatcher: {}", HANDLER);
        return "Invalid dispatcher, requested \"" + HANDLER + "\" does not exist";
    }

    if (MOD == 0 && !MODSTR.empty()) {
        Log::logger->log(Log::ERR, "Invalid mod: {}", MODSTR);
        return "Invalid mod, requested mod \"" + MODSTR + "\" is not a valid mod.";
    }

    //[!]keyboard1 keyboard2 ...
    bool                            deviceInclusive = false;
    std::unordered_set<std::string> devices         = {};
    if (!DEVICEARGS.empty()) {
        deviceInclusive = DEVICEARGS[0] != '!';
        for (const auto deviceString : std::ranges::views::split(DEVICEARGS.substr(deviceInclusive ? 0 : 1), ' ')) {
            devices.emplace(std::string_view(deviceString));
        }
    }

    if ((!KEY.empty()) || multiKey) {
        SParsedKey parsedKey = parseKey(KEY);

        if (parsedKey.catchAll && m_currentSubmap.name.empty()) {
            Log::logger->log(Log::ERR, "Catchall not allowed outside of submap!");
            return "Invalid catchall, catchall keybinds are only allowed in submaps.";
        }

        g_pKeybindManager->addKeybind(SKeybind{parsedKey.key, KEYSYMS,      parsedKey.keycode, parsedKey.catchAll, MOD,      MODS,           HANDLER,
                                               COMMAND,       locked,       m_currentSubmap,   DESCRIPTION,        release,  repeat,         longPress,
                                               mouse,         nonConsuming, transparent,       ignoreMods,         multiKey, hasDescription, dontInhibit,
                                               click,         drag,         submapUniversal,   deviceInclusive,    devices});
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

std::optional<std::string> CConfigManager::handleWorkspaceRules(const std::string& command, const std::string& value) {
    // This can either be the monitor or the workspace identifier
    const auto FIRST_DELIM = value.find_first_of(',');

    auto       first_ident = trim(value.substr(0, FIRST_DELIM));

    const auto& [id, name, isAutoID] = getWorkspaceIDNameFromString(first_ident);

    auto                   rules = value.substr(FIRST_DELIM + 1);
    Config::CWorkspaceRule wsRule;
    wsRule.m_workspaceString = first_ident;
    // if (id == WORKSPACE_INVALID) {
    //     // it could be the monitor. If so, second value MUST be
    //     // the workspace.
    //     const auto WORKSPACE_DELIM = value.find_first_of(',', FIRST_DELIM + 1);
    //     auto       wsIdent         = removeBeginEndSpacesTabs(value.substr(FIRST_DELIM + 1, (WORKSPACE_DELIM - FIRST_DELIM - 1)));
    //     id                         = getWorkspaceIDFromString(wsIdent, name);
    //     if (id == WORKSPACE_INVALID) {
    //         Log::logger->log(Log::ERR, "Invalid workspace identifier found: {}", wsIdent);
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
            CVarList2 varlist(rule.substr(delim + 7), 0, ' ');
            wsRule.m_gapsIn = CCssGapData();
            try {
                wsRule.m_gapsIn->parseGapData(varlist);
            } catch (...) { return "Error parsing workspace rule gaps: {}", rule.substr(delim + 7); }
        } else if ((delim = rule.find("gapsout:")) != std::string::npos) {
            CVarList2 varlist(rule.substr(delim + 8), 0, ' ');
            wsRule.m_gapsOut = CCssGapData();
            try {
                wsRule.m_gapsOut->parseGapData(varlist);
            } catch (...) { return "Error parsing workspace rule gaps: {}", rule.substr(delim + 8); }
        } else if ((delim = rule.find("bordersize:")) != std::string::npos)
            try {
                wsRule.m_borderSize = std::stoi(rule.substr(delim + 11));
            } catch (...) { return "Error parsing workspace rule bordersize: {}", rule.substr(delim + 11); }
        else if ((delim = rule.find("border:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 7)))
            wsRule.m_noBorder = !*X;
        } else if ((delim = rule.find("shadow:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 7)))
            wsRule.m_noShadow = !*X;
        } else if ((delim = rule.find("rounding:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 9)))
            wsRule.m_noRounding = !*X;
        } else if ((delim = rule.find("decorate:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 9)))
            wsRule.m_decorate = *X;
        } else if ((delim = rule.find("monitor:")) != std::string::npos)
            wsRule.m_monitor = rule.substr(delim + 8);
        else if ((delim = rule.find("default:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 8)))
            wsRule.m_isDefault = *X;
        } else if ((delim = rule.find("persistent:")) != std::string::npos) {
            CHECK_OR_THROW(configStringToInt(rule.substr(delim + 11)))
            wsRule.m_isPersistent = *X;
        } else if ((delim = rule.find("defaultName:")) != std::string::npos)
            wsRule.m_defaultName = trim(rule.substr(delim + 12));
        else if ((delim = rule.find(ruleOnCreatedEmpty)) != std::string::npos) {
            CHECK_OR_THROW(cleanCmdForWorkspace(name, rule.substr(delim + ruleOnCreatedEmptyLen)))
            wsRule.m_onCreatedEmptyRunCmd = *X;
        } else if ((delim = rule.find("layoutopt:")) != std::string::npos) {
            std::string opt = rule.substr(delim + 10);
            if (!opt.contains(":")) {
                // invalid
                Log::logger->log(Log::ERR, "Invalid workspace rule found: {}", rule);
                return "Invalid workspace rule found: " + rule;
            }

            std::string val = opt.substr(opt.find(':') + 1);
            opt             = opt.substr(0, opt.find(':'));

            wsRule.m_layoutopts[opt] = val;
        } else if ((delim = rule.find("layout:")) != std::string::npos) {
            std::string layout = rule.substr(delim + 7);
            wsRule.m_layout    = std::move(layout);
        } else if ((delim = rule.find("animation:")) != std::string::npos) {
            std::string animationStyle = rule.substr(delim + 10);
            wsRule.m_animationStyle    = std::move(animationStyle);
        }

        return {};
    };

#undef CHECK_OR_THROW

    CVarList2 rulesList(std::string(rules), 0, ',', true);
    for (auto const& r : rulesList) {
        const auto R = assignRule(std::string(r));
        if (R.has_value())
            return R;
    }

    wsRule.m_workspaceName = name;
    wsRule.m_workspaceId   = isAutoID ? WORKSPACE_INVALID : id;

    Config::workspaceRuleMgr()->replaceOrAdd(std::move(wsRule));
    return {};
}

std::optional<std::string> CConfigManager::handleSubmap(const std::string&, const std::string& submap) {
    CVarList2 data((std::string(submap)));
    m_currentSubmap.name  = (data[0] == "reset") ? "" : data[0];
    m_currentSubmap.reset = data[1];
    return {};
}

std::optional<std::string> CConfigManager::handleSource(const std::string& command, const std::string& rawpath) {
    if (rawpath.length() < 2) {
        Log::logger->log(Log::ERR, "source= path garbage");
        return "source= path " + rawpath + " bogus!";
    }

    std::unique_ptr<glob_t, void (*)(glob_t*)> glob_buf{sc<glob_t*>(calloc(1, sizeof(glob_t))), // allocate and zero-initialize NOLINT(cppcoreguidelines-no-malloc)
                                                        [](glob_t* g) {
                                                            if (g) {
                                                                globfree(g); // free internal resources allocated by glob()
                                                                free(g);     // free the memory for the glob_t structure NOLINT(cppcoreguidelines-no-malloc)
                                                            }
                                                        }};

    if (auto r = glob(absolutePath(rawpath, m_configCurrentPath).c_str(), GLOB_TILDE, nullptr, glob_buf.get()); r != 0) {
        std::string err = std::format("source= globbing error: {}", r == GLOB_NOMATCH ? "found no match" : GLOB_ABORTED ? "read error" : "out of memory");
        Log::logger->log(Log::ERR, "{}", err);
        return err;
    }

    std::string errorsFromParsing;

    for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
        auto            value = absolutePath(glob_buf->gl_pathv[i], m_configCurrentPath);

        std::error_code ec;
        auto            file_status = std::filesystem::status(value, ec);

        if (ec) {
            Log::logger->log(Log::ERR, "source= file from glob result is inaccessible ({}): {}", ec.message(), value);
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
            Log::logger->log(Log::WARN, "source= skipping directory {}", value);
            continue;
        } else {
            Log::logger->log(Log::WARN, "source= skipping non-regular-file {}", value);
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
    CVarList2                   data((std::string(value)));

    eDynamicPermissionType      type = PERMISSION_TYPE_UNKNOWN;
    eDynamicPermissionAllowMode mode = PERMISSION_RULE_ALLOW_MODE_UNKNOWN;

    if (data[1] == "screencopy")
        type = PERMISSION_TYPE_SCREENCOPY;
    else if (data[1] == "cursorpos")
        type = PERMISSION_TYPE_CURSOR_POS;
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

    if (m_isFirstLaunch && g_pDynamicPermissionManager)
        g_pDynamicPermissionManager->addConfigPermissionRule(std::string(data[0]), type, mode);

    return {};
}

std::optional<std::string> CConfigManager::handleGesture(const std::string& command, const std::string& value) {
    CVarList2                 data((std::string(value)));

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

    int      startDataIdx   = 2;
    uint32_t modMask        = 0;
    float    deltaScale     = 1.F;
    bool     disableInhibit = false;

    for (const auto arg : command.substr(7)) {
        switch (arg) {
            case 'p': disableInhibit = true; break;
            default: return "gesture: invalid flag";
        }
    }

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

    if (data[startDataIdx] == "dispatcher")
        result = g_pTrackpadGestures->addGesture(makeUnique<CDispatcherTrackpadGesture>(std::string{data[startDataIdx + 1]}, data.join(",", startDataIdx + 2)), fingerCount,
                                                 direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "workspace")
        result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "resize")
        result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "move")
        result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "special")
        result =
            g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "close")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "float")
        result =
            g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "fullscreen")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(std::string{data[startDataIdx + 1]}), fingerCount, direction, modMask, deltaScale,
                                                 disableInhibit);
    else if (data[startDataIdx] == "cursorZoom") {
        result = g_pTrackpadGestures->addGesture(makeUnique<CCursorZoomTrackpadGesture>(std::string{data[startDataIdx + 1]}, std::string{data[startDataIdx + 2]}), fingerCount,
                                                 direction, modMask, deltaScale, disableInhibit);
    } else if (data[startDataIdx] == "unset")
        result = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, disableInhibit);
    else
        return std::format("Invalid gesture: {}", data[startDataIdx]);

    if (!result)
        return result.error();

    return std::nullopt;
}

std::optional<std::string> CConfigManager::handleWindowrule(const std::string& command, const std::string& value) {
    CVarList2                      data((std::string(value)));

    SP<Desktop::Rule::CWindowRule> rule = makeShared<Desktop::Rule::CWindowRule>();

    const auto&                    PROPS   = Desktop::Rule::allMatchPropStrings();
    const auto&                    EFFECTS = Desktop::Rule::windowEffects()->allEffectStrings();

    for (const auto& el : data) {
        // split on space, no need for a CVarList here
        size_t spacePos = el.find(' ');
        if (spacePos == std::string::npos)
            return std::format("invalid field {}: missing a value", el);

        const bool FIRST_IS_PROP = el.starts_with("match:");
        const auto FIRST         = FIRST_IS_PROP ? el.substr(6, spacePos - 6) : el.substr(0, spacePos);
        if (FIRST_IS_PROP && std::ranges::contains(PROPS, FIRST)) {
            // it's a prop
            const auto PROP = Desktop::Rule::matchPropFromString(FIRST);
            if (!PROP.has_value())
                return std::format("invalid prop {}", el);
            rule->registerMatch(*PROP, std::string{el.substr(spacePos + 1)});
        } else if (!FIRST_IS_PROP && std::ranges::contains(EFFECTS, FIRST)) {
            // it's an effect
            const auto EFFECT = Desktop::Rule::windowEffects()->get(FIRST);
            if (!EFFECT.has_value())
                return std::format("invalid effect {}", el);
            rule->addEffect(*EFFECT, std::string{el.substr(spacePos + 1)});
        } else
            return std::format("invalid field type {}", FIRST);
    }

    m_keywordRules.emplace_back(std::move(rule));
    if (g_pHyprCtl && g_pHyprCtl->m_currentRequestParams.isDynamicKeyword)
        Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>{m_keywordRules.back()});

    return std::nullopt;
}

std::optional<std::string> CConfigManager::handleLayerrule(const std::string& command, const std::string& value) {
    CVarList2                     data((std::string(value)));

    SP<Desktop::Rule::CLayerRule> rule = makeShared<Desktop::Rule::CLayerRule>();

    const auto&                   PROPS   = Desktop::Rule::allMatchPropStrings();
    const auto&                   EFFECTS = Desktop::Rule::layerEffects()->allEffectStrings();

    for (const auto& el : data) {
        // split on space, no need for a CVarList here
        size_t spacePos = el.find(' ');
        if (spacePos == std::string::npos)
            return std::format("invalid field {}: missing a value", el);

        const bool FIRST_IS_PROP = el.starts_with("match:");
        const auto FIRST         = FIRST_IS_PROP ? el.substr(6, spacePos - 6) : el.substr(0, spacePos);
        if (FIRST_IS_PROP && std::ranges::contains(PROPS, FIRST)) {
            // it's a prop
            const auto PROP = Desktop::Rule::matchPropFromString(FIRST);
            if (!PROP.has_value())
                return std::format("invalid prop {}", el);
            rule->registerMatch(*PROP, std::string{el.substr(spacePos + 1)});
        } else if (!FIRST_IS_PROP && std::ranges::contains(EFFECTS, FIRST)) {
            // it's an effect
            const auto EFFECT = Desktop::Rule::layerEffects()->get(FIRST);
            if (!EFFECT.has_value())
                return std::format("invalid effect {}", el);
            rule->addEffect(*EFFECT, std::string{el.substr(spacePos + 1)});
        } else
            return std::format("invalid field type {}", FIRST);
    }

    m_keywordRules.emplace_back(std::move(rule));

    return std::nullopt;
}

std::expected<void, std::string> CConfigManager::generateDefaultConfig(const std::filesystem::path& path, bool safeMode) {
    std::string parentPath = std::filesystem::path(path).parent_path();

    if (!parentPath.empty()) {
        std::error_code ec;
        bool            created = std::filesystem::create_directories(parentPath, ec);
        if (ec) {
            Log::logger->log(Log::ERR, "Couldn't create config home directory ({}): {}", ec.message(), parentPath);
            return std::unexpected("Config could not be generated.");
        }
        if (created)
            Log::logger->log(Log::WARN, "Creating config home directory");
    }

    Log::logger->log(Log::WARN, "No config file found; attempting to generate.");
    std::ofstream ofs;
    ofs.open(path, std::ios::trunc);

    if (!ofs.good())
        return std::unexpected("Config could not be generated.");

    if (!safeMode) {
        ofs << AUTOGENERATED_PREFIX;
        ofs << EXAMPLE_CONFIG;
    } else {
        std::string n = std::string{EXAMPLE_CONFIG};
        replaceInString(n, "\n$menu = hyprlauncher\n", "\n$menu = hyprland-run\n");
        ofs << n;
    }

    ofs.close();

    if (ofs.fail())
        return std::unexpected("Config could not be generated.");

    return {};
}

const std::vector<std::string>& CConfigManager::getConfigPaths() {
    return m_configPaths;
}

bool CConfigManager::configVerifPassed() {
    return m_lastConfigVerificationWasSuccessful;
}

std::string CConfigManager::getMainConfigPath() {
    return m_mainConfigPath;
}

std::string CConfigManager::currentConfigPath() {
    return m_configCurrentPath;
}

std::expected<void, std::string> CConfigManager::registerPluginValue(void* handle, SP<Config::Values::IValue> value) {
    const std::string NAME = value->name();

    if (!NAME.starts_with("plugin:"))
        return std::unexpected("name must start with plugin:");

    if (auto p = dc<Config::Values::CIntValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::INT{p->defaultVal()});
    else if (auto p = dc<Config::Values::CFloatValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::FLOAT{p->defaultVal()});
    else if (auto p = dc<Config::Values::CBoolValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::INT{p->defaultVal() ? 1 : 0});
    else if (auto p = dc<Config::Values::CStringValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::STRING{p->defaultVal().c_str()});
    else if (auto p = dc<Config::Values::CColorValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::INT{p->defaultVal()});
    else if (auto p = dc<Config::Values::CVec2Value*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::VEC2{p->defaultVal().x, p->defaultVal().y});
    else if (auto p = dc<Config::Values::CCssGapValue*>(value.get()))
        addPluginConfigVar(handle, NAME, Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, std::to_string(p->defaultVal().m_top).c_str()});
    else if (auto p = dc<Config::Values::CFontWeightValue*>(value.get()))
        addPluginConfigVar(handle, NAME,
                           Hyprlang::CConfigCustomValueType{&configHandleFontWeightSet, configHandleFontWeightDestroy, std::format("{}", p->defaultVal().m_value).c_str()});
    else if (auto p = dc<Config::Values::CGradientValue*>(value.get()))
        addPluginConfigVar(handle, NAME,
                           Hyprlang::CConfigCustomValueType{&configHandleGradientSet, configHandleGradientDestroy,
                                                            std::format("{:x}", (int64_t)p->defaultVal().m_colors.begin()->getAsHex()).c_str()});
    else
        return std::unexpected("unknown value type");

    return {};
}
