#include "LuaConfigGradient.hpp"
#include "../../../helpers/MiscFunctions.hpp"

#include <numbers>

using namespace Config;
using namespace Config::Lua;

static std::expected<CHyprColor, std::string> parseColorString(const std::string& str) {
    auto result = configStringToInt(str);
    if (!result)
        return std::unexpected(std::format("invalid color \"{}\"", str));
    return CHyprColor(static_cast<uint64_t>(*result));
}

CLuaConfigGradient::CLuaConfigGradient(CHyprColor def) : m_data(def) {
    ;
}

SParseError CLuaConfigGradient::parse(lua_State* s) {
    // accept a single color string
    if (lua_isstring(s, -1)) {
        std::string str = lua_tostring(s, -1);
        auto        col = parseColorString(str);
        if (!col)
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = col.error()};
        m_data.reset(*col);
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (!lua_istable(s, -1))
        return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "gradient type requires a color string or a table with \"colors\" and optional \"angle\""};

    // read colors array
    lua_getfield(s, -1, "colors");
    if (!lua_istable(s, -1)) {
        lua_pop(s, 1);
        return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "gradient table must have a \"colors\" array"};
    }

    std::vector<CHyprColor> colors;
    int                     len = static_cast<int>(lua_rawlen(s, -1));
    if (len == 0) {
        lua_pop(s, 1);
        return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "gradient \"colors\" must not be empty"};
    }

    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(s, -1, i);
        if (!lua_isstring(s, -1)) {
            lua_pop(s, 2);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = std::format("gradient color at index {} must be a string", i)};
        }
        std::string str = lua_tostring(s, -1);
        lua_pop(s, 1);

        auto col = parseColorString(str);
        if (!col) {
            lua_pop(s, 1);
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = col.error()};
        }
        colors.emplace_back(*col);
    }
    lua_pop(s, 1); // pop colors table

    // read optional angle (degrees)
    float angle = 0.F;
    lua_getfield(s, -1, "angle");
    if (!lua_isnil(s, -1)) {
        if (!lua_isnumber(s, -1)) {
            lua_pop(s, 1);
            return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "gradient \"angle\" must be a number (degrees)"};
        }
        angle = static_cast<float>(lua_tonumber(s, -1) * std::numbers::pi / 180.0);
    }
    lua_pop(s, 1);

    m_data.m_colors = std::move(colors);
    m_data.m_angle  = angle;
    m_data.updateColorsOk();
    m_bSetByUser = true;

    return {.errorCode = PARSE_ERROR_OK};
}

const std::type_info* CLuaConfigGradient::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigGradient::data() {
    return dc<IComplexConfigValue*>(&m_data);
}

std::string CLuaConfigGradient::toString() {
    return m_data.toString();
}
