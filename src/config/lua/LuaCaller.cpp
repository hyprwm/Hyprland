#include "LuaCaller.hpp"
#include "../../debug/log/Logger.hpp"

#include <algorithm>
#include <format>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Config::Lua;

static constexpr const char* CALLER_KEY = "hl_lua_caller";
static constexpr const char* ERRORS_KEY = "hl_lua_callback_errors";

CLuaCaller::CLuaCaller(lua_State* L, FErrorCallback&& onError) : m_lua(L), m_onError(std::move(onError)) {
    m_running.reserve(MAX_CALL_DEPTH);

    lua_pushlightuserdata(m_lua, this);
    lua_setfield(m_lua, LUA_REGISTRYINDEX, CALLER_KEY);

    // fn -> { [message] = true }, weak so reported errors die with their function
    lua_newtable(m_lua);
    lua_newtable(m_lua);
    lua_pushliteral(m_lua, "k");
    lua_setfield(m_lua, -2, "__mode");
    lua_setmetatable(m_lua, -2);
    lua_setfield(m_lua, LUA_REGISTRYINDEX, ERRORS_KEY);
}

CLuaCaller::~CLuaCaller() {
    lua_pushnil(m_lua);
    lua_setfield(m_lua, LUA_REGISTRYINDEX, CALLER_KEY);
}

lua_State* CLuaCaller::state() const {
    return m_lua;
}

CLuaCaller* CLuaCaller::fromLuaState(lua_State* L) {
    if (!L)
        return nullptr;

    lua_getfield(L, LUA_REGISTRYINDEX, CALLER_KEY);
    auto* caller = sc<CLuaCaller*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return caller;
}

void CLuaCaller::watchdogHook(lua_State* L, lua_Debug* /*ar*/) {
    const auto* caller = fromLuaState(L);
    if (!caller || !caller->m_watchdogActive)
        return;

    if (std::chrono::steady_clock::now() <= caller->m_watchdogDeadline)
        return;

    // lua_error longjmps, so nothing with a destructor may live here
    luaL_where(L, 1);
    lua_pushliteral(L, "[Lua] execution timed out");
    if (!caller->m_watchdogContext.empty()) {
        lua_pushliteral(L, " in ");
        lua_pushlstring(L, caller->m_watchdogContext.data(), caller->m_watchdogContext.size());
        lua_concat(L, 4);
    } else
        lua_concat(L, 2);

    lua_error(L);
}

static int toStringProtected(lua_State* L) {
    luaL_tolstring(L, 1, nullptr);
    return 1;
}

std::string CLuaCaller::errorToString(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TSTRING || lua_type(L, idx) == LUA_TNUMBER)
        return lua_tostring(L, idx);

    // __tostring is user code, so it runs protected and under the watchdog
    idx = lua_absindex(L, idx);
    lua_pushcfunction(L, toStringProtected);
    lua_pushvalue(L, idx);

    std::string result;
    if (guardedPCall(L, 1, 1, 0, LUA_TIMEOUT_EVENT_CALLBACK_MS, "__tostring") == LUA_OK && lua_type(L, -1) == LUA_TSTRING)
        result = lua_tostring(L, -1);
    else
        result = std::format("(error object is a {} value)", luaL_typename(L, idx));

    lua_pop(L, 1);
    return result;
}

int CLuaCaller::guardedPCall(lua_State* L, int nargs, int nresults, int errfunc, int timeoutMs, std::string_view context) {
    const auto PREV_HOOK     = lua_gethook(L);
    const int  PREV_MASK     = lua_gethookmask(L);
    const int  PREV_COUNT    = lua_gethookcount(L);
    const bool PREV_ACTIVE   = m_watchdogActive;
    const auto PREV_DEADLINE = m_watchdogDeadline;
    const auto PREV_CONTEXT  = m_watchdogContext;

    m_watchdogActive   = timeoutMs > 0;
    m_watchdogDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(timeoutMs, 1));
    m_watchdogContext  = context;

    lua_sethook(L, &CLuaCaller::watchdogHook, LUA_MASKCOUNT, LUA_WATCHDOG_INSTRUCTION_INTERVAL);
    const int RESULT = lua_pcall(L, nargs, nresults, errfunc);

    lua_sethook(L, PREV_HOOK, PREV_MASK, PREV_COUNT);
    m_watchdogActive   = PREV_ACTIVE;
    m_watchdogDeadline = PREV_DEADLINE;
    m_watchdogContext  = PREV_CONTEXT;

    return RESULT;
}

