#include "LuaConfigFloat.hpp"

using namespace Config;
using namespace Config::Lua;

CLuaConfigFloat::CLuaConfigFloat(Config::FLOAT def, std::optional<Config::FLOAT> min, std::optional<Config::FLOAT> max) : m_default(def), m_data(def), m_min(min), m_max(max) {
    ;
}

SParseError CLuaConfigFloat::parse(lua_State* s) {
    if (lua_isboolean(s, -1)) {
        auto data = lua_toboolean(s, -1) ? 1 : 0;

        if (m_min.has_value() && data < *m_min)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE,
                    .message   = std::format("value {} (automatically casted from boolean) is less than the minimum of {}", data ? "true" : "false", *m_min)};

        if (m_max.has_value() && data > *m_max)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE,
                    .message   = std::format("value {} (automatically casted from boolean) is more than the maximum of {}", data ? "true" : "false", *m_max)};

        m_data       = data;
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (lua_isinteger(s, -1)) {
        auto data = lua_tointeger(s, -1);

        if (m_min.has_value() && data < *m_min)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {} is less than the minimum of {:.2f}", data, *m_min)};

        if (m_max.has_value() && data > *m_max)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {} is more than the maximum of {:.2f}", data, *m_max)};

        m_data       = data;
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (lua_isnumber(s, -1)) {
        auto data = lua_tonumber(s, -1);

        if (m_min.has_value() && data < *m_min)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {:.2f} is less than the minimum of {:.2f}", data, *m_min)};

        if (m_max.has_value() && data > *m_max)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {:.2f} is more than the maximum of {:.2f}", data, *m_max)};

        m_data       = data;
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "float type requires a number"};
}

const std::type_info* CLuaConfigFloat::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigFloat::data() {
    return &m_data;
}

std::string CLuaConfigFloat::toString() {
    return std::to_string(m_data);
}

void CLuaConfigFloat::push(lua_State* s) {
    lua_pushnumber(s, m_data);
}

Config::FLOAT CLuaConfigFloat::asFloat() {
    return m_data;
}

const Config::FLOAT& CLuaConfigFloat::parsed() {
    return m_data;
}

void CLuaConfigFloat::reset() {
    m_data = m_default;
}
