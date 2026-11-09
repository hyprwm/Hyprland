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
#include "../../../output/Monitor.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../../keybinds/Manager.hpp"
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

    std::string                                                  argStr(lua_State* L, int idx);
    std::optional<std::string>                                   tableOptStr(lua_State* L, int idx, const char* field);
    std::optional<double>                                        tableOptNum(lua_State* L, int idx, const char* field);
    std::optional<bool>                                          tableOptBool(lua_State* L, int idx, const char* field);

    PHLMONITOR                                                   monitorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    PHLWORKSPACE                                                 workspaceFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    PHLWINDOW                                                    windowFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<PHLMONITOR>                                    tableOptMonitor(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<PHLWORKSPACE>                                  tableOptWorkspace(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<PHLWINDOW>                                     tableOptWindow(lua_State* L, int idx, const char* field, const char* fnName);

    std::optional<std::string>                                   monitorSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<std::string>                                   workspaceSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);
    std::optional<std::string>                                   windowSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName);

    std::optional<std::string>                                   tableOptMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<std::string>                                   tableOptWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::optional<std::string>                                   tableOptWindowSelector(lua_State* L, int idx, const char* field, const char* fnName);

    std::string                                                  requireTableFieldMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::string                                                  requireTableFieldWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName);
    std::string                                                  requireTableFieldWindowSelector(lua_State* L, int idx, const char* field, const char* fnName);

    Math::eDirection                                             parseDirectionStr(const std::string& str);
    Config::Actions::eTogglableAction                            parseToggleStr(const std::string& str);

    std::optional<PHLWINDOW>                                     windowFromUpval(lua_State* L, int idx);
    void                                                         pushWindowUpval(lua_State* L, int tableIdx);
    int                                                          checkResult(lua_State* L, const Config::Actions::ActionResult& r);
    int                                                          pushSuccessResult(lua_State* L, const Config::Actions::SActionResult& r = {});
    int                                                          pushErrorResult(lua_State* L, const Config::Actions::SActionError& e);
    Keybinds::SBindResult                                        bindResultFromTable(lua_State* L, int idx);
    void                                                         reportError(lua_State* L, const Config::Actions::SActionError& e);
    PHLMONITOR                                                   resolveMonitorStr(const std::string& args);
    std::string                                                  getSourceInfo(lua_State* L, int stackLevel = 1);

    std::string                                                  requireTableFieldStr(lua_State* L, int idx, const char* field, const char* fnName);
    double                                                       requireTableFieldNum(lua_State* L, int idx, const char* field, const char* fnName);
    Config::Actions::eTogglableAction                            tableToggleAction(lua_State* L, int idx, const char* field = "action");

    std::expected<SP<Config::Lua::ILuaConfigValue>, std::string> parseRuleValue(lua_State* L, SP<Config::Values::IValue> value);
    std::expected<void, std::string>                             addWindowRuleEffectFromLua(lua_State* L, Desktop::Rule::CWindowRuleEffectContainer::storageType effect,
                                                                                            const SP<Desktop::Rule::CWindowRule>& rule);
    std::expected<SP<Desktop::Rule::CWindowRule>, int>           buildRuleFromTable(lua_State* L, int idx);

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

    void                             setFn(lua_State* L, const char* name, lua_CFunction fn);
    void                             setMgrFn(lua_State* L, CConfigManager* mgr, const char* name, lua_CFunction fn);
    void                             setDispatcherFn(lua_State* L, const char* name, lua_CFunction fn, int maxArgs);
    int                              wrapDispatcher(lua_State* L);
    std::expected<void, std::string> pushDispatcherFunction(lua_State* L, int idx);

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
