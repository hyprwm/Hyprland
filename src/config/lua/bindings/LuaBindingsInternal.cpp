#include "LuaBindingsInternal.hpp"

#include "../../../desktop/rule/windowRule/WindowRule.hpp"

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;

namespace CA = Config::Actions;

namespace {
    constexpr const char* LUA_WINDOW_MT    = "HL.Window";
    constexpr const char* LUA_WORKSPACE_MT = "HL.Workspace";
    constexpr const char* LUA_MONITOR_MT   = "HL.Monitor";
}

std::string Internal::argStr(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TNUMBER)
        return std::to_string((long long)lua_tonumber(L, idx));

    size_t      n = 0;
    const char* s = luaL_checklstring(L, idx, &n);
    return {s, n};
}

std::optional<std::string> Internal::tableOptStr(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    const char* s = lua_tostring(L, -1);
    lua_pop(L, 1);
    return s ? std::optional(std::string(s)) : std::nullopt;
}

std::optional<double> Internal::tableOptNum(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1) || !lua_isnumber(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    const double v = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

std::optional<bool> Internal::tableOptBool(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    const bool v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

PHLMONITOR Internal::monitorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return nullptr;

    if (auto* ref = sc<PHLMONITORREF*>(luaL_testudata(L, idx, LUA_MONITOR_MT)); ref)
        return ref->lock();

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return g_pCompositor->getMonitorFromString(argStr(L, idx));

    Internal::configError(L, "{}: expected a monitor object or selector", fnName);
    return nullptr;
}

PHLWORKSPACE Internal::workspaceFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return nullptr;

    if (auto* ref = sc<PHLWORKSPACEREF*>(luaL_testudata(L, idx, LUA_WORKSPACE_MT)); ref) {
        auto ws = ref->lock();
        if (!ws || ws->inert())
            return nullptr;

        return ws;
    }

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return g_pCompositor->getWorkspaceByString(argStr(L, idx));

    Internal::configError(L, "{}: expected a workspace object or selector", fnName);
    return nullptr;
}

PHLWINDOW Internal::windowFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return nullptr;

    if (auto* ref = sc<PHLWINDOWREF*>(luaL_testudata(L, idx, LUA_WINDOW_MT)); ref)
        return ref->lock();

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return g_pCompositor->getWindowByRegex(argStr(L, idx));

    Internal::configError(L, "{}: expected a window object or selector", fnName);
    return nullptr;
}

std::optional<PHLMONITOR> Internal::tableOptMonitor(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto mon = monitorFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return mon;
}

std::optional<PHLWORKSPACE> Internal::tableOptWorkspace(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto ws = workspaceFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return ws;
}

std::optional<PHLWINDOW> Internal::tableOptWindow(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto window = windowFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return window;
}

std::optional<std::string> Internal::monitorSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return std::nullopt;

    if (auto* ref = sc<PHLMONITORREF*>(luaL_testudata(L, idx, LUA_MONITOR_MT)); ref) {
        const auto mon = ref->lock();
        if (!mon) {
            Internal::configError(L, "{}: monitor object is expired", fnName);
            return std::nullopt;
        }

        return mon->m_name;
    }

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return argStr(L, idx);

    Internal::configError(L, "{}: expected a monitor object or selector", fnName);
    return std::nullopt;
}

std::optional<std::string> Internal::workspaceSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return std::nullopt;

    if (auto* ref = sc<PHLWORKSPACEREF*>(luaL_testudata(L, idx, LUA_WORKSPACE_MT)); ref) {
        const auto ws = ref->lock();
        if (!ws || ws->inert()) {
            Internal::configError(L, "{}: workspace object is expired", fnName);
            return std::nullopt;
        }

        return std::to_string(ws->m_id);
    }

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return argStr(L, idx);

    Internal::configError(L, "{}: expected a workspace object or selector", fnName);
    return std::nullopt;
}

