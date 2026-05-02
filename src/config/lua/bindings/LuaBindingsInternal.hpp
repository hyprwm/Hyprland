#pragma once

#include "../LuaBindings.hpp"

#include "../ConfigManager.hpp"
#include "../types/LuaConfigValue.hpp"
#include "../types/LuaConfigBool.hpp"
#include "../types/LuaConfigFloat.hpp"
#include "../types/LuaConfigGradient.hpp"
#include "../types/LuaConfigInt.hpp"
#include "../types/LuaConfigString.hpp"
#include "../types/LuaConfigVec2.hpp"
#include "../types/LuaConfigExpressionVec2.hpp"

#include "../../../Compositor.hpp"
#include "../../../helpers/MiscFunctions.hpp"
#include "../../../helpers/Monitor.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../../managers/KeybindManager.hpp"
#include "../../shared/actions/ConfigActions.hpp"

#include <functional>
#include <optional>
#include <format>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Desktop::Rule {
    class CWindowRule;
}

namespace Config::Lua::Bindings::Internal {

    struct SWindowRuleEffectDesc {
        const char*                       name;
        std::function<ILuaConfigValue*()> factory;
        uint16_t                          effect;
    };

    using WE = Desktop::Rule::eWindowRuleEffect;

    inline const SWindowRuleEffectDesc WINDOW_RULE_EFFECT_DESCS[] = {
        {"float", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FLOAT},
        {"tile", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_TILE},
        {"fullscreen", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FULLSCREEN},
        {"maximize", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_MAXIMIZE},
        {"center", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_CENTER},
        {"pseudo", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PSEUDO},
        {"no_initial_focus", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NOINITIALFOCUS},
        {"pin", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PIN},
        {"fullscreen_state", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_FULLSCREENSTATE},
        {"move", []() -> ILuaConfigValue* { return new CLuaConfigExpressionVec2(); }, WE::WINDOW_RULE_EFFECT_MOVE},
        {"size", []() -> ILuaConfigValue* { return new CLuaConfigExpressionVec2(); }, WE::WINDOW_RULE_EFFECT_SIZE},
        {"monitor", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_MONITOR},
        {"workspace", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_WORKSPACE},
        {"group", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_GROUP},
        {"suppress_event", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_SUPPRESSEVENT},
        {"content", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_CONTENT},
        {"no_close_for", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, WE::WINDOW_RULE_EFFECT_NOCLOSEFOR},
        {"scrolling_width", []() -> ILuaConfigValue* { return new CLuaConfigFloat(0.F); }, WE::WINDOW_RULE_EFFECT_SCROLLING_WIDTH},
        {"rounding", []() -> ILuaConfigValue* { return new CLuaConfigInt(0, 0, 20); }, WE::WINDOW_RULE_EFFECT_ROUNDING},
        {"border_size", []() -> ILuaConfigValue* { return new CLuaConfigInt(0); }, WE::WINDOW_RULE_EFFECT_BORDER_SIZE},
        {"rounding_power", []() -> ILuaConfigValue* { return new CLuaConfigFloat(2.F, 1.F, 10.F); }, WE::WINDOW_RULE_EFFECT_ROUNDING_POWER},
        {"scroll_mouse", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.01F, 10.F); }, WE::WINDOW_RULE_EFFECT_SCROLL_MOUSE},
        {"scroll_touchpad", []() -> ILuaConfigValue* { return new CLuaConfigFloat(1.F, 0.01F, 10.F); }, WE::WINDOW_RULE_EFFECT_SCROLL_TOUCHPAD},
        {"animation", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_ANIMATION},
        {"idle_inhibit", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_IDLE_INHIBIT},
        {"opacity", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_OPACITY},
        {"tag", []() -> ILuaConfigValue* { return new CLuaConfigString(STRVAL_EMPTY); }, WE::WINDOW_RULE_EFFECT_TAG},
        {"max_size", []() -> ILuaConfigValue* { return new CLuaConfigExpressionVec2(); }, WE::WINDOW_RULE_EFFECT_MAX_SIZE},
        {"min_size", []() -> ILuaConfigValue* { return new CLuaConfigExpressionVec2(); }, WE::WINDOW_RULE_EFFECT_MIN_SIZE},
        {"border_color", []() -> ILuaConfigValue* { return new CLuaConfigGradient(CHyprColor(0xFF000000)); }, WE::WINDOW_RULE_EFFECT_BORDER_COLOR},
        {"persistent_size", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_PERSISTENT_SIZE},
        {"allows_input", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_ALLOWS_INPUT},
        {"dim_around", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_DIM_AROUND},
        {"decorate", []() -> ILuaConfigValue* { return new CLuaConfigBool(true); }, WE::WINDOW_RULE_EFFECT_DECORATE},
        {"focus_on_activate", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FOCUS_ON_ACTIVATE},
        {"keep_aspect_ratio", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_KEEP_ASPECT_RATIO},
        {"nearest_neighbor", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NEAREST_NEIGHBOR},
        {"no_anim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_ANIM},
        {"no_blur", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_BLUR},
        {"no_dim", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_DIM},
        {"no_focus", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_FOCUS},
        {"no_follow_mouse", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_FOLLOW_MOUSE},
        {"no_max_size", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_MAX_SIZE},
        {"no_shadow", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SHADOW},
        {"no_shortcuts_inhibit", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SHORTCUTS_INHIBIT},
        {"opaque", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_OPAQUE},
        {"force_rgbx", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_FORCE_RGBX},
        {"sync_fullscreen", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_SYNC_FULLSCREEN},
        {"immediate", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_IMMEDIATE},
        {"xray", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_XRAY},
        {"render_unfocused", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_RENDER_UNFOCUSED},
        {"no_screen_share", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_SCREEN_SHARE},
        {"no_vrr", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_NO_VRR},
        {"stay_focused", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_STAY_FOCUSED},
        {"confine_pointer", []() -> ILuaConfigValue* { return new CLuaConfigBool(false); }, WE::WINDOW_RULE_EFFECT_CONFINE_POINTER},
    };

    std::string                                        argStr(lua_State* L, int idx);
    std::optional<std::string>                         tableOptStr(lua_State* L, int idx, const char* field);
    std::optional<double>                              tableOptNum(lua_State* L, int idx, const char* field);
    std::optional<bool>                                tableOptBool(lua_State* L, int idx, const char* field);

    PHLMONITOR                                         monitorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    PHLWORKSPACE                                       workspaceFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    PHLWINDOW                                          windowFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<PHLMONITOR>                          tableOptMonitor(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<PHLWORKSPACE>                        tableOptWorkspace(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<PHLWINDOW>                           tableOptWindow(lua_State* L, int idx, const char* field, const char* fnName);

    std::optional<std::string>                         monitorSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<std::string>                         workspaceSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<std::string>                         windowSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);

    std::optional<std::string>                         tableOptMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<std::string>                         tableOptWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<std::string>                         tableOptWindowSelector(lua_State* L, int idx, const char* field, const char* fnName);

    std::string                                        requireTableFieldMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::string                                        requireTableFieldWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::string                                        requireTableFieldWindowSelector(lua_State* L, int idx, const char* field, const char* fnName);

    Math::eDirection                                   parseDirectionStr(const std::string& str);
    Config::Actions::eTogglableAction                  parseToggleStr(const std::string& str);

    std::optional<PHLWINDOW>                           windowFromUpval(lua_State* L, int idx);
    void                                               pushWindowUpval(lua_State* L, int tableIdx);
    int                                                checkResult(lua_State* L, const Config::Actions::ActionResult& r);
    int                                                pushSuccessResult(lua_State* L, const Config::Actions::SActionResult& r = {});
    int                                                pushErrorResult(lua_State* L, const Config::Actions::SActionError& e);
    void                                               reportError(lua_State* L, const Config::Actions::SActionError& e);
    PHLWORKSPACE                                       resolveWorkspaceStr(const std::string& args);
    PHLMONITOR                                         resolveMonitorStr(const std::string& args);
    std::string                                        getSourceInfo(lua_State* L, int stackLevel = 1);

    std::string                                        requireTableFieldStr(lua_State* L, int idx, const char* field, const char* fnName);
    double                                             requireTableFieldNum(lua_State* L, int idx, const char* field, const char* fnName);
    Config::Actions::eTogglableAction                  tableToggleAction(lua_State* L, int idx, const char* field = "action");

    std::expected<std::string, std::string>            ruleValueToString(lua_State* L);
    std::expected<void, std::string>                   addWindowRuleEffectFromLua(lua_State* L, const SWindowRuleEffectDesc& desc, const SP<Desktop::Rule::CWindowRule>& rule);
    std::expected<SP<Desktop::Rule::CWindowRule>, int> buildRuleFromTable(lua_State* L, int idx);

    int configError(lua_State* L, std::string s, Config::Actions::eActionErrorLevel level = Config::Actions::eActionErrorLevel::ERROR,
                    Config::Actions::eActionErrorCode code = Config::Actions::eActionErrorCode::UNKNOWN, int stackLevel = 1);
    int dispatcherError(lua_State* L, std::string s, Config::Actions::eActionErrorLevel level = Config::Actions::eActionErrorLevel::ERROR,
                        Config::Actions::eActionErrorCode code = Config::Actions::eActionErrorCode::UNKNOWN, int stackLevel = 1);

    template <typename... Args>
    int configError(lua_State* L, std::format_string<Args...> fmt, Args&&... args) {
        return configError(L, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    int dispatcherError(lua_State* L, Config::Actions::eActionErrorLevel level, Config::Actions::eActionErrorCode code, std::format_string<Args...> fmt, Args&&... args) {
        return dispatcherError(L, std::format(fmt, std::forward<Args>(args)...), level, code);
    }

    template <typename... Args>
    int configError(lua_State* L, int stackLevel, std::format_string<Args...> fmt, Args&&... args) {
        return configError(L, std::format(fmt, std::forward<Args>(args)...), Config::Actions::eActionErrorLevel::ERROR, Config::Actions::eActionErrorCode::UNKNOWN, stackLevel);
    }

    template <typename T, size_t N>
    const T* findDescByName(const T (&descs)[N], std::string_view key) {
        for (const auto& desc : descs) {
            if (key == desc.name)
                return &desc;
        }

        return nullptr;
    }

    void setFn(lua_State* L, const char* name, lua_CFunction fn);
    void setMgrFn(lua_State* L, CConfigManager* mgr, const char* name, lua_CFunction fn);
    void markDispatcherTable(lua_State* L);
    int  wrapDispatcher(lua_State* L);
    bool pushDispatcherFunction(lua_State* L, int idx);

    template <typename T>
    SParseError parseTableField(lua_State* L, int tableIdx, const char* field, T& parser) {
        lua_getfield(L, tableIdx, field);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = std::format("missing required field \"{}\"", field)};
        }

        auto err = parser.parse(L);
        lua_pop(L, 1);
        if (err.errorCode != PARSE_ERROR_OK)
            err.message = std::format("field \"{}\": {}", field, err.message);
        return err;
    }

    bool hasTableField(lua_State* L, int tableIdx, const char* field);
    void registerToplevelBindings(lua_State* L, CConfigManager* mgr);
    void registerLayoutBindings(lua_State* L, CConfigManager* mgr);
    void registerQueryBindings(lua_State* L);
    void registerNotificationBindings(lua_State* L);
    void registerConfigRuleBindings(lua_State* L, CConfigManager* mgr);
    void registerBindingsImpl(lua_State* L, CConfigManager* mgr);
    void registerDispatcherBindings(lua_State* L);
}
