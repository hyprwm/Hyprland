#include "LuaConfigCorners.hpp"
#include "LuaConfigInt.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

using namespace Config;
using namespace Config::Lua;

static constexpr std::array FIELDS = {
    std::pair{"top_left", &CCornersData::m_topLeft},
    std::pair{"top_right", &CCornersData::m_topRight},
    std::pair{"bottom_right", &CCornersData::m_bottomRight},
    std::pair{"bottom_left", &CCornersData::m_bottomLeft},
};

CLuaConfigCorners::CLuaConfigCorners(Config::INTEGER def, std::optional<int64_t> min, std::optional<int64_t> max) : m_default(def), m_data(def), m_min(min), m_max(max) {
    ;
}

SParseError CLuaConfigCorners::parse(lua_State* s) {
    if (!lua_istable(s, -1)) {
        // Preserve integer options' scalar acceptance, including booleans.
        CLuaConfigInt scalar(m_default, m_min, m_max);
        const auto    error = scalar.parse(s);
        if (error.errorCode != PARSE_ERROR_OK)
            return error;

        m_data       = CCornersData(scalar.parsed());
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    CCornersData parsed;
    lua_pushnil(s);
    while (lua_next(s, -2)) {
        if (lua_type(s, -2) != LUA_TSTRING) {
            lua_pop(s, 2);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "corners requires named top_left, top_right, bottom_right, bottom_left fields, not a positional list"};
        }

        size_t                 length = 0;
        const auto             name   = lua_tolstring(s, -2, &length);
        const std::string_view field{name, length};
        const auto             entry = std::ranges::find_if(FIELDS, [field](const auto& item) { return item.first == field; });
        if (entry == FIELDS.end()) {
            const auto message = std::format("unknown corners field \"{}\"", field);
            lua_pop(s, 2);
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = message};
        }

        int        valid = 0;
        const auto value = lua_tointegerx(s, -1, &valid);
        if (!valid) {
            lua_pop(s, 2);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = std::format("corners \"{}\" must be an integer", entry->first)};
        }
        if ((m_min && value < *m_min) || (m_max && value > *m_max)) {
            lua_pop(s, 2);
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("corners \"{}\" value {} is out of range", entry->first, value)};
        }

        parsed.*(entry->second) = value;
        lua_pop(s, 1);
    }

    m_data       = parsed;
    m_bSetByUser = true;
    return {.errorCode = PARSE_ERROR_OK};
}

const std::type_info* CLuaConfigCorners::underlying() {
    return &typeid(decltype(m_data));
}

const void* CLuaConfigCorners::data() {
    return sc<IComplexConfigValue*>(&m_data);
}

std::string CLuaConfigCorners::toString() {
    return m_data.toString();
}

void CLuaConfigCorners::push(lua_State* s) {
    if (m_data.isUniform()) {
        lua_pushinteger(s, m_data.m_topLeft);
        return;
    }

    lua_createtable(s, 0, FIELDS.size());
    for (const auto& [field, member] : FIELDS) {
        lua_pushinteger(s, m_data.*member);
        lua_setfield(s, -2, field);
    }
}

void CLuaConfigCorners::reset() {
    m_data = CCornersData(m_default);
}

const CCornersData& CLuaConfigCorners::parsed() const {
    return m_data;
}
