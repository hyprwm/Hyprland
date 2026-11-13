#include "LuaConfigCssGap.hpp"

#include <cmath>

using namespace Config;
using namespace Config::Lua;

CLuaConfigCssGap::CLuaConfigCssGap(double def, std::optional<double> min, std::optional<double> max) : m_default(def), m_data(def), m_min(min), m_max(max) {
    ;
}

static std::string checkRange(double v, const std::string& field, std::optional<double> min, std::optional<double> max) {
    if (!std::isfinite(v))
        return std::format("gap \"{}\" must be finite", field);
    if (min.has_value() && v < *min)
        return std::format("gap \"{}\" value {} is less than the minimum of {}", field, v, *min);
    if (max.has_value() && v > *max)
        return std::format("gap \"{}\" value {} is more than the maximum of {}", field, v, *max);
    return {};
}

SParseError CLuaConfigCssGap::parse(lua_State* s) {
    // accept a plain number
    if (lua_isinteger(s, -1) || lua_isnumber(s, -1)) {
        double v = lua_tonumber(s, -1);
        auto   e = checkRange(v, "global", m_min, m_max);
        if (!e.empty())
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = e};
        m_data.reset(v);
        m_bSetByUser = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (!lua_istable(s, -1))
        return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "css_gap type requires a number or a table with optional \"top\", \"right\", \"bottom\", \"left\" fields"};

    const auto readField = [&](const char* name, double fallback) -> std::expected<double, SParseError> {
        lua_getfield(s, -1, name);
        double val = fallback;
        if (!lua_isnil(s, -1)) {
            if (!lua_isinteger(s, -1) && !lua_isnumber(s, -1)) {
                lua_pop(s, 1);
                return std::unexpected(SParseError{.errorCode = PARSE_ERROR_BAD_TYPE, .message = std::format("css_gap \"{}\" must be a number", name)});
            }
            val    = lua_tonumber(s, -1);
            auto e = checkRange(val, name, m_min, m_max);
            if (!e.empty()) {
                lua_pop(s, 1);
                return std::unexpected(SParseError{.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = e});
            }
        }
        lua_pop(s, 1);
        return val;
    };

    auto top = readField("top", 0);
    if (!top)
        return top.error();
    auto right = readField("right", 0);
    if (!right)
        return right.error();
    auto bottom = readField("bottom", 0);
    if (!bottom)
        return bottom.error();
    auto left = readField("left", 0);
    if (!left)
        return left.error();

    m_data       = CCssGapData(*top, *right, *bottom, *left);
    m_bSetByUser = true;
    return {.errorCode = PARSE_ERROR_OK};
}

const std::type_info* CLuaConfigCssGap::underlying() {
    return &typeid(decltype(m_data));
}

void const* CLuaConfigCssGap::data() {
    return dc<IComplexConfigValue*>(&m_data);
}

std::string CLuaConfigCssGap::toString() {
    return m_data.toString();
}

void CLuaConfigCssGap::push(lua_State* s) {
    lua_createtable(s, 0, 4);

    lua_pushnumber(s, m_data.m_top);
    lua_setfield(s, -2, "top");

    lua_pushnumber(s, m_data.m_right);
    lua_setfield(s, -2, "right");

    lua_pushnumber(s, m_data.m_bottom);
    lua_setfield(s, -2, "bottom");

    lua_pushnumber(s, m_data.m_left);
    lua_setfield(s, -2, "left");
}

const CCssGapData& CLuaConfigCssGap::parsed() {
    return m_data;
}

void CLuaConfigCssGap::reset() {
    m_data = CCssGapData(m_default);
}
