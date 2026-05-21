#include "Check.hpp"

#include <format>

using namespace Config::Lua;
using namespace Config::Lua::Bindings;

static const char* idxType(lua_State* L, int idx) {
    if (lua_isstring(L, idx))
        return "string";
    if (lua_isnoneornil(L, idx))
        return "nil";
    if (lua_isboolean(L, idx))
        return "bool";
    if (lua_isfunction(L, idx))
        return "function";
    if (lua_istable(L, idx))
        return "table";
    if (lua_isnumber(L, idx))
        return "number";
    if (lua_isinteger(L, idx))
        return "integer";
    return "?";
}

std::expected<std::string, std::string> Check::string(lua_State* L, int idx) {
    if (!lua_isstring(L, idx))
        return std::unexpected(std::format("expected string, got {}", idxType(L, idx)));

    return lua_tostring(L, idx);
}

std::expected<int64_t, std::string> Check::integer(lua_State* L, int idx) {
    if (!lua_isinteger(L, idx))
        return std::unexpected(std::format("expected integer, got {}", idxType(L, idx)));

    return lua_tointeger(L, idx);
}

std::expected<double, std::string> Check::number(lua_State* L, int idx) {
    if (!lua_isnumber(L, idx))
        return std::unexpected(std::format("expected number, got {}", idxType(L, idx)));

    return lua_tonumber(L, idx);
}

std::expected<bool, std::string> Check::boolean(lua_State* L, int idx) {
    if (!lua_isboolean(L, idx))
        return std::unexpected(std::format("expected boolean, got {}", idxType(L, idx)));

    return lua_toboolean(L, idx);
}