std::expected<void, std::string> CLuaCaller::call(const CLuaRef& fn, int timeoutMs, std::string_view context, const std::function<int(lua_State*)>& pushArgs,
                                                  const std::function<void(lua_State*)>& readResult) {
    if (!fn.push())
        return std::unexpected(std::format("{}: callback belongs to an expired config state", context));

    const int NARGS = pushArgs ? pushArgs(m_lua) : 0;
    return callPushed(NARGS, timeoutMs, context, readResult);
}

std::expected<void, std::string> CLuaCaller::callPushed(int nargs, int timeoutMs, std::string_view context, const std::function<void(lua_State*)>& readResult) {
    const int  FN_IDX    = lua_gettop(m_lua) - nargs;
    const int  BASE      = FN_IDX - 1;
    const auto KEY       = lua_topointer(m_lua, FN_IDX);
    const bool RECURSIVE = std::ranges::contains(m_running, KEY);

    // not an error, the outer call keeps its result
    if (RECURSIVE || m_running.size() >= MAX_CALL_DEPTH) {
        const auto MESSAGE = std::format("{}: {}", context, RECURSIVE ? "recursive call skipped" : "callbacks nested too deep, call skipped");
        if (markReported(FN_IDX, MESSAGE))
            LOG(Log::WARN, "[Lua] {}", MESSAGE);

        lua_settop(m_lua, BASE);
        return {};
    }

    // keep the function below the call so errors can be attributed to it
    lua_pushvalue(m_lua, FN_IDX);
    lua_insert(m_lua, FN_IDX);

    m_running.push_back(KEY);
    Hyprutils::Utils::CScopeGuard x([this] { m_running.pop_back(); });

    if (guardedPCall(m_lua, nargs, readResult ? 1 : 0, 0, timeoutMs, context) != LUA_OK) {
        auto message = std::format("{}: {}", context, errorToString(m_lua, -1));

        if (markReported(FN_IDX, message)) {
            LOG(Log::ERR, "[Lua] {}", message);
            m_onError(std::string{message});
        }

        lua_settop(m_lua, BASE);
        return std::unexpected(std::move(message));
    }

    if (readResult)
        readResult(m_lua);

    lua_settop(m_lua, BASE);
    return {};
}

bool CLuaCaller::markReported(int fnIdx, const std::string& message) {
    fnIdx = lua_absindex(m_lua, fnIdx);

    lua_getfield(m_lua, LUA_REGISTRYINDEX, ERRORS_KEY);
    lua_pushvalue(m_lua, fnIdx);
    lua_rawget(m_lua, -2);

    if (lua_isnil(m_lua, -1)) {
        lua_pop(m_lua, 1);
        lua_newtable(m_lua);
        lua_pushvalue(m_lua, fnIdx);
        lua_pushvalue(m_lua, -2);
        lua_rawset(m_lua, -4);
    }

    lua_pushlstring(m_lua, message.data(), message.size());
    lua_rawget(m_lua, -2);
    const bool SEEN = lua_toboolean(m_lua, -1);
    lua_pop(m_lua, 1);

    if (!SEEN) {
        lua_pushlstring(m_lua, message.data(), message.size());
        lua_pushboolean(m_lua, true);
        lua_rawset(m_lua, -3);
    }

    lua_pop(m_lua, 2);
    return !SEEN;
}
