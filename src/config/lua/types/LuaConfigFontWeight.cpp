#include "LuaConfigFontWeight.hpp"

#include <algorithm>
#include <map>
#include <hyprutils/string/String.hpp>

#include "../../shared/complex/ComplexDataTypes.hpp"

using namespace Config;
using namespace Config::Lua;

CLuaConfigFontWeight::CLuaConfigFontWeight(Config::INTEGER def) : m_default(def), m_data(def) {
    ;
}

SParseError CLuaConfigFontWeight::parse(lua_State* s) {
    if (lua_isinteger(s, -1) || lua_isnumber(s, -1)) {
        int64_t v = sc<int64_t>(lua_tonumber(s, -1));
        if (v < 0)
            return {.errorCode = PARSE_ERROR_OUT_OF_RANGE, .message = std::format("font weight {} must not be negative", v)};
        m_data.m_value = v;
        m_bSetByUser   = true;
        return {.errorCode = PARSE_ERROR_OK};
    }

    if (lua_isstring(s, -1)) {
        std::string str = lua_tostring(s, -1);
        std::string lc  = str;
        std::ranges::transform(str, lc.begin(), ::tolower);

        auto it = CFontWeightConfigValueData::WEIGHTS.find(lc);
        if (it != CFontWeightConfigValueData::WEIGHTS.end()) {
            m_data.m_value = it->second;
            m_bSetByUser   = true;
            return {.errorCode = PARSE_ERROR_OK};
        }

        if (Hyprutils::String::isNumber(str))
            return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = std::format("font weight \"{}\" was not found. Did you mean to put in an integer (without quotes?)", str)};

        return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = std::format("font weight \"{}\" was not found", str)};
    }

    return {.errorCode = PARSE_ERROR_BAD_TYPE, .message = "font weight type requires a number or a weight name string"};
}

const std::type_info* CLuaConfigFontWeight::underlying() {
    return &typeid(IComplexConfigValue*);
}

void const* CLuaConfigFontWeight::data() {
    return dc<IComplexConfigValue*>(&m_data);
}

std::string CLuaConfigFontWeight::toString() {
    return m_data.toString();
}

void CLuaConfigFontWeight::push(lua_State* s) {
    lua_pushinteger(s, m_data.m_value);
}

const CFontWeightConfigValueData& CLuaConfigFontWeight::parsed() {
    return m_data;
}

void CLuaConfigFontWeight::reset() {
    m_data = CFontWeightConfigValueData(m_default);
}
