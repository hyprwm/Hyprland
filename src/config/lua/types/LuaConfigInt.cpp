#include "LuaConfigInt.hpp"

using namespace Config;
using namespace Config::Lua;

CLuaConfigInt::CLuaConfigInt(Config::INTEGER def, std::optional<Config::INTEGER> min, std::optional<Config::INTEGER> max,
                             std::optional<std::unordered_map<std::string, Config::INTEGER>> map) : m_default(def), m_data(def), m_min(min), m_max(max), m_map(std::move(map)) {
    ;
}

SParseError CLuaConfigInt::parse(lua_State* s) {
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

    if (lua_isstring(s, -1) && !lua_isinteger(s, -1)) {
        if (!m_map.has_value())
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "integer type requires a bool or an integer"};

        const std::string str = lua_tostring(s, -1);
        const auto        it  = m_map->find(str);
        if (it == m_map->end()) {
            std::string keys;
            for (const auto& [k, _] : *m_map) {
                if (!keys.empty())
                    keys += ", ";
                keys += "\"" + k + "\"";
            }
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = std::format("unknown string value \"{}\", acceptable values are: {}", str, keys)};
        }

        m_data       = it->second;
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (lua_isinteger(s, -1)) {
        auto data = lua_tointeger(s, -1);

        if (m_min.has_value() && data < *m_min)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {} is less than the minimum of {}", data, *m_min)};

        if (m_max.has_value() && data > *m_max)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("value {} is more than the maximum of {}", data, *m_max)};

        m_data       = data;
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "integer type requires a bool or an integer"};
}

const std::type_info* CLuaConfigInt::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigInt::data() {
    return &m_data;
}

std::string CLuaConfigInt::toString() {
    return std::to_string(m_data);
}

void CLuaConfigInt::push(lua_State* s) {
    lua_pushinteger(s, m_data);
}

Config::INTEGER CLuaConfigInt::asInt() {
    return m_data;
}

const Config::INTEGER& CLuaConfigInt::parsed() {
    return m_data;
}

void CLuaConfigInt::reset() {
    m_data = m_default;
}