std::optional<std::string> Internal::windowSelectorFromLuaSelectorOrObject(lua_State* L, int idx, const char* fnName) {
    idx = lua_absindex(L, idx);

    if (lua_isnil(L, idx))
        return std::nullopt;

    if (auto* ref = sc<PHLWINDOWREF*>(luaL_testudata(L, idx, LUA_WINDOW_MT)); ref) {
        const auto w = ref->lock();
        if (!w) {
            Internal::configError(L, "{}: window object is expired", fnName);
            return std::nullopt;
        }

        return std::format("address:0x{:x}", reinterpret_cast<uintptr_t>(w.get()));
    }

    if (lua_isstring(L, idx) || lua_isnumber(L, idx))
        return argStr(L, idx);

    Internal::configError(L, "{}: expected a window object or selector", fnName);
    return std::nullopt;
}

std::optional<std::string> Internal::tableOptMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto selector = monitorSelectorFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return selector;
}

std::optional<std::string> Internal::tableOptWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto selector = workspaceSelectorFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return selector;
}

std::optional<std::string> Internal::tableOptWindowSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    idx = lua_absindex(L, idx);
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }

    auto selector = windowSelectorFromLuaSelectorOrObject(L, -1, fnName);
    lua_pop(L, 1);
    return selector;
}

std::string Internal::requireTableFieldMonitorSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    auto selector = tableOptMonitorSelector(L, idx, field, fnName);
    if (!selector) {
        Internal::configError(L, "{}: '{}' is required", fnName, field);
        return "";
    }

    return *selector;
}

std::string Internal::requireTableFieldWorkspaceSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    auto selector = tableOptWorkspaceSelector(L, idx, field, fnName);
    if (!selector) {
        Internal::configError(L, "{}: '{}' is required", fnName, field);
        return "";
    }

    return *selector;
}

std::string Internal::requireTableFieldWindowSelector(lua_State* L, int idx, const char* field, const char* fnName) {
    auto selector = tableOptWindowSelector(L, idx, field, fnName);
    if (!selector) {
        Internal::configError(L, "{}: '{}' is required", fnName, field);
        return "";
    }

    return *selector;
}

Math::eDirection Internal::parseDirectionStr(const std::string& str) {
    if (str == "left" || str == "l")
        return Math::DIRECTION_LEFT;
    if (str == "right" || str == "r")
        return Math::DIRECTION_RIGHT;
    if (str == "up" || str == "u" || str == "t")
        return Math::DIRECTION_UP;
    if (str == "down" || str == "d" || str == "b")
        return Math::DIRECTION_DOWN;
    return Math::DIRECTION_DEFAULT;
}

CA::eTogglableAction Internal::parseToggleStr(const std::string& str) {
    if (str.empty() || str == "toggle")
        return CA::TOGGLE_ACTION_TOGGLE;
    if (str == "enable" || str == "on")
        return CA::TOGGLE_ACTION_ENABLE;
    if (str == "disable" || str == "off")
        return CA::TOGGLE_ACTION_DISABLE;
    return CA::TOGGLE_ACTION_TOGGLE;
}

std::optional<PHLWINDOW> Internal::windowFromUpval(lua_State* L, int idx) {
    if (lua_isnil(L, lua_upvalueindex(idx)))
        return std::nullopt;

    return g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(idx)));
}

void Internal::pushWindowUpval(lua_State* L, int tableIdx) {
    if (lua_istable(L, tableIdx)) {
        auto selector = tableOptWindowSelector(L, tableIdx, "window", "window selector");
        if (!selector)
            lua_pushnil(L);
        else
            lua_pushstring(L, selector->c_str());
    } else
        lua_pushnil(L);
}

static auto logLevelForActionError(CA::eActionErrorLevel level) {
    switch (level) {
        case CA::eActionErrorLevel::SILENT: return Log::DEBUG;
        case CA::eActionErrorLevel::INFO: return Log::INFO;
        case CA::eActionErrorLevel::WARNING: return Log::WARN;
        case CA::eActionErrorLevel::ERROR: return Log::ERR;
    }

    return Log::ERR;
}

