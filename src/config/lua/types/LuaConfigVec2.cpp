#include "LuaConfigVec2.hpp"

using namespace Config;
using namespace Config::Lua;

CLuaConfigVec2::CLuaConfigVec2(Config::VEC2 def, std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>>&& validator) :
    m_data(def), m_validator(std::move(validator)) {
    ;
}

SParseError CLuaConfigVec2::parse(lua_State* s) {
    if (!lua_istable(s, -1))
        return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "vec2 type requires an array"};

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

    Config::VEC2 vec = {x, y};

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

const Config::VEC2& CLuaConfigVec2::parsed() {
    return m_data;
}
