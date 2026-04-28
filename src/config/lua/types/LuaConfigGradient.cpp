#include "LuaConfigGradient.hpp"
#include "../../../helpers/MiscFunctions.hpp"

#include <numbers>

using namespace Config;
using namespace Config::Lua;

static std::expected<CHyprColor, std::string> parseColorString(const std::string& str) {
    auto result = configStringToInt(str);
    if (!result)
        return std::unexpected(std::format("invalid color \"{}\"", str));
    return CHyprColor(sc<uint64_t>(*result));
}

CLuaConfigGradient::CLuaConfigGradient(CHyprColor def) : m_default(def), m_data(def) {
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
        return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = R"(gradient type requires a color string or a table with "colors" and optional "angle")"};

    // read colors array
    lua_getfield(s, -1, "colors");
    if (!lua_istable(s, -1)) {
        lua_pop(s, 1);
        return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = "gradient table must have a \"colors\" array"};
    }

    std::vector<CHyprColor> colors;
    int                     len = sc<int>(lua_rawlen(s, -1));
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
        angle = sc<float>(lua_tonumber(s, -1) * std::numbers::pi / 180.0);
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
    std::string result;

    for (size_t i = 0; i < m_data.m_colors.size(); ++i) {
        if (i > 0)
            result += ' ';
        result += std::format("0x{:08X}", m_data.m_colors[i].getAsHex());
    }

    if (!result.empty())
        result += ' ';

    result += std::format("{}deg", sc<int>(m_data.m_angle * 180.0 / std::numbers::pi_v<float>));

    return result;
}

void CLuaConfigGradient::push(lua_State* s) {
    lua_createtable(s, 0, 2);

    lua_createtable(s, m_data.m_colors.size(), 0);
    for (size_t i = 0; i < m_data.m_colors.size(); ++i) {
        const auto col = std::format("0x{:08X}", m_data.m_colors[i].getAsHex());
        lua_pushstring(s, col.c_str());
        lua_rawseti(s, -2, i + 1);
    }
    lua_setfield(s, -2, "colors");

    lua_pushnumber(s, m_data.m_angle * 180.0 / std::numbers::pi);
    lua_setfield(s, -2, "angle");
}

const CGradientValueData& CLuaConfigGradient::parsed() {
    return m_data;
}

void CLuaConfigGradient::reset() {
    m_data = CGradientValueData(m_default);
}