void Internal::reportError(lua_State* L, const CA::SActionError& e) {
    Log::logger->log(logLevelForActionError(e.level), "Lua {} ({}): {}", CA::toString(e.level), CA::toString(e.code), e.message);

    if (auto mgr = Config::Lua::mgr(); mgr) {
        mgr->addEvalIssue(e);

        if (e.level == CA::eActionErrorLevel::ERROR)
            mgr->addError(std::string{e.message});
    }
}

int Internal::pushSuccessResult(lua_State* L, const CA::SActionResult& r) {
    lua_newtable(L);
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "ok");
    lua_pushboolean(L, r.passEvent);
    lua_setfield(L, -2, "pass_event");
    return 1;
}

int Internal::pushErrorResult(lua_State* L, const CA::SActionError& e) {
    lua_newtable(L);
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "ok");
    lua_pushstring(L, e.message.c_str());
    lua_setfield(L, -2, "error");
    lua_pushstring(L, CA::toString(e.level));
    lua_setfield(L, -2, "level");
    lua_pushstring(L, CA::toString(e.code));
    lua_setfield(L, -2, "code");
    return 1;
}

int Internal::checkResult(lua_State* L, const CA::ActionResult& r) {
    if (!r) {
        auto error    = r.error();
        error.message = std::format("{}: {}", getSourceInfo(L), std::move(error.message));
        Internal::reportError(L, error);
        return Internal::pushErrorResult(L, error);
    }

    return Internal::pushSuccessResult(L, *r);
}

PHLWORKSPACE Internal::resolveWorkspaceStr(const std::string& args) {
    const auto& [id, name, isAutoID] = getWorkspaceIDNameFromString(args);
    if (id == WORKSPACE_INVALID)
        return nullptr;

    auto ws = g_pCompositor->getWorkspaceByID(id);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(id, PMONITOR->m_id, name, false);
    }

    return ws;
}

PHLMONITOR Internal::resolveMonitorStr(const std::string& args) {
    auto mon = g_pCompositor->getMonitorFromString(args);
    return mon;
}

std::string Internal::getSourceInfo(lua_State* L, int stackLevel) {
    lua_Debug   ar         = {};
    std::string sourceInfo = "?:?";

    if (lua_getstack(L, stackLevel, &ar) && lua_getinfo(L, "Sl", &ar)) {
        const char* src = ar.source;
        if (src && src[0] == '@')
            src++;

        sourceInfo = std::format("{}:{}", src ? src : "?", ar.currentline);
    }

    return sourceInfo;
}

std::string Internal::requireTableFieldStr(lua_State* L, int idx, const char* field, const char* fnName) {
    auto value = tableOptStr(L, idx, field);
    if (!value) {
        Internal::configError(L, "{}: '{}' is required", fnName, field);
        return "";
    }

    return *value;
}

double Internal::requireTableFieldNum(lua_State* L, int idx, const char* field, const char* fnName) {
    auto value = tableOptNum(L, idx, field);
    if (!value) {
        Internal::configError(L, "{}: '{}' is required", fnName, field);
        return 0;
    }

    return *value;
}

CA::eTogglableAction Internal::tableToggleAction(lua_State* L, int idx, const char* field) {
    if (!lua_istable(L, idx))
        return CA::TOGGLE_ACTION_TOGGLE;

    if (const auto action = tableOptStr(L, idx, field); action.has_value())
        return parseToggleStr(*action);

    return CA::TOGGLE_ACTION_TOGGLE;
}

void Internal::setMgrFn(lua_State* L, CConfigManager* mgr, const char* name, lua_CFunction fn) {
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, fn, 1);
    lua_setfield(L, -2, name);
}

int Internal::configError(lua_State* L, std::string s, CA::eActionErrorLevel level, CA::eActionErrorCode code, int stackLevel) {
    s = std::format("{}: {}", getSourceInfo(L, stackLevel), std::move(s));

    Internal::reportError(L, CA::SActionError{std::move(s), level, code});
    return 0;
}

int Internal::dispatcherError(lua_State* L, std::string s, CA::eActionErrorLevel level, CA::eActionErrorCode code, int stackLevel) {
    s = std::format("{}: {}", getSourceInfo(L, stackLevel), std::move(s));

    CA::SActionError error{std::move(s), level, code};
    Internal::reportError(L, error);
    return Internal::pushErrorResult(L, error);
}

