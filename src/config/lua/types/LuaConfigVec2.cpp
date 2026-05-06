#include "LuaConfigVec2.hpp"

#include <sstream>
#include <algorithm>

using namespace Config;
using namespace Config::Lua;

CLuaConfigVec2::CLuaConfigVec2(Config::VEC2 def, std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>>&& validator) :
    m_default(def), m_data(def), m_validator(std::move(validator)) {
    ;
}

SParseError CLuaConfigVec2::parse(lua_State* s) {
    Config::VEC2 vec = {};

    if (lua_isstring(s, -1)) {
        std::string input = lua_tostring(s, -1);
        std::ranges::replace(input, ',', ' ');

        std::istringstream in(input);
        double             x = 0.F, y = 0.F;
        std::string        tail;
        if (!(in >> x >> y) || (in >> tail))
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "vec2 string requires exactly 2 numbers (e.g. \"1 1\")"};

        vec = {x, y};
    } else {
        if (!lua_istable(s, -1))
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "vec2 type requires an array or string"};

        if (lua_rawlen(s, -1) != 2)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "vec2 type requires exactly 2 elements"};

        lua_rawgeti(s, -1, 1);
        if (!lua_isnumber(s, -1)) {
            lua_pop(s, 1);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "vec2 elements must be numbers"};
        }
        double x = lua_tonumber(s, -1);
        lua_pop(s, 1);

        lua_rawgeti(s, -1, 2);
        if (!lua_isnumber(s, -1)) {
            lua_pop(s, 1);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "vec2 elements must be numbers"};
        }
        double y = lua_tonumber(s, -1);
        lua_pop(s, 1);

        vec = {x, y};
    }

    if (m_validator.has_value()) {
        auto res = m_validator.value()(vec);
        if (!res)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = res.error()};
    }

    m_data       = vec;
    m_bSetByUser = true;

    return {.errorCode = PARSE_ERROR_OK};
}

const std::type_info* CLuaConfigVec2::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigVec2::data() {
    return &m_data;
}

std::string CLuaConfigVec2::toString() {
    return std::format("{} {}", (int)m_data.x, (int)m_data.y);
}

void CLuaConfigVec2::push(lua_State* s) {
    lua_createtable(s, 2, 2);

    lua_pushnumber(s, m_data.x);
    lua_rawseti(s, -2, 1);

    lua_pushnumber(s, m_data.y);
    lua_rawseti(s, -2, 2);

    lua_pushnumber(s, m_data.x);
    lua_setfield(s, -2, "x");

    lua_pushnumber(s, m_data.y);
    lua_setfield(s, -2, "y");
}

Config::VEC2 CLuaConfigVec2::asVec2() {
    return m_data;
}

const Config::VEC2& CLuaConfigVec2::parsed() {
    return m_data;
}

void CLuaConfigVec2::reset() {
    m_data = m_default;
}
