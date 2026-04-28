#include "LuaConfigExpressionVec2.hpp"

#include <format>
#include <utility>

using namespace Config;
using namespace Config::Lua;

CLuaConfigExpressionVec2::CLuaConfigExpressionVec2(Math::SExpressionVec2 def) : m_default(std::move(def)), m_data(m_default) {
    ;
}

static std::expected<std::string, std::string> expressionVec2ElementToString(lua_State* s, int idx) {
    if (lua_isinteger(s, idx))
        return std::to_string(lua_tointeger(s, idx));

    if (lua_isnumber(s, idx))
        return std::format("{}", lua_tonumber(s, idx));

    if (lua_isstring(s, idx))
        return std::string{lua_tostring(s, idx)};

    return std::unexpected("expression vec2 elements must be strings or numbers");
}

SParseError CLuaConfigExpressionVec2::parse(lua_State* s) {
    Math::SExpressionVec2 vec;

    if (lua_isstring(s, -1)) {
        auto parsed = Math::parseExpressionVec2(lua_tostring(s, -1));
        if (!parsed)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = parsed.error()};

        vec = std::move(*parsed);
    } else {
        if (!lua_istable(s, -1))
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "expression vec2 type requires an array or string"};

        if (lua_rawlen(s, -1) != 2)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "expression vec2 type requires exactly 2 elements"};

        lua_rawgeti(s, -1, 1);
        auto x = expressionVec2ElementToString(s, -1);
        lua_pop(s, 1);
        if (!x)
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = x.error()};

        lua_rawgeti(s, -1, 2);
        auto y = expressionVec2ElementToString(s, -1);
        lua_pop(s, 1);
        if (!y)
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = y.error()};

        if (x->empty() || y->empty())
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "expression vec2 elements must not be empty"};

        vec = {std::move(*x), std::move(*y)};
    }

    m_data       = std::move(vec);
    m_bSetByUser = true;

    return {.errorCode = PARSE_ERROR_OK};
}

const std::type_info* CLuaConfigExpressionVec2::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigExpressionVec2::data() {
    return &m_data;
}

std::string CLuaConfigExpressionVec2::toString() {
    return m_data.toString();
}

void CLuaConfigExpressionVec2::push(lua_State* s) {
    lua_createtable(s, 2, 2);

    lua_pushstring(s, m_data.x.c_str());
    lua_rawseti(s, -2, 1);

    lua_pushstring(s, m_data.y.c_str());
    lua_rawseti(s, -2, 2);

    lua_pushstring(s, m_data.x.c_str());
    lua_setfield(s, -2, "x");

    lua_pushstring(s, m_data.y.c_str());
    lua_setfield(s, -2, "y");
}

const Math::SExpressionVec2& CLuaConfigExpressionVec2::parsed() {
    return m_data;
}

void CLuaConfigExpressionVec2::reset() {
    m_data = m_default;
}