std::expected<std::string, std::string> Internal::ruleValueToString(lua_State* L) {
    if (lua_type(L, -1) == LUA_TBOOLEAN)
        return lua_toboolean(L, -1) ? "true" : "false";

    if (lua_isinteger(L, -1))
        return std::to_string(lua_tointeger(L, -1));

    if (lua_isnumber(L, -1))
        return std::to_string(lua_tonumber(L, -1));

    if (lua_isstring(L, -1))
        return std::string(lua_tostring(L, -1));

    return std::unexpected("value must be a string, bool, or number");
}

std::expected<void, std::string> Internal::addWindowRuleEffectFromLua(lua_State* L, const SWindowRuleEffectDesc& desc, const SP<Desktop::Rule::CWindowRule>& rule) {
    auto val = UP<ILuaConfigValue>(desc.factory());
    auto err = val->parse(L);

    if (err.errorCode != PARSE_ERROR_OK) {
        const bool allowLegacyString = desc.effect == WE::WINDOW_RULE_EFFECT_BORDER_COLOR && lua_isstring(L, -1);
        if (!allowLegacyString)
            return std::unexpected(err.message);

        return rule->addEffect(desc.effect, lua_tostring(L, -1));
    }

    if (const auto expr = dc<CLuaConfigExpressionVec2*>(val.get()); expr)
        return rule->addEffect(desc.effect, expr->parsed());

    return rule->addEffect(desc.effect, val->toString());
}

std::expected<SP<Desktop::Rule::CWindowRule>, int> Internal::buildRuleFromTable(lua_State* L, int idx) {
    SP<Desktop::Rule::CWindowRule> rule;

    if (!lua_isnoneornil(L, idx)) {
        if (!lua_istable(L, idx))
            return std::unexpected(Internal::configError(L, "buildRuleFromTable: failed to build table for exec rules from argument"));

        int  optsIdx        = lua_absindex(L, idx);
        bool hasRuleEffects = false;

        rule = makeShared<Desktop::Rule::CWindowRule>();

        lua_pushnil(L);
        while (lua_next(L, optsIdx) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                lua_pop(L, 1);
                return std::unexpected(Internal::configError(L, "buildRuleFromTable: effect key must be a string"));
            }

            std::string key = lua_tostring(L, -2);

            if (key == "floating")
                key = "float";

            const auto* desc = Internal::findDescByName(Internal::WINDOW_RULE_EFFECT_DESCS, key);
            if (!desc) {
                const auto dynamicEffect = Desktop::Rule::windowEffects()->get(key);
                if (!dynamicEffect.has_value()) {
                    lua_pop(L, 1);
                    return std::unexpected(Internal::configError(L, "buildRuleFromTable: unknown effect '{}'", key));
                }

                auto val = ruleValueToString(L);
                if (!val) {
                    lua_pop(L, 1);
                    return std::unexpected(Internal::configError(L, "buildRuleFromTable: effect '{}': {}", key, val.error()));
                }

                auto res = rule->addEffect(*dynamicEffect, *val);
                if (!res) {
                    lua_pop(L, 1);
                    return std::unexpected(Internal::configError(L, "buildRuleFromTable: effect '{}': {}", key, res.error()));
                }
                hasRuleEffects = true;

                lua_pop(L, 1);
                continue;
            }

            auto res = Internal::addWindowRuleEffectFromLua(L, *desc, rule);
            if (!res) {
                lua_pop(L, 1);
                return std::unexpected(Internal::configError(L, "buildRuleFromTable: effect '{}': {}", key, res.error()));
            }

            hasRuleEffects = true;
            lua_pop(L, 1);
        }

        if (!hasRuleEffects)
            return nullptr;
    }

    return rule;
}

bool Internal::hasTableField(lua_State* L, int tableIdx, const char* field) {
    lua_getfield(L, tableIdx, field);
    if (lua_isnoneornil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    lua_pop(L, 1);
    return true;
}
